/*******************************************************************************
* Copyright 2017-2022 Intel Corporation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#include <assert.h>
#include <stdlib.h>
#include "bnorm/bnorm.hpp"

namespace bnorm {

check_alg_t str2check_alg(const char *str) {
    if (!strcasecmp("alg_0", str)) return ALG_0;
    if (!strcasecmp("alg_1", str)) return ALG_1;
    if (!strcasecmp("alg_2", str)) return ALG_2;
    return ALG_AUTO;
}

const char *check_alg2str(check_alg_t alg) {
    switch (alg) {
        case ALG_0: return "alg_0";
        case ALG_1: return "alg_1";
        case ALG_2: return "alg_2";
        case ALG_AUTO: return "alg_auto";
    }
    return "alg_auto";
}

flags_t str2flags(const char *str) {
    flags_t flags = NONE;
    while (str && *str) {
        if (*str == 'G') flags |= GLOB_STATS;
        if (*str == 'S') flags |= USE_SCALESHIFT;
        if (*str == 'C') flags |= USE_SCALE;
        if (*str == 'H') flags |= USE_SHIFT;
        if (*str == 'R') flags |= FUSE_NORM_RELU;
        if (*str == 'A') flags |= FUSE_NORM_ADD_RELU;
        str++;
    }
    return flags;
}

std::string flags2str(flags_t flags) {
    std::string str;
    if (flags & GLOB_STATS) str += "G";
    if (flags & USE_SCALESHIFT) str += "S";
    if (flags & USE_SCALE) str += "C";
    if (flags & USE_SHIFT) str += "H";
    if (flags & FUSE_NORM_RELU) str += "R";
    if (flags & FUSE_NORM_ADD_RELU) str += "A";
    return str;
}

int str2desc(desc_t *desc, const char *str) {
    // Canonical form: mbXicXihXiwXidXepsYnS,
    // where
    //     X is integer
    //     Y is float
    //     S is string
    // note: symbol `_` is ignored.
    // Cubic/square shapes are supported by specifying just highest dimension.

    desc_t d {0};
    d.mb = 2;
    d.eps = 1.f / 16;

    const char *s = str;
    assert(s);

    auto mstrtol = [](const char *nptr, char **endptr) {
        return strtol(nptr, endptr, 10);
    };

#define CASE_NN(prb, c, cvfunc) \
    do { \
        if (!strncmp(prb, s, strlen(prb))) { \
            ok = 1; \
            s += strlen(prb); \
            char *end_s; \
            d.c = cvfunc(s, &end_s); \
            s += (end_s - s); \
            if (d.c < 0) return FAIL; \
            /* printf("@@@debug: %s: " IFMT "\n", prb, d. c); */ \
        } \
    } while (0)
#define CASE_N(c, cvfunc) CASE_NN(#c, c, cvfunc)
    while (*s) {
        int ok = 0;
        CASE_N(mb, mstrtol);
        CASE_N(ic, mstrtol);
        CASE_N(id, mstrtol);
        CASE_N(ih, mstrtol);
        CASE_N(iw, mstrtol);
        CASE_N(eps, strtof);
        if (*s == 'n') {
            d.name = s + 1;
            break;
        }
        if (*s == '_') ++s;
        if (!ok) return FAIL;
    }
#undef CASE_NN
#undef CASE_N

    if (d.ic == 0) return FAIL;

    if (sanitize_desc(d.ndims, {d.id}, {d.ih}, {d.iw}, {1}) != OK) return FAIL;

    *desc = d;

    return OK;
}

dims_t desc_t::data_dims() const {
    dims_t data_dims {mb, ic, id, ih, iw};
    for (int d = 0; d < 5 - ndims; ++d) {
        data_dims.erase(data_dims.begin() + 2);
    }

    return data_dims;
}

std::ostream &operator<<(std::ostream &s, const desc_t &d) {
    bool print_d = true, print_h = true, print_w = true;
    print_dhw(print_d, print_h, print_w, d.ndims, {d.id}, {d.ih}, {d.iw});

    if (canonical || d.mb != 2) s << "mb" << d.mb;

    s << "ic" << d.ic;

    if (print_d) s << "id" << d.id;
    if (print_h) s << "ih" << d.ih;
    if (print_w) s << "iw" << d.iw;

    if (canonical || d.eps != 1.f / 16) s << "eps" << d.eps;

    if (!d.name.empty()) s << "n" << d.name;

    return s;
}

std::ostream &operator<<(std::ostream &s, const prb_t &prb) {
    dump_global_params(s);
    settings_t def;

    if (canonical || prb.dir != def.dir[0]) s << "--dir=" << prb.dir << " ";
    if (canonical || prb.dt != def.dt[0]) s << "--dt=" << prb.dt << " ";
    if (canonical || prb.tag != def.tag[0]) s << "--tag=" << prb.tag << " ";
    if (canonical || prb.flags != def.flags[0])
        s << "--flags=" << flags2str(prb.flags) << " ";
    if (canonical || prb.check_alg != def.check_alg)
        s << "--check-alg=" << check_alg2str(prb.check_alg) << " ";
    if (canonical || prb.inplace != def.inplace[0])
        s << "--inplace=" << bool2str(prb.inplace) << " ";
    if (canonical || prb.debug_check_ws != def.debug_check_ws)
        s << "--debug-check-ws=" << bool2str(prb.debug_check_ws) << " ";

    s << prb.attr;
    if (canonical || prb.ctx_init != def.ctx_init[0])
        s << "--ctx-init=" << prb.ctx_init << " ";
    if (canonical || prb.ctx_exe != def.ctx_exe[0])
        s << "--ctx-exe=" << prb.ctx_exe << " ";

    s << static_cast<const desc_t &>(prb);

    return s;
}

} // namespace bnorm
