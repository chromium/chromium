// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_CONVOLVER_LSX_H_
#define SKIA_EXT_CONVOLVER_LSX_H_

#include "skia/ext/convolver.h"

namespace skia {

void ConvolveVertically_LSX(const ConvolutionFilter1D::Fixed* filter_values,
                            int filter_length,
                            unsigned char* const* source_data_rows,
                            int pixel_width,
                            unsigned char* out_row,
                            bool has_alpha);
void Convolve4RowsHorizontally_LSX(const unsigned char* src_data[4],
                                   const ConvolutionFilter1D& filter,
                                   unsigned char* out_row[4]);
void ConvolveHorizontally_LSX(const unsigned char* src_data,
                              const ConvolutionFilter1D& filter,
                              unsigned char* out_row,
                              bool has_alpha);
}  // namespace skia

#endif  // SKIA_EXT_CONVOLVER_LSX_H_
