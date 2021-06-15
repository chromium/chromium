// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/identifiability_metrics.h"

#include "base/callback.h"
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

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_DICTIONARY)
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
  NOTREACHED();
}
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_DICTIONARY)
void Visit(IdentifiableTokenBuilder& builder, const ConstrainDouble& d) {
  if (d.IsDouble()) {
    builder.AddToken(d.GetAsDouble());
    return;
  }
  if (d.IsConstrainDoubleRange()) {
    Visit(builder, d.GetAsConstrainDoubleRange());
    return;
  }
  DCHECK(d.IsNull());
  builder.AddToken(IdentifiableToken());
}
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_DICTIONARY)

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_DICTIONARY)
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
  NOTREACHED();
}
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_DICTIONARY)
void Visit(IdentifiableTokenBuilder& builder, const ConstrainLong& l) {
  if (l.IsLong()) {
    builder.AddToken(l.GetAsLong());
    return;
  }
  if (l.IsConstrainLongRange()) {
    Visit(builder, l.GetAsConstrainLongRange());
    return;
  }
  DCHECK(l.IsNull());
  builder.AddToken(IdentifiableToken());
}
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_DICTIONARY)

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_DICTIONARY)
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
  NOTREACHED();
}
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_DICTIONARY)
void Visit(IdentifiableTokenBuilder& builder, const StringOrStringSequence& s) {
  if (s.IsString()) {
    builder.AddToken(IdentifiabilityBenignStringToken(s.GetAsString()));
    return;
  }
  if (s.IsStringSequence()) {
    for (const String& str : s.GetAsStringSequence()) {
      builder.AddToken(IdentifiabilityBenignStringToken(str));
    }
    return;
  }
  DCHECK(s.IsNull());
  builder.AddToken(IdentifiableToken());
}
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_DICTIONARY)

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_DICTIONARY)
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
  NOTREACHED();
}
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_DICTIONARY)
void Visit(IdentifiableTokenBuilder& builder, const ConstrainDOMString& s) {
  if (s.IsString()) {
    builder.AddToken(IdentifiabilityBenignStringToken(s.GetAsString()));
    return;
  }
  if (s.IsStringSequence()) {
    for (const String& str : s.GetAsStringSequence()) {
      builder.AddToken(IdentifiabilityBenignStringToken(str));
    }
    return;
  }
  if (s.IsConstrainDOMStringParameters()) {
    const ConstrainDOMStringParameters* params =
        s.GetAsConstrainDOMStringParameters();
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
  DCHECK(s.IsNull());
  builder.AddToken(IdentifiableToken());
}
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_DICTIONARY)

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_DICTIONARY)
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
  NOTREACHED();
}
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_DICTIONARY)
void Visit(IdentifiableTokenBuilder& builder, const ConstrainBoolean& b) {
  if (b.IsBoolean()) {
    builder.AddToken(b.GetAsBoolean());
    return;
  }
  if (b.IsConstrainBooleanParameters()) {
    const ConstrainBooleanParameters* params =
        b.GetAsConstrainBooleanParameters();
    builder.AddToken(params->hasExact() ? params->exact()
                                        : IdentifiableToken());
    builder.AddToken(params->hasIdeal() ? params->ideal()
                                        : IdentifiableToken());
    return;
  }
  DCHECK(b.IsNull());
  builder.AddToken(IdentifiableToken());
}
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_DICTIONARY)

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_DICTIONARY)
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
  NOTREACHED();
}
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_DICTIONARY)
void Visit(IdentifiableTokenBuilder& builder,
           const BooleanOrDoubleOrConstrainDoubleRange& x) {
  if (x.IsBoolean()) {
    builder.AddToken(x.GetAsBoolean());
    return;
  }
  if (x.IsConstrainDoubleRange()) {
    Visit(builder, x.GetAsConstrainDoubleRange());
    return;
  }
  if (x.IsDouble()) {
    builder.AddToken(x.GetAsDouble());
    return;
  }
  DCHECK(x.IsNull());
  builder.AddToken(IdentifiableToken());
}
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_DICTIONARY)

void Visit(IdentifiableTokenBuilder& builder,
           const HeapVector<Member<Point2D>>& points) {
  for (const auto& point : points) {
    builder.AddToken(point->hasX() ? point->x() : IdentifiableToken());
    builder.AddToken(point->hasY() ? point->y() : IdentifiableToken());
  }
}

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_DICTIONARY)
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
  NOTREACHED();
}
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_DICTIONARY)
void Visit(IdentifiableTokenBuilder& builder, const ConstrainPoint2D& x) {
  if (x.IsPoint2DSequence()) {
    Visit(builder, x.GetAsPoint2DSequence());
    return;
  }
  if (x.IsConstrainPoint2DParameters()) {
    const ConstrainPoint2DParameters* params =
        x.GetAsConstrainPoint2DParameters();
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
  DCHECK(x.IsNull());
  builder.AddToken(IdentifiableToken());
}
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_DICTIONARY)

