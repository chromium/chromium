// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_capture_element_constraints.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_html_media_stream_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_constraint_set.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_boolean_constrainbooleanordomstringparameters_string.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_boolean_constrainbooleanparameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_boolean_constraindoublerange_double.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_constraindomstringparameters_string_stringsequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_constraindoublerange_double.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_constrainlongrange_long.h"
#include "third_party/blink/renderer/core/html/html_camera_element.h"
#include "third_party/blink/renderer/core/html/html_capability_element_metrics_util.h"
#include "third_party/blink/renderer/core/html/html_media_track_element_base.h"
#include "third_party/blink/renderer/core/html/html_microphone_element.h"
#include "third_party/blink/renderer/core/html/html_user_media_element.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

namespace {

// Keep basic properties, strip out 'ideal', 'exact', 'min', 'max' ranges.
// Guaranteed to return a non-null MediaTrackConstraints object.
MediaTrackConstraints* SanitizeTrackConstraints(
    const MediaTrackConstraintSet* constraints) {
  MediaTrackConstraints* sanitized = MediaTrackConstraints::Create();
  if (!constraints) {
    return sanitized;
  }

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

}  // namespace

const char MediaCaptureElementConstraints::kSupplementName[] =
    "MediaCaptureElementConstraints";

MediaCaptureElementConstraints& MediaCaptureElementConstraints::From(
    HTMLMediaCaptureElementBase& element) {
  MediaCaptureElementConstraints* supplement =
      Supplement<HTMLMediaCaptureElementBase>::From<
          MediaCaptureElementConstraints>(element);
  if (!supplement) {
    supplement = MakeGarbageCollected<MediaCaptureElementConstraints>(element);
    ProvideTo(element, supplement);
  }
  return *supplement;
}

MediaCaptureElementConstraints::MediaCaptureElementConstraints(
    HTMLMediaCaptureElementBase& element)
    : Supplement<HTMLMediaCaptureElementBase>(element) {}

void MediaCaptureElementConstraints::Trace(Visitor* visitor) const {
  visitor->Trace(video_constraints_);
  visitor->Trace(audio_constraints_);
  Supplement<HTMLMediaCaptureElementBase>::Trace(visitor);
}

void MediaCaptureElementConstraints::setConstraints(
    HTMLUserMediaElement& element,
    const HTMLMediaStreamConstraints* constraints) {
  MediaCaptureElementConstraints& self = From(element);
  if (self.DidSetConstraints()) {
    RecordCapabilityElementSetConstraintsRepeatedCall(element.TagQName(),
                                                      RepeatedCall(true));
    return;
  }
  RecordCapabilityElementSetConstraintsRepeatedCall(element.TagQName(),
                                                    RepeatedCall(false));

  MediaTrackConstraints* video_constraints = SanitizeTrackConstraints(
      (constraints && constraints->hasVideo()) ? constraints->video()
                                               : nullptr);
  MediaTrackConstraints* audio_constraints = SanitizeTrackConstraints(
      (constraints && constraints->hasAudio()) ? constraints->audio()
                                               : nullptr);

  self.SetConstraints(video_constraints, audio_constraints);
}

void MediaCaptureElementConstraints::setConstraints(
    HTMLMediaTrackElementBase& element,
    const MediaTrackConstraintSet* constraints) {
  MediaCaptureElementConstraints& self = From(element);
  if (self.DidSetConstraints()) {
    RecordCapabilityElementSetConstraintsRepeatedCall(element.TagQName(),
                                                      RepeatedCall(true));
    return;
  }
  RecordCapabilityElementSetConstraintsRepeatedCall(element.TagQName(),
                                                    RepeatedCall(false));

  MediaTrackConstraints* sanitized_track =
      SanitizeTrackConstraints(constraints);

  if (element.IsHTMLCameraElement()) {
    self.SetConstraints(/*video_constraints=*/sanitized_track,
                        /*audio_constraints=*/nullptr);
  } else if (element.IsHTMLMicrophoneElement()) {
    self.SetConstraints(/*video_constraints=*/nullptr,
                        /*audio_constraints=*/sanitized_track);
  }
}

}  // namespace blink
