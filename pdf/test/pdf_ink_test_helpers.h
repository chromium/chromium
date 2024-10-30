// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_TEST_PDF_INK_TEST_HELPERS_H_
#define PDF_TEST_PDF_INK_TEST_HELPERS_H_

#include <optional>
#include <string>

#include "base/containers/span.h"
#include "base/time/time.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/ink/src/ink/geometry/affine_transform.h"
#include "third_party/ink/src/ink/strokes/input/stroke_input_batch.h"
#include "ui/gfx/geometry/point_f.h"

namespace chrome_pdf {

// Optional parameters that the `setAnnotationBrushMessage` may have, depending
// on the brush type.
struct TestAnnotationBrushMessageParams {
  int color_r;
  int color_g;
  int color_b;
};

// Used to generate ink::StrokeInput. Many tests may need both a `position` and
// a `time` to consistently generate the same results.
struct PdfInkInputData {
  gfx::PointF position;
  base::TimeDelta time;
};

// Generates an ink::StrokeInputBatch.  Treats `inputs` as mouse inputs.
std::optional<ink::StrokeInputBatch> CreateInkInputBatch(
    base::span<const PdfInkInputData> inputs);

base::Value::Dict CreateSetAnnotationModeMessageForTesting(bool enable);

base::Value::Dict CreateSetAnnotationBrushMessageForTesting(
    const std::string& type,
    double size,
    const TestAnnotationBrushMessageParams* params);

MATCHER_P6(InkAffineTransformEq,
           expected_a,
           expected_b,
           expected_c,
           expected_d,
           expected_e,
           expected_f,
           "") {
  using testing::FloatEq;
  using testing::Matches;
  return Matches(FloatEq(expected_a))(arg.A()) &&
         Matches(FloatEq(expected_b))(arg.B()) &&
         Matches(FloatEq(expected_c))(arg.C()) &&
         Matches(FloatEq(expected_d))(arg.D()) &&
         Matches(FloatEq(expected_e))(arg.E()) &&
         Matches(FloatEq(expected_f))(arg.F());
}

}  // namespace chrome_pdf

#endif  // PDF_TEST_PDF_INK_TEST_HELPERS_H_
