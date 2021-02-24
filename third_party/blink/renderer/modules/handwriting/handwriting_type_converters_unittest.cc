// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/handwriting/handwriting.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_drawing_segment.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_feature_query.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_feature_query_result.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_hints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_point.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_prediction.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_segment.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_stroke.h"
#include "third_party/blink/renderer/modules/handwriting/handwriting_type_converters.h"

namespace blink {

using handwriting::mojom::blink::HandwritingDrawingSegmentPtr;
using handwriting::mojom::blink::HandwritingFeatureQueryPtr;
using handwriting::mojom::blink::HandwritingFeatureQueryResultPtr;
using handwriting::mojom::blink::HandwritingFeatureStatus;
using handwriting::mojom::blink::HandwritingHintsPtr;
using handwriting::mojom::blink::HandwritingPointPtr;
using handwriting::mojom::blink::HandwritingPredictionPtr;
using handwriting::mojom::blink::HandwritingSegmentPtr;
using handwriting::mojom::blink::HandwritingStrokePtr;

TEST(HandwritingTypeConvertersTest, IdlHandwritingPointToMojo) {
  auto* idl_point = blink::HandwritingPoint::Create();

  idl_point->setX(1.1);
  idl_point->setY(2.3);
  idl_point->setT(345);

  auto mojo_point = mojo::ConvertTo<HandwritingPointPtr>(idl_point);
  ASSERT_FALSE(mojo_point.is_null());
  EXPECT_NEAR(mojo_point->location.x(), 1.1, 1e-5);
  EXPECT_NEAR(mojo_point->location.y(), 2.3, 1e-5);
  EXPECT_EQ(mojo_point->t.InMilliseconds(), 345);
}

TEST(HandwritingTypeConvertersTest, IdlHandwritingStrokeToMojo) {
  auto* idl_stroke = blink::HandwritingStroke::Create();
  auto* idl_point1 = blink::HandwritingPoint::Create();
  idl_point1->setX(0.1);
  idl_point1->setY(0.2);
  idl_point1->setT(123);
  auto* idl_point2 = blink::HandwritingPoint::Create();
  idl_stroke->addPoint(idl_point1);
  idl_point2->setX(1.1);
  idl_point2->setY(1.2);
  idl_point2->setT(456);
  idl_stroke->addPoint(idl_point2);

  auto mojo_stroke = mojo::ConvertTo<HandwritingStrokePtr>(idl_stroke);
  ASSERT_FALSE(mojo_stroke.is_null());
  ASSERT_EQ(mojo_stroke->points.size(), 2u);
  EXPECT_NEAR(mojo_stroke->points[0]->location.x(), 0.1, 1e-5);
  EXPECT_NEAR(mojo_stroke->points[0]->location.y(), 0.2, 1e-5);
  EXPECT_EQ(mojo_stroke->points[0]->t.InMilliseconds(), 123);
  EXPECT_NEAR(mojo_stroke->points[1]->location.x(), 1.1, 1e-5);
  EXPECT_NEAR(mojo_stroke->points[1]->location.y(), 1.2, 1e-5);
  EXPECT_EQ(mojo_stroke->points[1]->t.InMilliseconds(), 456);
}

TEST(HandwritingTypeConvertersTest, IdlHandwritingHintsToMojo) {
  auto* idl_hints = blink::HandwritingHints::Create();
  idl_hints->setRecognitionType("recognition type");
  idl_hints->setInputType("input type");
  idl_hints->setTextContext("text context");
  idl_hints->setAlternatives(10);

  auto mojo_hints = mojo::ConvertTo<HandwritingHintsPtr>(idl_hints);
  ASSERT_FALSE(mojo_hints.is_null());
  EXPECT_EQ(mojo_hints->recognition_type, "recognition type");
  EXPECT_EQ(mojo_hints->input_type, "input type");
  EXPECT_EQ(mojo_hints->text_context, "text context");
  EXPECT_EQ(mojo_hints->alternatives, 10u);
}

TEST(HandwritingTypeConvertersTest, IdlHandwritingFeatureQueryToMojo) {
  V8TestingScope v8_testing_scope;
  auto* idl_query = blink::HandwritingFeatureQuery::Create();
  idl_query->setLanguages({"en", "fr"});
  idl_query->setAlternatives(
      blink::ScriptValue::From(v8_testing_scope.GetScriptState(), 10));
  // Intentionally does not set `segmentationResult`.

  auto mojo_query = mojo::ConvertTo<HandwritingFeatureQueryPtr>(idl_query);
  ASSERT_FALSE(mojo_query.is_null());
  ASSERT_EQ(mojo_query->languages.size(), 2u);
  EXPECT_EQ(mojo_query->languages[0], "en");
  EXPECT_EQ(mojo_query->languages[1], "fr");
  EXPECT_EQ(mojo_query->alternatives, true);
  EXPECT_EQ(mojo_query->segmentation_result, false);
}

TEST(HandwritingTypeConvertersTest, MojoHandwritingPointToIdl) {
  auto mojo_point = handwriting::mojom::blink::HandwritingPoint::New();
  mojo_point->location = gfx::PointF(0.3, 0.4);
  mojo_point->t = base::TimeDelta::FromMilliseconds(123);

  auto* idl_point = mojo::ConvertTo<blink::HandwritingPoint*>(mojo_point);
  ASSERT_NE(idl_point, nullptr);
  EXPECT_NEAR(idl_point->x(), 0.3, 1e-5);
  EXPECT_NEAR(idl_point->y(), 0.4, 1e-5);
  EXPECT_EQ(idl_point->t(), 123u);
}

TEST(HandwritingTypeConvertersTest, MojoHandwritingStrokeToIdl) {
  auto mojo_stroke = handwriting::mojom::blink::HandwritingStroke::New();
  auto mojo_point1 = handwriting::mojom::blink::HandwritingPoint::New();
  mojo_point1->location = gfx::PointF(2.1, 2.2);
  mojo_point1->t = base::TimeDelta::FromMilliseconds(321);
  mojo_stroke->points.push_back(std::move(mojo_point1));
  auto mojo_point2 = handwriting::mojom::blink::HandwritingPoint::New();
  mojo_point2->location = gfx::PointF(3.1, 3.2);
  mojo_point2->t = base::TimeDelta::FromMilliseconds(456);
  mojo_stroke->points.push_back(std::move(mojo_point2));

  auto* idl_stroke = mojo::ConvertTo<blink::HandwritingStroke*>(mojo_stroke);
  ASSERT_NE(idl_stroke, nullptr);
  ASSERT_EQ(idl_stroke->getPoints().size(), 2u);
  EXPECT_NEAR(idl_stroke->getPoints()[0]->x(), 2.1, 1e-5);
  EXPECT_NEAR(idl_stroke->getPoints()[0]->y(), 2.2, 1e-5);
  EXPECT_EQ(idl_stroke->getPoints()[0]->t(), 321u);
  EXPECT_NEAR(idl_stroke->getPoints()[1]->x(), 3.1, 1e-5);
  EXPECT_NEAR(idl_stroke->getPoints()[1]->y(), 3.2, 1e-5);
  EXPECT_EQ(idl_stroke->getPoints()[1]->t(), 456u);
}

TEST(HandwritingTypeConvertersTest, MojoHandwritingFeatureQueryResultIdl) {
  auto mojo_query_result =
      handwriting::mojom::blink::HandwritingFeatureQueryResult::New();
  mojo_query_result->languages = HandwritingFeatureStatus::kNotQueried;
  mojo_query_result->alternatives = HandwritingFeatureStatus::kSupported;
  mojo_query_result->segmentation_result =
      HandwritingFeatureStatus::kNotSupported;

  auto* idl_query_result =
      mojo::ConvertTo<blink::HandwritingFeatureQueryResult*>(mojo_query_result);
  ASSERT_NE(idl_query_result, nullptr);
  EXPECT_FALSE(idl_query_result->hasLanguages());
  ASSERT_TRUE(idl_query_result->hasAlternatives());
  EXPECT_TRUE(idl_query_result->alternatives());
  ASSERT_TRUE(idl_query_result->hasSegmentationResult());
  EXPECT_FALSE(idl_query_result->segmentationResult());
}

TEST(HandwritingTypeConvertersTest, MojoHandwritingDrawingSegmentIdl) {
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

}  // namespace blink
