// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/handwriting/handwriting_type_converters.h"

#include <string>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/handwriting/handwriting.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_drawing_segment.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_hints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_hints_query_result.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_input_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_model_constraint.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_point.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_prediction.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_recognition_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_recognizer_query_result.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_segment.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_stroke.h"
#include "third_party/blink/renderer/modules/handwriting/handwriting_stroke.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

using handwriting::mojom::blink::HandwritingDrawingSegmentPtr;
using handwriting::mojom::blink::HandwritingHintsPtr;
using handwriting::mojom::blink::HandwritingModelConstraintPtr;
using handwriting::mojom::blink::HandwritingPointPtr;
using handwriting::mojom::blink::HandwritingPredictionPtr;
using handwriting::mojom::blink::HandwritingSegmentPtr;
using handwriting::mojom::blink::HandwritingStrokePtr;

TEST(HandwritingTypeConvertersTest, IdlHandwritingPointToMojo) {
  test::TaskEnvironment task_environment;
  auto* idl_point = blink::HandwritingPoint::Create();

  idl_point->setX(1.1);
  idl_point->setY(2.3);
  idl_point->setT(345);

  auto mojo_point = mojo::ConvertTo<HandwritingPointPtr>(idl_point);
  ASSERT_FALSE(mojo_point.is_null());
  EXPECT_NEAR(mojo_point->location.x(), 1.1, 1e-5);
  EXPECT_NEAR(mojo_point->location.y(), 2.3, 1e-5);
  ASSERT_TRUE(mojo_point->t.has_value());
  EXPECT_EQ(mojo_point->t->InMilliseconds(), 345);
}

TEST(HandwritingTypeConvertersTest, IdlHandwritingPointToMojoWithoutT) {
  test::TaskEnvironment task_environment;
  auto* idl_point = blink::HandwritingPoint::Create();

  idl_point->setX(3.1);
  idl_point->setY(4.3);

  auto mojo_point = mojo::ConvertTo<HandwritingPointPtr>(idl_point);
  ASSERT_FALSE(mojo_point.is_null());
  EXPECT_NEAR(mojo_point->location.x(), 3.1, 1e-5);
  EXPECT_NEAR(mojo_point->location.y(), 4.3, 1e-5);
  ASSERT_FALSE(mojo_point->t.has_value());
}

TEST(HandwritingTypeConvertersTest, IdlHandwritingModelConstraintToMojo) {
  test::TaskEnvironment task_environment;
  auto* idl_constraint = blink::HandwritingModelConstraint::Create();
  idl_constraint->setLanguages({"en", "zh"});

  auto mojo_constraint =
      mojo::ConvertTo<HandwritingModelConstraintPtr>(idl_constraint);
  EXPECT_FALSE(mojo_constraint.is_null());
  EXPECT_EQ(mojo_constraint->languages.size(), 2u);
  EXPECT_EQ(mojo_constraint->languages[0], "en");
  EXPECT_EQ(mojo_constraint->languages[1], "zh");
}

TEST(HandwritingTypeConvertersTest, IdlEmptyHandwritingModelConstraintToMojo) {
  test::TaskEnvironment task_environment;
  auto* idl_constraint = blink::HandwritingModelConstraint::Create();

  auto mojo_constraint =
      mojo::ConvertTo<HandwritingModelConstraintPtr>(idl_constraint);
  EXPECT_FALSE(mojo_constraint.is_null());
  EXPECT_EQ(mojo_constraint->languages.size(), 0u);
}

TEST(HandwritingTypeConvertersTest, IdlNullHandwritingModelConstraintToMojo) {
  test::TaskEnvironment task_environment;
  HandwritingModelConstraint* idl_constraint = nullptr;
  auto mojo_constraint =
      mojo::ConvertTo<HandwritingModelConstraintPtr>(idl_constraint);
  EXPECT_TRUE(mojo_constraint.is_null());
}

TEST(HandwritingTypeConvertersTest, IdlHandwritingStrokeToMojo) {
  test::TaskEnvironment task_environment;
  auto* idl_stroke = blink::HandwritingStroke::Create();
  auto* idl_point1 = blink::HandwritingPoint::Create();
  idl_point1->setX(0.1);
  idl_point1->setY(0.2);
  idl_point1->setT(123);
  auto* idl_point2 = blink::HandwritingPoint::Create();
  idl_stroke->addPoint(idl_point1);
  idl_point2->setX(1.1);
  idl_point2->setY(1.2);
  idl_stroke->addPoint(idl_point2);

  auto mojo_stroke = mojo::ConvertTo<HandwritingStrokePtr>(idl_stroke);
  ASSERT_FALSE(mojo_stroke.is_null());
  ASSERT_EQ(mojo_stroke->points.size(), 2u);
  EXPECT_NEAR(mojo_stroke->points[0]->location.x(), 0.1, 1e-5);
  EXPECT_NEAR(mojo_stroke->points[0]->location.y(), 0.2, 1e-5);
  ASSERT_TRUE(mojo_stroke->points[0]->t.has_value());
  EXPECT_EQ(mojo_stroke->points[0]->t->InMilliseconds(), 123);
  EXPECT_NEAR(mojo_stroke->points[1]->location.x(), 1.1, 1e-5);
  EXPECT_NEAR(mojo_stroke->points[1]->location.y(), 1.2, 1e-5);
  ASSERT_FALSE(mojo_stroke->points[1]->t.has_value());
}

