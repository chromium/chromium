// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_TEST_PDF_INK_TEST_HELPERS_H_
#define PDF_TEST_PDF_INK_TEST_HELPERS_H_

#include <string>

#include "base/values.h"

namespace chrome_pdf {

// Optional parameters that the `setAnnotationBrushMessage` may have, depending
// on the brush type.
struct TestAnnotationBrushMessageParams {
  int color_r;
  int color_g;
  int color_b;
};

base::Value::Dict CreateSetAnnotationModeMessageForTesting(bool enable);

base::Value::Dict CreateSetAnnotationBrushMessageForTesting(
    const std::string& type,
    double size,
    const TestAnnotationBrushMessageParams* params);

}  // namespace chrome_pdf

#endif  // PDF_TEST_PDF_INK_TEST_HELPERS_H_
