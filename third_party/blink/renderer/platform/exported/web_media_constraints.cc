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

#include "third_party/blink/public/platform/web_media_constraints.h"

#include <math.h>
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
  if (!emit)
    return;
  if (builder.length() > 1)
    builder.Append(", ");
  builder.Append(name);
  builder.Append(": ");
  builder.AppendNumber(value);
}

void MaybeEmitNamedBoolean(StringBuilder& builder,
                           bool emit,
                           const char* name,
                           bool value) {
  if (!emit)
    return;
  if (builder.length() > 1)
    builder.Append(", ");
  builder.Append(name);
  builder.Append(": ");
  if (value)
    builder.Append("true");
  else
    builder.Append("false");
}

}  // namespace

const char kEchoCancellationTypeBrowser[] = "browser";
const char kEchoCancellationTypeAec3[] = "aec3";
const char kEchoCancellationTypeSystem[] = "system";

class WebMediaConstraintsPrivate final
    : public ThreadSafeRefCounted<WebMediaConstraintsPrivate> {
 public:
  static scoped_refptr<WebMediaConstraintsPrivate> Create();
  static scoped_refptr<WebMediaConstraintsPrivate> Create(
      const WebMediaTrackConstraintSet& basic,
      const WebVector<WebMediaTrackConstraintSet>& advanced);

  bool IsEmpty() const;
  const WebMediaTrackConstraintSet& Basic() const;
  const WebVector<WebMediaTrackConstraintSet>& Advanced() const;
  const String ToString() const;

 private:
  WebMediaConstraintsPrivate(
      const WebMediaTrackConstraintSet& basic,
      const WebVector<WebMediaTrackConstraintSet>& advanced);

  WebMediaTrackConstraintSet basic_;
  WebVector<WebMediaTrackConstraintSet> advanced_;
};

scoped_refptr<WebMediaConstraintsPrivate> WebMediaConstraintsPrivate::Create() {
  WebMediaTrackConstraintSet basic;
  WebVector<WebMediaTrackConstraintSet> advanced;
  return base::AdoptRef(new WebMediaConstraintsPrivate(basic, advanced));
}

scoped_refptr<WebMediaConstraintsPrivate> WebMediaConstraintsPrivate::Create(
    const WebMediaTrackConstraintSet& basic,
    const WebVector<WebMediaTrackConstraintSet>& advanced) {
  return base::AdoptRef(new WebMediaConstraintsPrivate(basic, advanced));
}

WebMediaConstraintsPrivate::WebMediaConstraintsPrivate(
    const WebMediaTrackConstraintSet& basic,
    const WebVector<WebMediaTrackConstraintSet>& advanced)
    : basic_(basic), advanced_(advanced) {}

bool WebMediaConstraintsPrivate::IsEmpty() const {
  // TODO(hta): When generating advanced constraints, make sure no empty
  // elements can be added to the m_advanced vector.
  return basic_.IsEmpty() && advanced_.empty();
}

const WebMediaTrackConstraintSet& WebMediaConstraintsPrivate::Basic() const {
  return basic_;
}

const WebVector<WebMediaTrackConstraintSet>&
WebMediaConstraintsPrivate::Advanced() const {
  return advanced_;
}

