// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/identifiability_metrics.h"

#include "base/functional/callback.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_stringsequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_constrain_boolean_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_constrain_dom_string_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_constrain_double_range.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_constrain_long_range.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_constrain_point_2d_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_stream_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_constraint_set.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_point_2d.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_boolean_constrainbooleanparameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_boolean_constraindoublerange_double.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_boolean_mediatrackconstraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_constraindomstringparameters_string_stringsequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_constraindoublerange_double.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_constrainlongrange_long.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_constrainpoint2dparameters_point2dsequence.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"

namespace blink {

namespace {

template <typename T>
void Visit(IdentifiableTokenBuilder& builder, const T* range) {
  if (!range)
    return;
  builder.AddToken(range->hasExact() ? range->exact() : IdentifiableToken());
  builder.AddToken(range->hasIdeal() ? range->ideal() : IdentifiableToken());
  builder.AddToken(range->hasMax() ? range->max() : IdentifiableToken());
  builder.AddToken(range->hasMin() ? range->min() : IdentifiableToken());
}

void Visit(IdentifiableTokenBuilder& builder, const V8ConstrainDouble* d) {
  if (!d) {
    builder.AddToken(IdentifiableToken());
    return;
  }
  switch (d->GetContentType()) {
    case V8ConstrainDouble::ContentType::kConstrainDoubleRange:
      return Visit(builder, d->GetAsConstrainDoubleRange());
    case V8ConstrainDouble::ContentType::kDouble:
      builder.AddToken(d->GetAsDouble());
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

void Visit(IdentifiableTokenBuilder& builder, const V8ConstrainLong* l) {
  if (!l) {
    builder.AddToken(IdentifiableToken());
    return;
  }
  switch (l->GetContentType()) {
    case V8ConstrainLong::ContentType::kConstrainLongRange:
      return Visit(builder, l->GetAsConstrainLongRange());
    case V8ConstrainLong::ContentType::kLong:
      builder.AddToken(l->GetAsLong());
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

void Visit(IdentifiableTokenBuilder& builder,
           const V8UnionStringOrStringSequence* s) {
  if (!s) {
    builder.AddToken(IdentifiableToken());
    return;
  }
  switch (s->GetContentType()) {
    case V8UnionStringOrStringSequence::ContentType::kString:
      builder.AddToken(IdentifiabilityBenignStringToken(s->GetAsString()));
      return;
    case V8UnionStringOrStringSequence::ContentType::kStringSequence:
      for (const String& str : s->GetAsStringSequence()) {
        builder.AddToken(IdentifiabilityBenignStringToken(str));
      }
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

void Visit(IdentifiableTokenBuilder& builder, const V8ConstrainDOMString* s) {
  if (!s) {
    builder.AddToken(IdentifiableToken());
    return;
  }
  switch (s->GetContentType()) {
    case V8ConstrainDOMString::ContentType::kConstrainDOMStringParameters: {
      const ConstrainDOMStringParameters* params =
          s->GetAsConstrainDOMStringParameters();
      Visit(builder, params->getExactOr(nullptr));
      Visit(builder, params->getIdealOr(nullptr));
      return;
    }
    case V8ConstrainDOMString::ContentType::kString:
      builder.AddToken(IdentifiabilityBenignStringToken(s->GetAsString()));
      return;
    case V8ConstrainDOMString::ContentType::kStringSequence:
      for (const String& str : s->GetAsStringSequence()) {
        builder.AddToken(IdentifiabilityBenignStringToken(str));
      }
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

void Visit(IdentifiableTokenBuilder& builder, const V8ConstrainBoolean* b) {
  if (!b) {
    builder.AddToken(IdentifiableToken());
    return;
  }
  switch (b->GetContentType()) {
    case V8ConstrainBoolean::ContentType::kBoolean:
      builder.AddToken(b->GetAsBoolean());
      return;
    case V8ConstrainBoolean::ContentType::kConstrainBooleanParameters: {
      const ConstrainBooleanParameters* params =
          b->GetAsConstrainBooleanParameters();
      builder.AddToken(params->hasExact() ? params->exact()
                                          : IdentifiableToken());
      builder.AddToken(params->hasIdeal() ? params->ideal()
                                          : IdentifiableToken());
      return;
    }
  }
  NOTREACHED_IN_MIGRATION();
}

void Visit(IdentifiableTokenBuilder& builder,
           const V8UnionBooleanOrConstrainDouble* x) {
  if (!x) {
    builder.AddToken(IdentifiableToken());
    return;
  }
  switch (x->GetContentType()) {
    case V8UnionBooleanOrConstrainDouble::ContentType::kBoolean:
      builder.AddToken(x->GetAsBoolean());
      return;
    case V8UnionBooleanOrConstrainDouble::ContentType::kConstrainDoubleRange:
      return Visit(builder, x->GetAsConstrainDoubleRange());
    case V8UnionBooleanOrConstrainDouble::ContentType::kDouble:
      builder.AddToken(x->GetAsDouble());
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

void Visit(IdentifiableTokenBuilder& builder,
           const HeapVector<Member<Point2D>>& points) {
  for (const auto& point : points) {
    builder.AddToken(point->hasX() ? point->x() : IdentifiableToken());
    builder.AddToken(point->hasY() ? point->y() : IdentifiableToken());
  }
}

void Visit(IdentifiableTokenBuilder& builder, const V8ConstrainPoint2D* p) {
  if (!p) {
    builder.AddToken(IdentifiableToken());
    return;
  }
  switch (p->GetContentType()) {
    case V8ConstrainPoint2D::ContentType::kConstrainPoint2DParameters: {
      const ConstrainPoint2DParameters* params =
          p->GetAsConstrainPoint2DParameters();
      if (params->hasExact()) {
        Visit(builder, params->exact());
      } else {
        builder.AddToken(IdentifiableToken());
      }
      if (params->hasIdeal()) {
        Visit(builder, params->ideal());
      } else {
        builder.AddToken(IdentifiableToken());
      }
      return;
    }
    case V8ConstrainPoint2D::ContentType::kPoint2DSequence:
      return Visit(builder, p->GetAsPoint2DSequence());
  }
  NOTREACHED_IN_MIGRATION();
}

void Visit(IdentifiableTokenBuilder& builder,
           const MediaTrackConstraintSet& set) {
  // TODO(crbug.com/1070871): As a workaround for code simplicity, we use a
  // default value of a union type if each member is not provided in input.
  Visit(builder, set.getWidthOr(nullptr));
  Visit(builder, set.getHeightOr(nullptr));
  Visit(builder, set.getAspectRatioOr(nullptr));
  Visit(builder, set.getFrameRateOr(nullptr));
  Visit(builder, set.getFacingModeOr(nullptr));
  Visit(builder, set.getSampleRateOr(nullptr));
  Visit(builder, set.getSampleSizeOr(nullptr));
  Visit(builder, set.getEchoCancellationOr(nullptr));
  Visit(builder, set.getAutoGainControlOr(nullptr));
  Visit(builder, set.getLatencyOr(nullptr));
  Visit(builder, set.getChannelCountOr(nullptr));
  Visit(builder, set.getWhiteBalanceModeOr(nullptr));
  Visit(builder, set.getExposureModeOr(nullptr));
  Visit(builder, set.getFocusModeOr(nullptr));
  Visit(builder, set.getPointsOfInterestOr(nullptr));
  Visit(builder, set.getExposureCompensationOr(nullptr));
  Visit(builder, set.getExposureTimeOr(nullptr));
  Visit(builder, set.getColorTemperatureOr(nullptr));
  Visit(builder, set.getIsoOr(nullptr));
  Visit(builder, set.getBrightnessOr(nullptr));
  Visit(builder, set.getContrastOr(nullptr));
  Visit(builder, set.getSaturationOr(nullptr));
  Visit(builder, set.getSharpnessOr(nullptr));
  Visit(builder, set.getFocusDistanceOr(nullptr));
  Visit(builder, set.getPanOr(nullptr));
  Visit(builder, set.getTiltOr(nullptr));
  Visit(builder, set.getZoomOr(nullptr));
  Visit(builder, set.getTorchOr(nullptr));
  Visit(builder, set.getBackgroundBlurOr(nullptr));
  Visit(builder, set.getBackgroundSegmentationMaskOr(nullptr));
  Visit(builder, set.getEyeGazeCorrectionOr(nullptr));
  Visit(builder, set.getFaceFramingOr(nullptr));
}

void Visit(IdentifiableTokenBuilder& builder,
           const V8UnionBooleanOrMediaTrackConstraints* constraint) {
  if (!constraint) {
    builder.AddToken(IdentifiableToken());
    return;
  }
  switch (constraint->GetContentType()) {
    case V8UnionBooleanOrMediaTrackConstraints::ContentType::kBoolean:
      builder.AddToken(constraint->GetAsBoolean());
      return;
    case V8UnionBooleanOrMediaTrackConstraints::ContentType::
        kMediaTrackConstraints: {
      const MediaTrackConstraints* constraints =
          constraint->GetAsMediaTrackConstraints();
      DCHECK(constraints);
      if (constraints->hasAdvanced()) {
        for (const auto& advanced : constraints->advanced()) {
          Visit(builder, *advanced);
        }
      } else {
        builder.AddToken(IdentifiableToken());
      }
      return;
    }
  }
  NOTREACHED_IN_MIGRATION();
}

}  // namespace

IdentifiableToken TokenFromConstraints(
    const MediaStreamConstraints* constraints) {
  IdentifiableTokenBuilder builder;
  if (constraints) {
    Visit(builder, constraints->audio());
    Visit(builder, constraints->video());
  } else {
    builder.AddToken(IdentifiableToken());
  }
  return builder.GetToken();
}

void RecordIdentifiabilityMetric(const IdentifiableSurface& surface,
                                 ExecutionContext* context,
                                 IdentifiableToken token) {
  if (surface.IsValid() && context &&
      IdentifiabilityStudySettings::Get()->ShouldSampleSurface(surface)) {
    IdentifiabilityMetricBuilder(context->UkmSourceID())
        .Add(surface, token)
        .Record(context->UkmRecorder());
  }
}

}  // namespace blink
