/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/mediastream/media_constraints.h"

#include <math.h>

#include "base/containers/contains.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"

namespace blink {

namespace {

template <typename T>
void MaybeEmitNamedValue(StringBuilder& builder,
                         bool emit,
                         const char* name,
                         T value) {
  if (!emit) {
    return;
  }
  if (builder.length() > 1) {
    builder.Append(", ");
  }
  builder.Append(name);
  builder.Append(": ");
  builder.AppendNumber(value);
}

void MaybeEmitNamedBoolean(StringBuilder& builder,
                           bool emit,
                           const char* name,
                           bool value) {
  if (!emit) {
    return;
  }
  if (builder.length() > 1) {
    builder.Append(", ");
  }
  builder.Append(name);
  builder.Append(": ");
  if (value) {
    builder.Append("true");
  } else {
    builder.Append("false");
  }
}

}  // namespace

class MediaConstraintsPrivate final
    : public ThreadSafeRefCounted<MediaConstraintsPrivate> {
 public:
  static scoped_refptr<MediaConstraintsPrivate> Create();
  static scoped_refptr<MediaConstraintsPrivate> Create(
      const MediaTrackConstraintSetPlatform& basic,
      const Vector<MediaTrackConstraintSetPlatform>& advanced);

  bool IsUnconstrained() const;
  const MediaTrackConstraintSetPlatform& Basic() const;
  MediaTrackConstraintSetPlatform& MutableBasic();
  const Vector<MediaTrackConstraintSetPlatform>& Advanced() const;
  const String ToString() const;

 private:
  MediaConstraintsPrivate(
      const MediaTrackConstraintSetPlatform& basic,
      const Vector<MediaTrackConstraintSetPlatform>& advanced);

  MediaTrackConstraintSetPlatform basic_;
  Vector<MediaTrackConstraintSetPlatform> advanced_;
};

scoped_refptr<MediaConstraintsPrivate> MediaConstraintsPrivate::Create() {
  MediaTrackConstraintSetPlatform basic;
  Vector<MediaTrackConstraintSetPlatform> advanced;
  return base::AdoptRef(new MediaConstraintsPrivate(basic, advanced));
}

scoped_refptr<MediaConstraintsPrivate> MediaConstraintsPrivate::Create(
    const MediaTrackConstraintSetPlatform& basic,
    const Vector<MediaTrackConstraintSetPlatform>& advanced) {
  return base::AdoptRef(new MediaConstraintsPrivate(basic, advanced));
}

MediaConstraintsPrivate::MediaConstraintsPrivate(
    const MediaTrackConstraintSetPlatform& basic,
    const Vector<MediaTrackConstraintSetPlatform>& advanced)
    : basic_(basic), advanced_(advanced) {}

bool MediaConstraintsPrivate::IsUnconstrained() const {
  // TODO(hta): When generating advanced constraints, make sure no empty
  // elements can be added to the m_advanced vector.
  return basic_.IsUnconstrained() && advanced_.empty();
}

const MediaTrackConstraintSetPlatform& MediaConstraintsPrivate::Basic() const {
  return basic_;
}

MediaTrackConstraintSetPlatform& MediaConstraintsPrivate::MutableBasic() {
  return basic_;
}

const Vector<MediaTrackConstraintSetPlatform>&
MediaConstraintsPrivate::Advanced() const {
  return advanced_;
}

const String MediaConstraintsPrivate::ToString() const {
  StringBuilder builder;
  if (!IsUnconstrained()) {
    builder.Append('{');
    builder.Append(Basic().ToString());
    if (!Advanced().empty()) {
      if (builder.length() > 1) {
        builder.Append(", ");
      }
      builder.Append("advanced: [");
      bool first = true;
      for (const auto& constraint_set : Advanced()) {
        if (!first) {
          builder.Append(", ");
        }
        builder.Append('{');
        builder.Append(constraint_set.ToString());
        builder.Append('}');
        first = false;
      }
      builder.Append(']');
    }
    builder.Append('}');
  }
  return builder.ToString();
}

// *Constraints

BaseConstraint::BaseConstraint(const char* name) : name_(name) {}

BaseConstraint::~BaseConstraint() = default;

bool BaseConstraint::HasMandatory() const {
  return HasMin() || HasMax() || HasExact();
}

LongConstraint::LongConstraint(const char* name)
    : BaseConstraint(name),
      min_(),
      max_(),
      exact_(),
      ideal_(),
      has_min_(false),
      has_max_(false),
      has_exact_(false),
      has_ideal_(false) {}

bool LongConstraint::Matches(int32_t value) const {
  if (has_min_ && value < min_) {
    return false;
  }
  if (has_max_ && value > max_) {
    return false;
  }
  if (has_exact_ && value != exact_) {
    return false;
  }
  return true;
}

bool LongConstraint::IsUnconstrained() const {
  return !has_min_ && !has_max_ && !has_exact_ && !has_ideal_;
}

void LongConstraint::ResetToUnconstrained() {
  *this = LongConstraint(GetName());
}

String LongConstraint::ToString() const {
  StringBuilder builder;
  builder.Append('{');
  MaybeEmitNamedValue(builder, has_min_, "min", min_);
  MaybeEmitNamedValue(builder, has_max_, "max", max_);
  MaybeEmitNamedValue(builder, has_exact_, "exact", exact_);
  MaybeEmitNamedValue(builder, has_ideal_, "ideal", ideal_);
  builder.Append('}');
  return builder.ToString();
}

const double DoubleConstraint::kConstraintEpsilon = 0.00001;

DoubleConstraint::DoubleConstraint(const char* name)
    : BaseConstraint(name),
      min_(),
      max_(),
      exact_(),
      ideal_(),
      has_min_(false),
      has_max_(false),
      has_exact_(false),
      has_ideal_(false) {}

bool DoubleConstraint::Matches(double value) const {
  if (has_min_ && value < min_ - kConstraintEpsilon) {
    return false;
  }
  if (has_max_ && value > max_ + kConstraintEpsilon) {
    return false;
  }
  if (has_exact_ &&
      fabs(static_cast<double>(value) - exact_) > kConstraintEpsilon) {
    return false;
  }
  return true;
}

bool DoubleConstraint::IsUnconstrained() const {
  return !has_min_ && !has_max_ && !has_exact_ && !has_ideal_;
}

void DoubleConstraint::ResetToUnconstrained() {
  *this = DoubleConstraint(GetName());
}

String DoubleConstraint::ToString() const {
  StringBuilder builder;
  builder.Append('{');
  MaybeEmitNamedValue(builder, has_min_, "min", min_);
  MaybeEmitNamedValue(builder, has_max_, "max", max_);
  MaybeEmitNamedValue(builder, has_exact_, "exact", exact_);
  MaybeEmitNamedValue(builder, has_ideal_, "ideal", ideal_);
  builder.Append('}');
  return builder.ToString();
}

StringConstraint::StringConstraint(const char* name)
    : BaseConstraint(name), exact_(), ideal_() {}

bool StringConstraint::Matches(String value) const {
  if (exact_.empty()) {
    return true;
  }
  for (const auto& choice : exact_) {
    if (value == choice) {
      return true;
    }
  }
  return false;
}

bool StringConstraint::IsUnconstrained() const {
  return exact_.empty() && ideal_.empty();
}

const Vector<String>& StringConstraint::Exact() const {
  return exact_;
}

const Vector<String>& StringConstraint::Ideal() const {
  return ideal_;
}

void StringConstraint::ResetToUnconstrained() {
  *this = StringConstraint(GetName());
}

String StringConstraint::ToString() const {
  StringBuilder builder;
  builder.Append('{');
  if (!ideal_.empty()) {
    builder.Append("ideal: [");
    bool first = true;
    for (const auto& iter : ideal_) {
      if (!first) {
        builder.Append(", ");
      }
      builder.Append('"');
      builder.Append(iter);
      builder.Append('"');
      first = false;
    }
    builder.Append(']');
  }
  if (!exact_.empty()) {
    if (builder.length() > 1) {
      builder.Append(", ");
    }
    builder.Append("exact: [");
    bool first = true;
    for (const auto& iter : exact_) {
      if (!first) {
        builder.Append(", ");
      }
      builder.Append('"');
      builder.Append(iter);
      builder.Append('"');
    }
    builder.Append(']');
  }
  builder.Append('}');
  return builder.ToString();
}

BooleanConstraint::BooleanConstraint(const char* name)
    : BaseConstraint(name),
      ideal_(false),
      exact_(false),
      has_ideal_(false),
      has_exact_(false) {}

bool BooleanConstraint::Matches(bool value) const {
  if (has_exact_ && static_cast<bool>(exact_) != value) {
    return false;
  }
  return true;
}

bool BooleanConstraint::IsUnconstrained() const {
  return !has_ideal_ && !has_exact_;
}

void BooleanConstraint::ResetToUnconstrained() {
  *this = BooleanConstraint(GetName());
}

String BooleanConstraint::ToString() const {
  StringBuilder builder;
  builder.Append('{');
  MaybeEmitNamedBoolean(builder, has_exact_, "exact", Exact());
  MaybeEmitNamedBoolean(builder, has_ideal_, "ideal", Ideal());
  builder.Append('}');
  return builder.ToString();
}

MediaTrackConstraintSetPlatform::MediaTrackConstraintSetPlatform()
    : width("width"),
      height("height"),
      aspect_ratio("aspectRatio"),
      frame_rate("frameRate"),
      facing_mode("facingMode"),
      resize_mode("resizeMode"),
      volume("volume"),
      sample_rate("sampleRate"),
      sample_size("sampleSize"),
      echo_cancellation("echoCancellation"),
      voice_isolation("voiceIsolation"),
      echo_cancellation_type("echoCancellationType"),
      latency("latency"),
      channel_count("channelCount"),
      device_id("deviceId"),
      disable_local_echo("disableLocalEcho"),
      suppress_local_audio_playback("suppressLocalAudioPlayback"),
      group_id("groupId"),
      display_surface("displaySurface"),
      exposure_compensation("exposureCompensation"),
      exposure_time("exposureTime"),
      color_temperature("colorTemperature"),
      iso("iso"),
      brightness("brightness"),
      contrast("contrast"),
      saturation("saturation"),
      sharpness("sharpness"),
      focus_distance("focusDistance"),
      pan("pan"),
      tilt("tilt"),
      zoom("zoom"),
      torch("torch"),
      background_blur("backgroundBlur"),
      background_segmentation_mask("backgroundSegmentationMask"),
      eye_gaze_correction("eyeGazeCorrection"),
      face_framing("faceFraming"),
      media_stream_source("mediaStreamSource"),
      render_to_associated_sink("chromeRenderToAssociatedSink"),
      goog_echo_cancellation("googEchoCancellation"),
      goog_experimental_echo_cancellation("googExperimentalEchoCancellation"),
      goog_auto_gain_control("autoGainControl"),
      goog_noise_suppression("noiseSuppression"),
      goog_highpass_filter("googHighpassFilter"),
      goog_experimental_noise_suppression("googExperimentalNoiseSuppression"),
      goog_audio_mirroring("googAudioMirroring"),
      goog_da_echo_cancellation("googDAEchoCancellation"),
      goog_noise_reduction("googNoiseReduction") {}

Vector<const BaseConstraint*> MediaTrackConstraintSetPlatform::AllConstraints()
    const {
  return {&width,
          &height,
          &aspect_ratio,
          &frame_rate,
          &facing_mode,
          &resize_mode,
          &volume,
          &sample_rate,
          &sample_size,
          &echo_cancellation,
          &echo_cancellation_type,
          &latency,
          &channel_count,
          &device_id,
          &group_id,
          &display_surface,
          &media_stream_source,
          &disable_local_echo,
          &suppress_local_audio_playback,
          &exposure_compensation,
          &exposure_time,
          &color_temperature,
          &iso,
          &brightness,
          &contrast,
          &saturation,
          &sharpness,
          &focus_distance,
          &pan,
          &tilt,
          &zoom,
          &torch,
          &background_blur,
          &background_segmentation_mask,
          &eye_gaze_correction,
          &face_framing,
          &render_to_associated_sink,
          &goog_echo_cancellation,
          &goog_experimental_echo_cancellation,
          &goog_auto_gain_control,
          &goog_noise_suppression,
          &voice_isolation,
          &goog_highpass_filter,
          &goog_experimental_noise_suppression,
          &goog_audio_mirroring,
          &goog_da_echo_cancellation,
          &goog_noise_reduction};
}

bool MediaTrackConstraintSetPlatform::IsUnconstrained() const {
  for (auto* const constraint : AllConstraints()) {
    if (!constraint->IsUnconstrained()) {
      return false;
    }
  }
  return true;
}

bool MediaTrackConstraintSetPlatform::HasMandatoryOutsideSet(
    const Vector<String>& good_names,
    String& found_name) const {
  for (auto* const constraint : AllConstraints()) {
    if (constraint->HasMandatory()) {
      if (!base::Contains(good_names, constraint->GetName())) {
        found_name = constraint->GetName();
        return true;
      }
    }
  }
  return false;
}

bool MediaTrackConstraintSetPlatform::HasMandatory() const {
  String dummy_string;
  return HasMandatoryOutsideSet(Vector<String>(), dummy_string);
}

bool MediaTrackConstraintSetPlatform::HasMin() const {
  for (auto* const constraint : AllConstraints()) {
    if (constraint->HasMin()) {
      return true;
    }
  }
  return false;
}

bool MediaTrackConstraintSetPlatform::HasExact() const {
  for (auto* const constraint : AllConstraints()) {
    if (constraint->HasExact()) {
      return true;
    }
  }
  return false;
}

String MediaTrackConstraintSetPlatform::ToString() const {
  StringBuilder builder;
  bool first = true;
  for (auto* const constraint : AllConstraints()) {
    if (constraint->IsPresent()) {
      if (!first) {
        builder.Append(", ");
      }
      builder.Append(constraint->GetName());
      builder.Append(": ");
      builder.Append(constraint->ToString());
      first = false;
    }
  }
  return builder.ToString();
}

// MediaConstraints

void MediaConstraints::Assign(const MediaConstraints& other) {
  private_ = other.private_;
}

MediaConstraints::MediaConstraints() = default;

MediaConstraints::MediaConstraints(const MediaConstraints& other) {
  Assign(other);
}

void MediaConstraints::Reset() {
  private_.Reset();
}

bool MediaConstraints::IsUnconstrained() const {
  return private_.IsNull() || private_->IsUnconstrained();
}

void MediaConstraints::Initialize() {
  DCHECK(IsNull());
  private_ = MediaConstraintsPrivate::Create();
}

void MediaConstraints::Initialize(
    const MediaTrackConstraintSetPlatform& basic,
    const Vector<MediaTrackConstraintSetPlatform>& advanced) {
  DCHECK(IsNull());
  private_ = MediaConstraintsPrivate::Create(basic, advanced);
}

const MediaTrackConstraintSetPlatform& MediaConstraints::Basic() const {
  DCHECK(!IsNull());
  return private_->Basic();
}

MediaTrackConstraintSetPlatform& MediaConstraints::MutableBasic() {
  DCHECK(!IsNull());
  return private_->MutableBasic();
}

const Vector<MediaTrackConstraintSetPlatform>& MediaConstraints::Advanced()
    const {
  DCHECK(!IsNull());
  return private_->Advanced();
}

const String MediaConstraints::ToString() const {
  if (IsNull()) {
    return String("");
  }
  return private_->ToString();
}

}  // namespace blink
