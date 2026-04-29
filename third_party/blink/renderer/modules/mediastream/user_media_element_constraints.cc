// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/user_media_element_constraints.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_html_media_stream_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_constraint_set.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_boolean_constrainbooleanordomstringparameters_string.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_boolean_constrainbooleanparameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_boolean_constraindoublerange_double.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_constraindomstringparameters_string_stringsequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_constraindoublerange_double.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_constrainlongrange_long.h"
#include "third_party/blink/renderer/core/html/html_user_media_element.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

const char UserMediaElementConstraints::kSupplementName[] =
    "UserMediaElementConstraints";

// Keep the most basic constraints, strip out 'ideal', 'exact', 'min', 'max'
// from all properties
MediaTrackConstraints* SanitizeTrackConstraints(
    const MediaTrackConstraintSet* constraints) {
  if (!constraints) {
    return nullptr;
  }

  MediaTrackConstraints* sanitized = MediaTrackConstraints::Create();

  // 1. Video properties
  // Longs
  if (constraints->hasWidth() && constraints->width()->IsLong()) {
    sanitized->setWidth(constraints->width());
  }
  if (constraints->hasHeight() && constraints->height()->IsLong()) {
    sanitized->setHeight(constraints->height());
  }

  // Doubles
  if (constraints->hasAspectRatio() && constraints->aspectRatio()->IsDouble()) {
    sanitized->setAspectRatio(constraints->aspectRatio());
  }
  if (constraints->hasFrameRate() && constraints->frameRate()->IsDouble()) {
    sanitized->setFrameRate(constraints->frameRate());
  }

  // Strings / String Sequences
  if (constraints->hasFacingMode() &&
      constraints->facingMode()->IsV8UnionStringOrStringSequence()) {
    sanitized->setFacingMode(constraints->facingMode());
  }
  if (constraints->hasResizeMode() &&
      constraints->resizeMode()->IsV8UnionStringOrStringSequence()) {
    sanitized->setResizeMode(constraints->resizeMode());
  }

  // 2. Audio properties
  // Longs
  if (constraints->hasChannelCount() && constraints->channelCount()->IsLong()) {
    sanitized->setChannelCount(constraints->channelCount());
  }
  if (constraints->hasSampleSize() && constraints->sampleSize()->IsLong()) {
    sanitized->setSampleSize(constraints->sampleSize());
  }
  if (constraints->hasSampleRate() && constraints->sampleRate()->IsLong()) {
    sanitized->setSampleRate(constraints->sampleRate());
  }

  // Doubles
  if (constraints->hasLatency() && constraints->latency()->IsDouble()) {
    sanitized->setLatency(constraints->latency());
  }

  // Standard Booleans
  if (constraints->hasAutoGainControl() &&
      constraints->autoGainControl()->IsBoolean()) {
    sanitized->setAutoGainControl(constraints->autoGainControl());
  }
  if (constraints->hasEchoCancellation() &&
      constraints->echoCancellation()->IsBoolean()) {
    sanitized->setEchoCancellation(constraints->echoCancellation());
  }
  if (constraints->hasNoiseSuppression() &&
      constraints->noiseSuppression()->IsBoolean()) {
    sanitized->setNoiseSuppression(constraints->noiseSuppression());
  }
  if (constraints->hasVoiceIsolation() &&
      constraints->voiceIsolation()->IsBoolean()) {
    sanitized->setVoiceIsolation(constraints->voiceIsolation());
  }

  // 3. Shared properties
  if (constraints->hasDeviceId() &&
      constraints->deviceId()->IsV8UnionStringOrStringSequence()) {
    sanitized->setDeviceId(constraints->deviceId());
  }
  if (constraints->hasGroupId() &&
      constraints->groupId()->IsV8UnionStringOrStringSequence()) {
    sanitized->setGroupId(constraints->groupId());
  }

  return sanitized;
}

UserMediaElementConstraints& UserMediaElementConstraints::From(
    HTMLUserMediaElement& element) {
  UserMediaElementConstraints* supplement =
      Supplement<HTMLUserMediaElement>::From<UserMediaElementConstraints>(
          element);
  if (!supplement) {
    supplement = MakeGarbageCollected<UserMediaElementConstraints>(element);
    ProvideTo(element, supplement);
  }
  return *supplement;
}

UserMediaElementConstraints::UserMediaElementConstraints(
    HTMLUserMediaElement& element)
    : Supplement<HTMLUserMediaElement>(element) {}

void UserMediaElementConstraints::Trace(Visitor* visitor) const {
  visitor->Trace(constraints_);
  Supplement<HTMLUserMediaElement>::Trace(visitor);
}

void UserMediaElementConstraints::setConstraints(
    HTMLUserMediaElement& element,
    const HTMLMediaStreamConstraints* constraints) {
  UserMediaElementConstraints& self = From(element);
  if (self.did_set_constraints_) {
    return;
  }

  HTMLMediaStreamConstraints* sanitized_constraints =
      HTMLMediaStreamConstraints::Create();

  if (constraints->hasVideo()) {
    sanitized_constraints->setVideo(
        SanitizeTrackConstraints(constraints->video()));
  }

  if (constraints->hasAudio()) {
    sanitized_constraints->setAudio(
        SanitizeTrackConstraints(constraints->audio()));
  }

  self.SetConstraints(sanitized_constraints);
  self.did_set_constraints_ = true;
  element.OnConstraintsSet(sanitized_constraints->hasVideo(),
                           sanitized_constraints->hasAudio());
}

}  // namespace blink