void Visit(IdentifiableTokenBuilder& builder,
           const MediaTrackConstraintSet& set) {
  // TODO(crbug.com/1070871): As a workaround for code simplicity, we use a
  // default value of a union type if each member is not provided in input.
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_DICTIONARY)
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
  Visit(builder, set.getVideoKindOr(nullptr));
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
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_DICTIONARY)
  Visit(builder, set.hasWidth() ? set.width() : ConstrainLong());
  Visit(builder, set.hasHeight() ? set.height() : ConstrainLong());
  Visit(builder, set.hasAspectRatio() ? set.aspectRatio() : ConstrainDouble());
  Visit(builder, set.hasFrameRate() ? set.frameRate() : ConstrainDouble());
  Visit(builder, set.hasFacingMode() ? set.facingMode() : ConstrainDOMString());
  Visit(builder, set.hasSampleRate() ? set.sampleRate() : ConstrainLong());
  Visit(builder, set.hasSampleSize() ? set.sampleSize() : ConstrainLong());
  Visit(builder, set.hasEchoCancellation() ? set.echoCancellation()
                                           : ConstrainBoolean());
  Visit(builder,
        set.hasAutoGainControl() ? set.autoGainControl() : ConstrainBoolean());
  Visit(builder, set.hasLatency() ? set.latency() : ConstrainDouble());
  Visit(builder, set.hasChannelCount() ? set.channelCount() : ConstrainLong());
  Visit(builder, set.hasVideoKind() ? set.videoKind() : ConstrainDOMString());
  Visit(builder, set.hasWhiteBalanceMode() ? set.whiteBalanceMode()
                                           : ConstrainDOMString());
  Visit(builder,
        set.hasExposureMode() ? set.exposureMode() : ConstrainDOMString());
  Visit(builder, set.hasFocusMode() ? set.focusMode() : ConstrainDOMString());
  Visit(builder, set.hasPointsOfInterest() ? set.pointsOfInterest()
                                           : ConstrainPoint2D());
  Visit(builder, set.hasExposureCompensation() ? set.exposureCompensation()
                                               : ConstrainDouble());
  Visit(builder,
        set.hasExposureTime() ? set.exposureTime() : ConstrainDouble());
  Visit(builder,
        set.hasColorTemperature() ? set.colorTemperature() : ConstrainDouble());
  Visit(builder, set.hasIso() ? set.iso() : ConstrainDouble());
  Visit(builder, set.hasBrightness() ? set.brightness() : ConstrainDouble());
  Visit(builder, set.hasContrast() ? set.contrast() : ConstrainDouble());
  Visit(builder, set.hasSaturation() ? set.saturation() : ConstrainDouble());
  Visit(builder, set.hasSharpness() ? set.sharpness() : ConstrainDouble());
  Visit(builder,
        set.hasFocusDistance() ? set.focusDistance() : ConstrainDouble());
  Visit(builder,
        set.hasPan() ? set.pan() : BooleanOrDoubleOrConstrainDoubleRange());
  Visit(builder,
        set.hasTilt() ? set.tilt() : BooleanOrDoubleOrConstrainDoubleRange());
  Visit(builder,
        set.hasZoom() ? set.zoom() : BooleanOrDoubleOrConstrainDoubleRange());
  Visit(builder, set.hasTorch() ? set.torch() : ConstrainBoolean());
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_DICTIONARY)
}

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_DICTIONARY)
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
  NOTREACHED();
}
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_DICTIONARY)
void Visit(IdentifiableTokenBuilder& builder,
           const BooleanOrMediaTrackConstraints& constraint) {
  if (constraint.IsBoolean()) {
    builder.AddToken(constraint.GetAsBoolean());
    return;
  }
  if (constraint.IsMediaTrackConstraints()) {
    const MediaTrackConstraints* constraints =
        constraint.GetAsMediaTrackConstraints();
    if (constraints) {
      if (constraints->hasAdvanced()) {
        for (const auto& advanced : constraints->advanced()) {
          Visit(builder, *advanced);
        }
        return;
      }
    }
  }
  builder.AddToken(IdentifiableToken());
}
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_DICTIONARY)

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
      IdentifiabilityStudySettings::Get()->ShouldSample(surface)) {
    IdentifiabilityMetricBuilder(context->UkmSourceID())
        .Set(surface, token)
        .Record(context->UkmRecorder());
  }
}

}  // namespace blink
