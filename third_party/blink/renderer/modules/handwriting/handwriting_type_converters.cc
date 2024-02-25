// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/handwriting/handwriting_type_converters.h"

#include "third_party/blink/public/mojom/handwriting/handwriting.mojom-blink.h"
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
#include "third_party/blink/renderer/modules/modules_export.h"

namespace mojo {

using handwriting::mojom::blink::HandwritingDrawingSegmentPtr;
using handwriting::mojom::blink::HandwritingHintsPtr;
using handwriting::mojom::blink::HandwritingPointPtr;
using handwriting::mojom::blink::HandwritingPredictionPtr;
using handwriting::mojom::blink::HandwritingSegmentPtr;
using handwriting::mojom::blink::HandwritingStrokePtr;

// Converters from IDL to Mojo.

// static
HandwritingPointPtr
TypeConverter<HandwritingPointPtr, blink::HandwritingPoint*>::Convert(
    const blink::HandwritingPoint* input) {
  if (!input) {
    return nullptr;
  }
  auto output = handwriting::mojom::blink::HandwritingPoint::New();
  output->location = gfx::PointF(input->x(), input->y());
  if (input->hasT()) {
    output->t = base::Milliseconds(input->t());
  }
  return output;
}

// static
HandwritingStrokePtr
TypeConverter<HandwritingStrokePtr, blink::HandwritingStroke*>::Convert(
    const blink::HandwritingStroke* input) {
  if (!input) {
    return nullptr;
  }
  auto output = handwriting::mojom::blink::HandwritingStroke::New();
  output->points =
      mojo::ConvertTo<Vector<HandwritingPointPtr>>(input->getPoints());
  return output;
}

// static
HandwritingHintsPtr
TypeConverter<HandwritingHintsPtr, blink::HandwritingHints*>::Convert(
    const blink::HandwritingHints* input) {
  if (!input) {
    return nullptr;
  }
  auto output = handwriting::mojom::blink::HandwritingHints::New();
  output->recognition_type = input->recognitionType();
  output->input_type = input->inputType();
  if (input->hasTextContext()) {
    output->text_context = input->textContext();
  }
  output->alternatives = input->alternatives();
  return output;
}

// Converters from Mojo to IDL.

// static
blink::HandwritingPoint*
TypeConverter<blink::HandwritingPoint*, HandwritingPointPtr>::Convert(
    const HandwritingPointPtr& input) {
  if (!input) {
    return nullptr;
  }
  auto* output = blink::HandwritingPoint::Create();
  output->setX(input->location.x());
  output->setY(input->location.y());
  if (input->t.has_value()) {
    output->setT(input->t->InMilliseconds());
  }
  return output;
}

// static
blink::HandwritingStroke*
TypeConverter<blink::HandwritingStroke*, HandwritingStrokePtr>::Convert(
    const HandwritingStrokePtr& input) {
  if (!input) {
    return nullptr;
  }
  auto* output = blink::HandwritingStroke::Create();
  for (const auto& point : input->points) {
    output->addPoint(point.To<blink::HandwritingPoint*>());
  }
  return output;
}

// static
blink::HandwritingDrawingSegment*
TypeConverter<blink::HandwritingDrawingSegment*, HandwritingDrawingSegmentPtr>::
    Convert(const HandwritingDrawingSegmentPtr& input) {
  if (!input) {
    return nullptr;
  }
  auto* output = blink::HandwritingDrawingSegment::Create();
  output->setStrokeIndex(input->stroke_index);
  output->setBeginPointIndex(input->begin_point_index);
  output->setEndPointIndex(input->end_point_index);
  return output;
}

// static
blink::HandwritingSegment*
TypeConverter<blink::HandwritingSegment*,
              handwriting::mojom::blink::HandwritingSegmentPtr>::
    Convert(const handwriting::mojom::blink::HandwritingSegmentPtr& input) {
  if (!input) {
    return nullptr;
  }
  auto* output = blink::HandwritingSegment::Create();
  output->setGrapheme(input->grapheme);
  output->setBeginIndex(input->begin_index);
  output->setEndIndex(input->end_index);
  blink::HeapVector<blink::Member<blink::HandwritingDrawingSegment>>
      drawing_segments;
  for (const auto& drw_seg : input->drawing_segments) {
    drawing_segments.push_back(drw_seg.To<blink::HandwritingDrawingSegment*>());
  }
  output->setDrawingSegments(std::move(drawing_segments));
  return output;
}

// static
blink::HandwritingPrediction*
TypeConverter<blink::HandwritingPrediction*,
              handwriting::mojom::blink::HandwritingPredictionPtr>::
    Convert(const handwriting::mojom::blink::HandwritingPredictionPtr& input) {
  if (!input) {
    return nullptr;
  }
  auto* output = blink::HandwritingPrediction::Create();
  output->setText(input->text);
  blink::HeapVector<blink::Member<blink::HandwritingSegment>> segments;
  for (const auto& seg : input->segmentation_result) {
    segments.push_back(seg.To<blink::HandwritingSegment*>());
  }
  output->setSegmentationResult(std::move(segments));
  return output;
}

// static
handwriting::mojom::blink::HandwritingModelConstraintPtr
TypeConverter<handwriting::mojom::blink::HandwritingModelConstraintPtr,
              blink::HandwritingModelConstraint*>::
    Convert(const blink::HandwritingModelConstraint* input) {
  if (!input)
    return nullptr;

  auto output = handwriting::mojom::blink::HandwritingModelConstraint::New();
  if (input->hasLanguages()) {
    for (const auto& lang : input->languages()) {
      output->languages.push_back(lang);
    }
  }

  return output;
}

Vector<blink::V8HandwritingRecognitionType> ConvertRecognitionType(
    const Vector<handwriting::mojom::blink::HandwritingRecognitionType>&
        input) {
  using V8Type = blink::V8HandwritingRecognitionType;
  using BlinkType = handwriting::mojom::blink::HandwritingRecognitionType;

  Vector<V8Type> ret;

  for (const auto& it : input) {
    switch (it) {
      case BlinkType::kText:
        ret.push_back(V8Type(V8Type::Enum::kText));
        break;
    }
  }

  return ret;
}

Vector<blink::V8HandwritingInputType> ConvertInputType(
    const Vector<handwriting::mojom::blink::HandwritingInputType>& input) {
  using V8Type = blink::V8HandwritingInputType;
  using BlinkType = handwriting::mojom::blink::HandwritingInputType;

  Vector<V8Type> ret;

  for (const auto& it : input) {
    switch (it) {
      case BlinkType::kMouse:
        ret.push_back(V8Type(V8Type::Enum::kMouse));
        break;
      case BlinkType::kStylus:
        ret.push_back(V8Type(V8Type::Enum::kStylus));
        break;
      case BlinkType::kTouch:
        ret.push_back(V8Type(V8Type::Enum::kTouch));
        break;
    }
  }

  return ret;
}

// static
blink::HandwritingRecognizerQueryResult*
TypeConverter<blink::HandwritingRecognizerQueryResult*,
              handwriting::mojom::blink::QueryHandwritingRecognizerResultPtr>::
    Convert(
        const handwriting::mojom::blink::QueryHandwritingRecognizerResultPtr&
            input) {
  if (!input)
    return nullptr;

  auto* hints = blink::HandwritingHintsQueryResult::Create();
  hints->setTextContext(input->hints->text_context);
  hints->setAlternatives(input->hints->alternatives);
  hints->setRecognitionType(
      ConvertRecognitionType(input->hints->recognition_type));
  hints->setInputType(ConvertInputType(input->hints->input_type));

  auto* output = blink::HandwritingRecognizerQueryResult::Create();
  output->setTextAlternatives(input->text_alternatives);
  output->setTextSegmentation(input->text_segmentation);
  output->setHints(hints);

  return output;
}

}  // namespace mojo