TEST(HandwritingTypeConvertersTest, IdlHandwritingHintsToMojo) {
  test::TaskEnvironment task_environment;
  auto* idl_hints = blink::HandwritingHints::Create();
  idl_hints->setRecognitionType("recognition type");
  idl_hints->setInputType("input type");
  idl_hints->setTextContext("text context");
  idl_hints->setAlternatives(10);

  auto mojo_hints = mojo::ConvertTo<HandwritingHintsPtr>(idl_hints);
  ASSERT_FALSE(mojo_hints.is_null());
  EXPECT_EQ(mojo_hints->recognition_type, "recognition type");
  EXPECT_EQ(mojo_hints->input_type, "input type");
  ASSERT_FALSE(mojo_hints->text_context.IsNull());
  EXPECT_EQ(mojo_hints->text_context, "text context");
  EXPECT_EQ(mojo_hints->alternatives, 10u);
}

// Tests whether the default values of `HandwritingHints` can be correctly
// converted, especially for `textContext` which is not-set by default.
TEST(HandwritingTypeConvertersTest, IdlHandwritingHintsToDefaultValue) {
  test::TaskEnvironment task_environment;
  auto* idl_hints = blink::HandwritingHints::Create();

  auto mojo_hints = mojo::ConvertTo<HandwritingHintsPtr>(idl_hints);
  ASSERT_FALSE(mojo_hints.is_null());
  EXPECT_EQ(mojo_hints->recognition_type, "text");
  EXPECT_EQ(mojo_hints->input_type, "mouse");
  EXPECT_TRUE(mojo_hints->text_context.IsNull());
  EXPECT_EQ(mojo_hints->alternatives, 3u);
}

TEST(HandwritingTypeConvertersTest, MojoHandwritingPointToIdl) {
  test::TaskEnvironment task_environment;
  auto mojo_point = handwriting::mojom::blink::HandwritingPoint::New();
  mojo_point->location = gfx::PointF(0.3, 0.4);
  mojo_point->t = base::Milliseconds(123);

  auto* idl_point = mojo::ConvertTo<blink::HandwritingPoint*>(mojo_point);
  ASSERT_NE(idl_point, nullptr);
  EXPECT_NEAR(idl_point->x(), 0.3, 1e-5);
  EXPECT_NEAR(idl_point->y(), 0.4, 1e-5);
  ASSERT_TRUE(idl_point->hasT());
  EXPECT_EQ(idl_point->t(), 123u);
}

TEST(HandwritingTypeConvertersTest, MojoHandwritingPointToIdlWithoutT) {
  test::TaskEnvironment task_environment;
  auto mojo_point = handwriting::mojom::blink::HandwritingPoint::New();
  mojo_point->location = gfx::PointF(0.3, 0.4);

  auto* idl_point = mojo::ConvertTo<blink::HandwritingPoint*>(mojo_point);
  ASSERT_NE(idl_point, nullptr);
  EXPECT_NEAR(idl_point->x(), 0.3, 1e-5);
  EXPECT_NEAR(idl_point->y(), 0.4, 1e-5);
  ASSERT_FALSE(idl_point->hasT());
}

TEST(HandwritingTypeConvertersTest, MojoHandwritingStrokeToIdl) {
  test::TaskEnvironment task_environment;
  auto mojo_stroke = handwriting::mojom::blink::HandwritingStroke::New();
  auto mojo_point1 = handwriting::mojom::blink::HandwritingPoint::New();
  mojo_point1->location = gfx::PointF(2.1, 2.2);
  mojo_point1->t = base::Milliseconds(321);
  mojo_stroke->points.push_back(std::move(mojo_point1));
  auto mojo_point2 = handwriting::mojom::blink::HandwritingPoint::New();
  mojo_point2->location = gfx::PointF(3.1, 3.2);
  mojo_stroke->points.push_back(std::move(mojo_point2));

  auto* idl_stroke = mojo::ConvertTo<blink::HandwritingStroke*>(mojo_stroke);
  ASSERT_NE(idl_stroke, nullptr);
  ASSERT_EQ(idl_stroke->getPoints().size(), 2u);
  EXPECT_NEAR(idl_stroke->getPoints()[0]->x(), 2.1, 1e-5);
  EXPECT_NEAR(idl_stroke->getPoints()[0]->y(), 2.2, 1e-5);
  ASSERT_TRUE(idl_stroke->getPoints()[0]->hasT());
  EXPECT_EQ(idl_stroke->getPoints()[0]->t(), 321u);
  EXPECT_NEAR(idl_stroke->getPoints()[1]->x(), 3.1, 1e-5);
  EXPECT_NEAR(idl_stroke->getPoints()[1]->y(), 3.2, 1e-5);
  ASSERT_FALSE(idl_stroke->getPoints()[1]->hasT());
}

