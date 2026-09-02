// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_TEST_PDF_INK_TEST_HELPERS_H_
#define PDF_TEST_PDF_INK_TEST_HELPERS_H_

#include <stdint.h>

#include <iosfwd>
#include <optional>
#include <string>
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

base::DictValue CreateEditTextAnnotationMessage(int frontend_id);

base::DictValue CreateFinishTextAnnotationMessage(base::DictValue data);

base::DictValue SampleTextAttributesDict();

base::DictValue SampleTextBoxRectDict();

InkTextBoxAttributes SampleInkTextBoxAttributes();

InkTextBoxAttributes SampleInkTextBoxAttributesWithText(std::string text);

// Matches `SampleTextAttributesDict()`, `SampleTextBoxRectDict()`, and
// `SampleFinishTextAnnotationData()`.
testing::Matcher<const InkTextBoxAttributes&>
SampleInkTextBoxAttributesMatcher();

testing::Matcher<const InkTextBoxAttributes&>
SampleInkTextBoxAttributesMatcherWith(const std::string& text,
                                      PageOrientation viewport_orientation);

// Returns a serialized `pdf::mojom::InkTextInfo` blob for testing.
base::BlobStorage SampleInkTextInfoBlob(FontId typeface_id);

// Matches `SampleInkTextInfoBlob()`.
testing::Matcher<const InkTextInfo&> SampleInkTextInfoMatcher(
    FontId typeface_id);

// Matches `SampleInkTextInfoBlob()` when converted into an `InkTextLine`.
testing::Matcher<const InkTextLine&> SampleInkTextLineMatcher(
    FontId typeface_id);

base::DictValue SampleSerializedTypeface(FontId font_id,
                                         base::span<const uint8_t> font_data);

base::DictValue SampleFinishTextAnnotationData(int frontend_id,
                                               FontId font_id,
                                               int page_index,
                                               double pdf_zoom);

base::DictValue SampleFinishTextAnnotationDataWithSource(
    int frontend_id,
    FontId font_id,
    int page_index,
    double pdf_zoom,
    std::string_view source);

MATCHER_P6(InkAffineTransformEq,
           expected_m00,
           expected_m10,
           expected_m20,
           expected_m01,
           expected_m11,
           expected_m21,
           "") {
  using testing::FloatEq;
  using testing::Matches;
  return Matches(FloatEq(expected_m00))(arg.M00()) &&
         Matches(FloatEq(expected_m10))(arg.M10()) &&
         Matches(FloatEq(expected_m20))(arg.M20()) &&
         Matches(FloatEq(expected_m01))(arg.M01()) &&
         Matches(FloatEq(expected_m11))(arg.M11()) &&
         Matches(FloatEq(expected_m21))(arg.M21());
}

bool InkTextInfoEquals(const InkTextInfo& lhs, const InkTextInfo& rhs);

MATCHER_P6(InkTextInfoWithTextEq,
           font_id,
           glyphs,
           glyph_positions,
           location,
           is_horizontal,
           text,
           testing::PrintToString(InkTextInfo(font_id,
                                              glyphs,
                                              glyph_positions,
                                              location,
                                              is_horizontal,
                                              text))) {
  return InkTextInfoEquals(arg, InkTextInfo(font_id, glyphs, glyph_positions,
                                            location, is_horizontal, text));
}

MATCHER_P5(InkTextInfoEq,
           font_id,
           glyphs,
           glyph_positions,
           location,
           is_horizontal,
           testing::PrintToString(InkTextInfo(font_id,
                                              glyphs,
                                              glyph_positions,
                                              location,
                                              is_horizontal,
                                              u""))) {
  return InkTextInfoEquals(arg, InkTextInfo(font_id, glyphs, glyph_positions,
                                            location, is_horizontal, u""));
}

void PrintTo(const InkTextInfo& info, std::ostream* os);
void PrintTo(const InkTextBoxAttributes& info, std::ostream* os);

// Generate the path for test files specific to Ink.
base::FilePath GetInkTestDataFilePath(base::FilePath::StringViewType filename);

// Returns all variations of Ink tests to cover all features in development.
base::span<const InkTestVariation> GetAllInkTestVariations();

}  // namespace chrome_pdf

#endif  // PDF_TEST_PDF_INK_TEST_HELPERS_H_
