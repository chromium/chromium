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
#include "pdf/pdf_ink_text.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/ink/src/ink/geometry/affine_transform.h"
#include "third_party/ink/src/ink/strokes/input/stroke_input_batch.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"

using SkColor = uint32_t;

namespace chrome_pdf {

// A possible configuration of Ink feature parameters.
// This had multiple fields at one point. Keep the struct since it will be used
// again in the future.
struct InkTestVariation {
  bool use_text_annotations;
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

base::DictValue CreateSetAnnotationModeMessageForTesting(
    InkAnnotationMode mode);

base::DictValue CreateSetAnnotationBrushMessageForTesting(
    std::string_view type,
    const TestAnnotationBrushMessageParams* params);

base::DictValue CreateSetAnnotationUndoRedoMessageForTesting(
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

MATCHER_P8(InkTextBoxAttributesEq,
           rect,
           color,
           css_font_size,
           typeface,
           alignment,
           orientation,
           is_bold,
           is_italic,
           "matches InkTextBoxAttributes") {
  return arg.rect == rect && arg.color == color &&
         arg.css_font_size == css_font_size && arg.typeface == typeface &&
         arg.alignment == alignment && arg.orientation == orientation &&
         arg.is_bold == is_bold && arg.is_italic == is_italic;
}

MATCHER_P5(InkTextInfoEq,
           font_id,
           glyphs,
           glyph_positions,
           location,
           is_horizontal,
           "matches InkTextInfo") {
  return arg.font_id == font_id && arg.glyphs == glyphs &&
         arg.glyph_positions == glyph_positions && arg.location == location &&
         arg.is_horizontal == is_horizontal;
}

// Generate the path for test files specific to Ink.
base::FilePath GetInkTestDataFilePath(base::FilePath::StringViewType filename);

// Returns all variations of Ink tests to cover all features in development.
base::span<const InkTestVariation> GetAllInkTestVariations();

}  // namespace chrome_pdf

#endif  // PDF_TEST_PDF_INK_TEST_HELPERS_H_
