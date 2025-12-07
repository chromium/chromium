// Copyright 2019 The Chromium Authors
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

// Returns the number of 90 degree clockwise rotation steps for `orientation`.
// The return value is appropriate for use with PDFium APIs that expect a
// rotation value.
constexpr int GetClockwiseRotationSteps(PageOrientation orientation) {
  // Could use static_cast<int>(orientation), but using an exhaustive switch
  // will trigger an error if the definition of `PageOrientation` changes.
  switch (orientation) {
    case PageOrientation::kOriginal:
      return 0;
    case PageOrientation::kClockwise90:
      return 1;
    case PageOrientation::kClockwise180:
      return 2;
    case PageOrientation::kClockwise270:
      return 3;
  }
}

// Whether the page orientation is `kClockwise90` or `kClockwise270`.
bool IsTransposedPageOrientation(PageOrientation orientation);

// Rotates a page orientation clockwise by one step (90 degrees).
PageOrientation RotateClockwise(PageOrientation orientation);

// Rotates a page orientation counterclockwise by one step (90 degrees).
PageOrientation RotateCounterclockwise(PageOrientation orientation);

}  // namespace chrome_pdf

#endif  // PDF_PAGE_ORIENTATION_H_
