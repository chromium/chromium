// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_TEST_PDF_INK_TEST_HELPERS_H_
#define PDF_TEST_PDF_INK_TEST_HELPERS_H_

#include <stdint.h>

#include <optional>
#include <string_view>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "base/values.h"
#include "pdf/pdf_ink_annotation_mode.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/ink/src/ink/geometry/affine_transform.h"
#include "third_party/ink/src/ink/strokes/input/stroke_input_batch.h"
#include "ui/gfx/geometry/point_f.h"

using SkColor = uint32_t;

namespace chrome_pdf {

// A possible configuration of Ink feature parameters.
struct InkTestVariation {
  bool use_text_annotations;
  bool use_text_highlighting;
};

enum class TestAnnotationUndoRedoMessageType {
  kUndo,
  kRedo,
};

// Optional parameters that the `setAnnotationBrushMessage` may have, depending
// on the brush type.
struct TestAnnotationBrushMessageParams {
  SkColor color;
  double size;
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

base::Value::Dict CreateSetAnnotationModeMessageForTesting(
    InkAnnotationMode mode);

base::Value::Dict CreateSetAnnotationBrushMessageForTesting(
    std::string_view type,
    const TestAnnotationBrushMessageParams* params);

base::Value::Dict CreateSetAnnotationUndoRedoMessageForTesting(
    TestAnnotationUndoRedoMessageType type);

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

// Generate the path for test files specific to Ink.
base::FilePath GetInkTestDataFilePath(base::FilePath::StringViewType filename);

// Returns all variations of Ink tests to cover all features in development.
base::span<const InkTestVariation> GetAllInkTestVariations();

// Returns all variations of Ink tests that have text highlighting enabled.
base::span<const InkTestVariation> GetInkTestVariationsWithTextHighlighting();

}  // namespace chrome_pdf

#endif  // PDF_TEST_PDF_INK_TEST_HELPERS_H_
