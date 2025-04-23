// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PAGE_ROTATION_H_
#define PDF_PAGE_ROTATION_H_

namespace chrome_pdf {

// Page rotations in clockwise degrees. This represents an inherent property of
// a PDF page, separate from the `PageOrientation` in the viewer.
enum class PageRotation {
  kRotate0 = 0,
  kRotate90 = 1,
  kRotate180 = 2,
  kRotate270 = 3,
};

}  // namespace chrome_pdf

#endif  // PDF_PAGE_ROTATION_H_
