// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/cl_dnn_custom_kernels.h"

namespace ml {

const char kInterpKernelSource[] =
    R"V0G0N(// Copyright (c) 2018 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma OPENCL EXTENSION cl_khr_fp16 : enable

#define VEC_SIZE 16

#define _CAT(a,b) a##b
#define CAT(a,b) _CAT(a,b)

inline void interpolate(const int N, const int C,
                        const __global INPUT0_TYPE* src, const int x1, const int y1,
                        const int IH_pad, const int IW_pad, const int IH, const int IW,
                        __global OUTPUT0_TYPE* dst, const int x2, const int y2,
                        const int OH_pad, const int OW_pad, const int OH, const int OW)
{
#if defined(ALIGN_CORNERS)
    const INPUT0_TYPE rh = (OH_pad > 1) ? (IH_pad - 1) / (INPUT0_TYPE)(OH_pad - 1) : (INPUT0_TYPE)(0.0f);
    const INPUT0_TYPE rw = (OW_pad > 1) ? (IW_pad - 1) / (INPUT0_TYPE)(OW_pad - 1) : (INPUT0_TYPE)(0.0f);
#else
    const INPUT0_TYPE rh = (OH_pad > 1) ? IH_pad / (INPUT0_TYPE)(OH_pad) : (INPUT0_TYPE)(0.0f);
    const INPUT0_TYPE rw = (OW_pad > 1) ? IW_pad / (INPUT0_TYPE)(OW_pad) : (INPUT0_TYPE)(0.0f);
#endif

    int h = get_global_id(0);
    int w = get_global_id(1);

    if (w >= OW)
        return;

    INPUT0_TYPE fh = rh * (INPUT0_TYPE)h;
    int ih0 = (int)(fh);
    int ih1 = (ih0 < IH_pad - 1) ? ih0+1 : ih0;

    INPUT0_TYPE h_lambda0 = fh - ih0;
    INPUT0_TYPE h_lambda1 = (INPUT0_TYPE)(1.0f) - h_lambda0;
    INPUT0_TYPE fw = rw * w;
    int iw0 = (int)(fw);
    int iw1 = (iw0 < IW_pad - 1) ? iw0 + 1 : iw0;

    INPUT0_TYPE w_lambda0 = fw - iw0;
    INPUT0_TYPE w_lambda1 = (INPUT0_TYPE)(1.0f) - w_lambda0;

    const __global INPUT0_TYPE* psrc00 = src + (y1 + ih0)*INPUT0_PITCHES[2] + (x1 + iw0)*INPUT0_PITCHES[3];
    const __global INPUT0_TYPE* psrc01 = src + (y1 + ih0)*INPUT0_PITCHES[2] + (x1 + iw1)*INPUT0_PITCHES[3];
    const __global INPUT0_TYPE* psrc10 = src + (y1 + ih1)*INPUT0_PITCHES[2] + (x1 + iw0)*INPUT0_PITCHES[3];
    const __global INPUT0_TYPE* psrc11 = src + (y1 + ih1)*INPUT0_PITCHES[2] + (x1 + iw1)*INPUT0_PITCHES[3];

    __global OUTPUT0_TYPE* pdst = dst + (y2 + h)*OUTPUT0_PITCHES[2] + (x2 + w)*OUTPUT0_PITCHES[3];

    for (int n = 0; n < N; n++)
    {
        int c = 0;
        __attribute__((opencl_unroll_hint))
        for (; c < C; c++)
        {
            int in_idx = n*INPUT0_PITCHES[0] + c*INPUT0_PITCHES[1];
            int out_idx = n*OUTPUT0_PITCHES[0] + c*OUTPUT0_PITCHES[1];
            pdst[out_idx] = (OUTPUT0_TYPE)(h_lambda1 * (w_lambda1 * psrc00[in_idx] + w_lambda0 * psrc01[in_idx]) +
                                           h_lambda0 * (w_lambda1 * psrc10[in_idx] + w_lambda0 * psrc11[in_idx]));
        }
    }
}

__kernel void interp(const __global INPUT0_TYPE*  input,
                           __global OUTPUT0_TYPE* output)
{
    int IB = INPUT0_DIMS[0];
    int IF = INPUT0_DIMS[1];
    int IY = INPUT0_DIMS[2];
    int IX = INPUT0_DIMS[3];

    int OY = OUTPUT0_DIMS[2];
    int OX = OUTPUT0_DIMS[3];

    int IY_pad = IY + pad_beg_ + pad_end_;
    int IX_pad = IX + pad_beg_ + pad_end_;

    interpolate(IB, IF, input + INPUT0_OFFSET, -pad_beg_, -pad_beg_, IY_pad, IX_pad, IY, IX, output + OUTPUT0_OFFSET, 0, 0, OY, OX, OY, OX);
}
)V0G0N";

const char kInterpKernelEntryPoint[] = "interp";

}  // namespace ml