TEST(HandwritingTypeConvertersTest, MojoHandwritingDrawingSegmentIdl) {
  test::TaskEnvironment task_environment;
  auto mojo_drawing_segment =
      handwriting::mojom::blink::HandwritingDrawingSegment::New();
  mojo_drawing_segment->stroke_index = 123u;
  mojo_drawing_segment->begin_point_index = 10u;
  mojo_drawing_segment->end_point_index = 20u;

  auto* idl_drawing_segment =
      mojo_drawing_segment.To<blink::HandwritingDrawingSegment*>();
  EXPECT_EQ(idl_drawing_segment->strokeIndex(), 123u);
  EXPECT_EQ(idl_drawing_segment->beginPointIndex(), 10u);
  EXPECT_EQ(idl_drawing_segment->endPointIndex(), 20u);
}

TEST(HandwritingTypeConvertersTest, MojoHandwritingSegmentIdl) {
  test::TaskEnvironment task_environment;
  auto mojo_drawing_segment =
      handwriting::mojom::blink::HandwritingDrawingSegment::New();
  mojo_drawing_segment->stroke_index = 321u;
  mojo_drawing_segment->begin_point_index = 30u;
  mojo_drawing_segment->end_point_index = 40u;
  auto mojo_segment = handwriting::mojom::blink::HandwritingSegment::New();
  mojo_segment->grapheme = "The grapheme";
  mojo_segment->begin_index = 5u;
  mojo_segment->end_index = 6u;
  mojo_segment->drawing_segments.push_back(std::move(mojo_drawing_segment));

  auto* idl_segment = mojo_segment.To<blink::HandwritingSegment*>();
  EXPECT_EQ(idl_segment->grapheme(), "The grapheme");
  EXPECT_EQ(idl_segment->beginIndex(), 5u);
  EXPECT_EQ(idl_segment->endIndex(), 6u);
  ASSERT_EQ(idl_segment->drawingSegments().size(), 1u);
  EXPECT_EQ(idl_segment->drawingSegments()[0]->strokeIndex(), 321u);
  EXPECT_EQ(idl_segment->drawingSegments()[0]->beginPointIndex(), 30u);
  EXPECT_EQ(idl_segment->drawingSegments()[0]->endPointIndex(), 40u);
}

TEST(HandwritingTypeConvertersTest, MojoHandwritingPredictionIdl) {
  test::TaskEnvironment task_environment;
  auto mojo_drawing_segment =
      handwriting::mojom::blink::HandwritingDrawingSegment::New();
  mojo_drawing_segment->stroke_index = 456u;
  mojo_drawing_segment->begin_point_index = 7u;
  mojo_drawing_segment->end_point_index = 8u;
  auto mojo_segment = handwriting::mojom::blink::HandwritingSegment::New();
  mojo_segment->grapheme = "The grapheme";
  mojo_segment->begin_index = 100u;
  mojo_segment->end_index = 200u;
  mojo_segment->drawing_segments.push_back(std::move(mojo_drawing_segment));
  auto mojo_prediction =
      handwriting::mojom::blink::HandwritingPrediction::New();
  mojo_prediction->text = "The prediction";
  mojo_prediction->segmentation_result.push_back(std::move(mojo_segment));

  auto* idl_prediction = mojo_prediction.To<blink::HandwritingPrediction*>();
  EXPECT_EQ(idl_prediction->text(), "The prediction");
  ASSERT_EQ(idl_prediction->segmentationResult().size(), 1u);
  EXPECT_EQ(idl_prediction->segmentationResult()[0]->grapheme(),
            "The grapheme");
  EXPECT_EQ(idl_prediction->segmentationResult()[0]->beginIndex(), 100u);
  EXPECT_EQ(idl_prediction->segmentationResult()[0]->endIndex(), 200u);
  ASSERT_EQ(idl_prediction->segmentationResult()[0]->drawingSegments().size(),
            1u);
  EXPECT_EQ(idl_prediction->segmentationResult()[0]
                ->drawingSegments()[0]
                ->strokeIndex(),
            456u);
  EXPECT_EQ(idl_prediction->segmentationResult()[0]
                ->drawingSegments()[0]
                ->beginPointIndex(),
            7u);
  EXPECT_EQ(idl_prediction->segmentationResult()[0]
                ->drawingSegments()[0]
                ->endPointIndex(),
            8u);
}

