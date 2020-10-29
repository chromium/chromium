// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/identifiability_metrics.h"

#include "base/callback.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"
#include "third_party/blink/renderer/bindings/modules/v8/boolean_or_constrain_boolean_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/boolean_or_double_or_constrain_double_range.h"
#include "third_party/blink/renderer/bindings/modules/v8/boolean_or_media_track_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/double_or_constrain_double_range.h"
#include "third_party/blink/renderer/bindings/modules/v8/long_or_constrain_long_range.h"
#include "third_party/blink/renderer/bindings/modules/v8/point_2d_sequence_or_constrain_point_2d_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/string_or_string_sequence_or_constrain_dom_string_parameters.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/mediastream/media_track_constraint_set.h"
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

void Visit(IdentifiableTokenBuilder& builder,
           const DoubleOrConstrainDoubleRange& d) {
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

void Visit(IdentifiableTokenBuilder& builder,
           const LongOrConstrainLongRange& l) {
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

void Visit(IdentifiableTokenBuilder& builder,
           const StringOrStringSequenceOrConstrainDOMStringParameters& s) {
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

void Visit(IdentifiableTokenBuilder& builder,
           const BooleanOrConstrainBooleanParameters& b) {
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

void Visit(IdentifiableTokenBuilder& builder,
           const HeapVector<Member<Point2D>>& points) {
  for (const auto& point : points) {
    builder.AddToken(point->hasX() ? point->x() : IdentifiableToken());
    builder.AddToken(point->hasY() ? point->y() : IdentifiableToken());
  }
}

void Visit(IdentifiableTokenBuilder& builder,
           const Point2DSequenceOrConstrainPoint2DParameters& x) {
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

void Visit(IdentifiableTokenBuilder& builder,
           const MediaTrackConstraintSet& set) {
  Visit(builder, set.width());
  Visit(builder, set.height());
  Visit(builder, set.aspectRatio());
  Visit(builder, set.frameRate());
  Visit(builder, set.facingMode());
  Visit(builder, set.sampleRate());
  Visit(builder, set.sampleSize());
  Visit(builder, set.echoCancellation());
  Visit(builder, set.autoGainControl());
  Visit(builder, set.latency());
  Visit(builder, set.channelCount());
  Visit(builder, set.videoKind());
  Visit(builder, set.whiteBalanceMode());
  Visit(builder, set.exposureMode());
  Visit(builder, set.focusMode());
  Visit(builder, set.pointsOfInterest());
  Visit(builder, set.exposureCompensation());
  Visit(builder, set.exposureTime());
  Visit(builder, set.colorTemperature());
  Visit(builder, set.iso());
  Visit(builder, set.brightness());
  Visit(builder, set.contrast());
  Visit(builder, set.saturation());
  Visit(builder, set.sharpness());
  Visit(builder, set.focusDistance());
  Visit(builder, set.pan());
  Visit(builder, set.tilt());
  Visit(builder, set.zoom());
  Visit(builder, set.torch());
}

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