const String WebMediaConstraintsPrivate::ToString() const {
  StringBuilder builder;
  if (!IsEmpty()) {
    builder.Append('{');
    builder.Append(Basic().ToString());
    if (!Advanced().empty()) {
      if (builder.length() > 1)
        builder.Append(", ");
      builder.Append("advanced: [");
      bool first = true;
      for (const auto& constraint_set : Advanced()) {
        if (!first)
          builder.Append(", ");
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

bool LongConstraint::IsEmpty() const {
  return !has_min_ && !has_max_ && !has_exact_ && !has_ideal_;
}

WebString LongConstraint::ToString() const {
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

bool DoubleConstraint::IsEmpty() const {
  return !has_min_ && !has_max_ && !has_exact_ && !has_ideal_;
}

WebString DoubleConstraint::ToString() const {
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

bool StringConstraint::Matches(WebString value) const {
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

bool StringConstraint::IsEmpty() const {
  return exact_.empty() && ideal_.empty();
}

const WebVector<WebString>& StringConstraint::Exact() const {
  return exact_;
}

const WebVector<WebString>& StringConstraint::Ideal() const {
  return ideal_;
}

WebString StringConstraint::ToString() const {
  StringBuilder builder;
  builder.Append('{');
  if (!ideal_.empty()) {
    builder.Append("ideal: [");
    bool first = true;
    for (const auto& iter : ideal_) {
      if (!first)
        builder.Append(", ");
      builder.Append('"');
      builder.Append(iter);
      builder.Append('"');
      first = false;
    }
    builder.Append(']');
  }
  if (!exact_.empty()) {
    if (builder.length() > 1)
      builder.Append(", ");
    builder.Append("exact: [");
    bool first = true;
    for (const auto& iter : exact_) {
      if (!first)
        builder.Append(", ");
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

bool BooleanConstraint::IsEmpty() const {
  return !has_ideal_ && !has_exact_;
}

WebString BooleanConstraint::ToString() const {
  StringBuilder builder;
  builder.Append('{');
  MaybeEmitNamedBoolean(builder, has_exact_, "exact", Exact());
  MaybeEmitNamedBoolean(builder, has_ideal_, "ideal", Ideal());
  builder.Append('}');
  return builder.ToString();
}

WebMediaTrackConstraintSet::WebMediaTrackConstraintSet()
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
      echo_cancellation_type("echoCancellationType"),
      latency("latency"),
      channel_count("channelCount"),
      device_id("deviceId"),
      disable_local_echo("disableLocalEcho"),
      group_id("groupId"),
      video_kind("videoKind"),
      media_stream_source("mediaStreamSource"),
      render_to_associated_sink("chromeRenderToAssociatedSink"),
      goog_echo_cancellation("googEchoCancellation"),
      goog_experimental_echo_cancellation("googExperimentalEchoCancellation"),
      goog_auto_gain_control("autoGainControl"),
      goog_experimental_auto_gain_control("googExperimentalAutoGainControl"),
      goog_noise_suppression("noiseSuppression"),
      goog_highpass_filter("googHighpassFilter"),
      goog_experimental_noise_suppression("googExperimentalNoiseSuppression"),
      goog_audio_mirroring("googAudioMirroring"),
      goog_da_echo_cancellation("googDAEchoCancellation"),
      goog_noise_reduction("googNoiseReduction"),
      offer_to_receive_audio("offerToReceiveAudio"),
      offer_to_receive_video("offerToReceiveVideo"),
      voice_activity_detection("voiceActivityDetection"),
      ice_restart("iceRestart"),
      goog_use_rtp_mux("googUseRtpMux"),
      enable_dtls_srtp("enableDtlsSrtp"),
      enable_rtp_data_channels("enableRtpDataChannels"),
      enable_dscp("enableDscp"),
      enable_i_pv6("enableIPv6"),
      goog_enable_video_suspend_below_min_bitrate(
          "googEnableVideoSuspendBelowMinBitrate"),
      goog_num_unsignalled_recv_streams("googNumUnsignalledRecvStreams"),
      goog_combined_audio_video_bwe("googCombinedAudioVideoBwe"),
      goog_screencast_min_bitrate("googScreencastMinBitrate"),
      goog_cpu_overuse_detection("googCpuOveruseDetection"),
      goog_cpu_underuse_threshold("googCpuUnderuseThreshold"),
      goog_cpu_overuse_threshold("googCpuOveruseThreshold"),
      goog_cpu_underuse_encode_rsd_threshold(
          "googCpuUnderuseEncodeRsdThreshold"),
      goog_cpu_overuse_encode_rsd_threshold("googCpuOveruseEncodeRsdThreshold"),
      goog_cpu_overuse_encode_usage("googCpuOveruseEncodeUsage"),
      goog_high_start_bitrate("googHighStartBitrate"),
      goog_payload_padding("googPayloadPadding"),
      goog_latency_ms("latencyMs") {}

std::vector<const BaseConstraint*> WebMediaTrackConstraintSet::AllConstraints()
    const {
  const BaseConstraint* temp[] = {&width,
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
                                  &video_kind,
                                  &media_stream_source,
                                  &disable_local_echo,
                                  &render_to_associated_sink,
                                  &goog_echo_cancellation,
                                  &goog_experimental_echo_cancellation,
                                  &goog_auto_gain_control,
                                  &goog_experimental_auto_gain_control,
                                  &goog_noise_suppression,
                                  &goog_highpass_filter,
                                  &goog_experimental_noise_suppression,
                                  &goog_audio_mirroring,
                                  &goog_da_echo_cancellation,
                                  &goog_noise_reduction,
                                  &offer_to_receive_audio,
                                  &offer_to_receive_video,
                                  &voice_activity_detection,
                                  &ice_restart,
                                  &goog_use_rtp_mux,
                                  &enable_dtls_srtp,
                                  &enable_rtp_data_channels,
                                  &enable_dscp,
                                  &enable_i_pv6,
                                  &goog_enable_video_suspend_below_min_bitrate,
                                  &goog_num_unsignalled_recv_streams,
                                  &goog_combined_audio_video_bwe,
                                  &goog_screencast_min_bitrate,
                                  &goog_cpu_overuse_detection,
                                  &goog_cpu_underuse_threshold,
                                  &goog_cpu_overuse_threshold,
                                  &goog_cpu_underuse_encode_rsd_threshold,
                                  &goog_cpu_overuse_encode_rsd_threshold,
                                  &goog_cpu_overuse_encode_usage,
                                  &goog_high_start_bitrate,
                                  &goog_payload_padding,
                                  &goog_latency_ms};
  const int element_count = sizeof(temp) / sizeof(temp[0]);
  return std::vector<const BaseConstraint*>(&temp[0], &temp[element_count]);
}

bool WebMediaTrackConstraintSet::IsEmpty() const {
  for (auto* const constraint : AllConstraints()) {
    if (!constraint->IsEmpty())
      return false;
  }
  return true;
}

bool WebMediaTrackConstraintSet::HasMandatoryOutsideSet(
    const std::vector<std::string>& good_names,
    std::string& found_name) const {
  for (auto* const constraint : AllConstraints()) {
    if (constraint->HasMandatory()) {
      if (std::find(good_names.begin(), good_names.end(),
                    constraint->GetName()) == good_names.end()) {
        found_name = constraint->GetName();
        return true;
      }
    }
  }
  return false;
}

bool WebMediaTrackConstraintSet::HasMandatory() const {
  std::string dummy_string;
  return HasMandatoryOutsideSet(std::vector<std::string>(), dummy_string);
}

bool WebMediaTrackConstraintSet::HasMin() const {
  for (auto* const constraint : AllConstraints()) {
    if (constraint->HasMin())
      return true;
  }
  return false;
}

bool WebMediaTrackConstraintSet::HasExact() const {
  for (auto* const constraint : AllConstraints()) {
    if (constraint->HasExact())
      return true;
  }
  return false;
}

WebString WebMediaTrackConstraintSet::ToString() const {
  StringBuilder builder;
  bool first = true;
  for (auto* const constraint : AllConstraints()) {
    if (!constraint->IsEmpty()) {
      if (!first)
        builder.Append(", ");
      builder.Append(constraint->GetName());
      builder.Append(": ");
      builder.Append(constraint->ToString());
      first = false;
    }
  }
  return builder.ToString();
}

// WebMediaConstraints

void WebMediaConstraints::Assign(const WebMediaConstraints& other) {
  private_ = other.private_;
}

void WebMediaConstraints::Reset() {
  private_.Reset();
}

bool WebMediaConstraints::IsEmpty() const {
  return private_.IsNull() || private_->IsEmpty();
}

void WebMediaConstraints::Initialize() {
  DCHECK(IsNull());
  private_ = WebMediaConstraintsPrivate::Create();
}

void WebMediaConstraints::Initialize(
    const WebMediaTrackConstraintSet& basic,
    const WebVector<WebMediaTrackConstraintSet>& advanced) {
  DCHECK(IsNull());
  private_ = WebMediaConstraintsPrivate::Create(basic, advanced);
}

const WebMediaTrackConstraintSet& WebMediaConstraints::Basic() const {
  DCHECK(!IsNull());
  return private_->Basic();
}

const WebVector<WebMediaTrackConstraintSet>& WebMediaConstraints::Advanced()
    const {
  DCHECK(!IsNull());
  return private_->Advanced();
}

const WebString WebMediaConstraints::ToString() const {
  if (IsNull())
    return WebString("");
  return private_->ToString();
}

}  // namespace blink