TEST(HandwritingTypeConvertersTest, MojoHandwritingRecognizerQueryResultIdl) {
  test::TaskEnvironment task_environment;
  auto mojo_query_result =
      handwriting::mojom::blink::QueryHandwritingRecognizerResult::New();
  mojo_query_result->text_alternatives = true;
  mojo_query_result->text_segmentation = true;
  mojo_query_result->hints =
      handwriting::mojom::blink::HandwritingHintsQueryResult::New();
  mojo_query_result->hints->recognition_type =
      Vector<handwriting::mojom::blink::HandwritingRecognitionType>{
          handwriting::mojom::blink::HandwritingRecognitionType::kText};
  mojo_query_result->hints->input_type =
      Vector<handwriting::mojom::blink::HandwritingInputType>{
          handwriting::mojom::blink::HandwritingInputType::kMouse,
          handwriting::mojom::blink::HandwritingInputType::kStylus,
          handwriting::mojom::blink::HandwritingInputType::kTouch};
  mojo_query_result->hints->alternatives = true;
  mojo_query_result->hints->text_context = true;

  auto* idl_query_result =
      mojo::ConvertTo<blink::HandwritingRecognizerQueryResult*>(
          mojo_query_result);
  ASSERT_NE(idl_query_result, nullptr);
  EXPECT_TRUE(idl_query_result->hasTextAlternatives());
  EXPECT_TRUE(idl_query_result->textAlternatives());
  EXPECT_TRUE(idl_query_result->hasTextSegmentation());
  EXPECT_TRUE(idl_query_result->textSegmentation());
  EXPECT_TRUE(idl_query_result->hasHints());

  EXPECT_TRUE(idl_query_result->hints()->hasRecognitionType());
  EXPECT_EQ(1u, idl_query_result->hints()->recognitionType().size());
  EXPECT_EQ("text", idl_query_result->hints()->recognitionType()[0].AsString());

  EXPECT_TRUE(idl_query_result->hints()->hasInputType());
  EXPECT_EQ(3u, idl_query_result->hints()->inputType().size());
  EXPECT_EQ("mouse", idl_query_result->hints()->inputType()[0].AsString());
  EXPECT_EQ("stylus", idl_query_result->hints()->inputType()[1].AsString());
  EXPECT_EQ("touch", idl_query_result->hints()->inputType()[2].AsString());

  EXPECT_TRUE(idl_query_result->hints()->hasAlternatives());
  EXPECT_TRUE(idl_query_result->hints()->alternatives());

  EXPECT_TRUE(idl_query_result->hints()->hasTextContext());
  EXPECT_TRUE(idl_query_result->hints()->textContext());
}

TEST(HandwritingTypeConvertersTest,
     MojoHandwritingRecognizerQueryResultIdl_FalseValues) {
  auto mojo_query_result =
      handwriting::mojom::blink::QueryHandwritingRecognizerResult::New();
  mojo_query_result->text_alternatives = false;
  mojo_query_result->text_segmentation = false;
  mojo_query_result->hints =
      handwriting::mojom::blink::HandwritingHintsQueryResult::New();
  mojo_query_result->hints->recognition_type =
      Vector<handwriting::mojom::blink::HandwritingRecognitionType>{};
  mojo_query_result->hints->input_type =
      Vector<handwriting::mojom::blink::HandwritingInputType>{};
  mojo_query_result->hints->alternatives = false;
  mojo_query_result->hints->text_context = false;

  auto* idl_query_result =
      mojo::ConvertTo<blink::HandwritingRecognizerQueryResult*>(
          mojo_query_result);
  ASSERT_NE(idl_query_result, nullptr);
  EXPECT_TRUE(idl_query_result->hasTextAlternatives());
  EXPECT_FALSE(idl_query_result->textAlternatives());
  EXPECT_TRUE(idl_query_result->hasTextSegmentation());
  EXPECT_FALSE(idl_query_result->textSegmentation());
  EXPECT_TRUE(idl_query_result->hasHints());

  EXPECT_TRUE(idl_query_result->hints()->hasRecognitionType());
  EXPECT_EQ(0u, idl_query_result->hints()->recognitionType().size());

  EXPECT_TRUE(idl_query_result->hints()->hasInputType());
  EXPECT_EQ(0u, idl_query_result->hints()->inputType().size());

  EXPECT_TRUE(idl_query_result->hints()->hasAlternatives());
  EXPECT_FALSE(idl_query_result->hints()->alternatives());

  EXPECT_TRUE(idl_query_result->hints()->hasTextContext());
  EXPECT_FALSE(idl_query_result->hints()->textContext());
}

}  // namespace blink
