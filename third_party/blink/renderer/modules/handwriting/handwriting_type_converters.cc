// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/handwriting/handwriting_type_converters.h"

#include "third_party/blink/public/mojom/handwriting/handwriting.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_drawing_segment.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_feature_query.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_feature_query_result.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_hints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_point.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_prediction.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_segment.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_stroke.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace mojo {

using handwriting::mojom::blink::HandwritingDrawingSegmentPtr;
using handwriting::mojom::blink::HandwritingFeatureQueryPtr;
using handwriting::mojom::blink::HandwritingFeatureQueryResultPtr;
using handwriting::mojom::blink::HandwritingFeatureStatus;
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
  output->t = base::TimeDelta::FromMilliseconds(input->t());
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
  for (const auto& point : input->getPoints()) {
    output->points.push_back(mojo::ConvertTo<HandwritingPointPtr>(point.Get()));
  }
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
  output->text_context = input->textContext();
  output->alternatives = input->alternatives();
  return output;
}

// static
HandwritingFeatureQueryPtr
TypeConverter<HandwritingFeatureQueryPtr, blink::HandwritingFeatureQuery*>::
    Convert(const blink::HandwritingFeatureQuery* input) {
  if (!input) {
    return nullptr;
  }
  auto output = handwriting::mojom::blink::HandwritingFeatureQuery::New();
  for (const auto& lang : input->languages()) {
    output->languages.push_back(lang);
  }
  output->alternatives = input->hasAlternatives();
  output->segmentation_result = input->hasSegmentationResult();

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
  output->setT(input->t.InMilliseconds());
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
blink::HandwritingFeatureQueryResult*
TypeConverter<blink::HandwritingFeatureQueryResult*,
              HandwritingFeatureQueryResultPtr>::
    Convert(const HandwritingFeatureQueryResultPtr& input) {
  if (!input) {
    return nullptr;
  }
  auto* output = blink::HandwritingFeatureQueryResult::Create();

#define HANDWRITING_SET_FEATURE_QUERY_RESULT(feature, setter)        \
  if (input->feature != HandwritingFeatureStatus::kNotQueried) {     \
    if (input->feature == HandwritingFeatureStatus::kNotSupported) { \
      output->setter(false);                                         \
    } else {                                                         \
      output->setter(true);                                          \
    }                                                                \
  }

  HANDWRITING_SET_FEATURE_QUERY_RESULT(languages, setLanguages);
  HANDWRITING_SET_FEATURE_QUERY_RESULT(alternatives, setAlternatives);
  HANDWRITING_SET_FEATURE_QUERY_RESULT(segmentation_result,
                                       setSegmentationResult);
#undef HANDWRITING_SET_FEATURE_QUERY_RESULT

  return output;
}

}  // namespace mojo
