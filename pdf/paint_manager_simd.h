// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PAINT_MANAGER_SIMD_H_
#define PDF_PAINT_MANAGER_SIMD_H_

#include <stddef.h>
#include <stdint.h>

namespace chrome_pdf {

void NonPremulBlend(uint8_t* src_ptr, uint8_t* dest_ptr, size_t n_pixels);
void PremulBlend(uint8_t* src_ptr, uint8_t* dest_ptr, size_t n_pixels);

}  // namespace chrome_pdf

#endif  // PDF_PAINT_MANAGER_SIMD_H_
