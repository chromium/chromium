// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_INK_CONSTANTS_H_
#define PDF_PDF_INK_CONSTANTS_H_

namespace chrome_pdf {

// Constants used by Ink that are not associated with any particular class.

// Signature for "V2" PDF page objects. Do not change.
inline constexpr char kInkAnnotationIdentifierKeyV2[] = "GOOG:INKIsInker";

}  // namespace chrome_pdf

#endif  // PDF_PDF_INK_CONSTANTS_H_
