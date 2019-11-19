// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PAGE_ORIENTATION_H_
#define PDF_PAGE_ORIENTATION_H_

#include <cstdint>

namespace chrome_pdf {

// Enumeration of allowed page orientations. Assigned values permit simple
// modular arithmetic on orientations.
enum class PageOrientation : uint8_t {
  // Original orientation.
  kOriginal = 0,

  // Rotated clockwise 90 degrees.
  kClockwise90 = 1,

  // Rotated (clockwise) 180 degrees.
  kClockwise180 = 2,

  // Rotated clockwise 270 degrees (counterclockwise 90 degrees).
  kClockwise270 = 3,

  // Last enumeration value.
  kLast = kClockwise270
};

// Rotates a page orientation clockwise by one step (90 degrees).
PageOrientation RotateClockwise(PageOrientation orientation);

// Rotates a page orientation counterclockwise by one step (90 degrees).
PageOrientation RotateCounterclockwise(PageOrientation orientation);

}  // namespace chrome_pdf

#endif  // PDF_PAGE_ORIENTATION_H_
