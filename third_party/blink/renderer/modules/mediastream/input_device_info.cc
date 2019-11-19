// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/input_device_info.h"

#include <algorithm>

#include "build/build_config.h"
#include "media/base/sample_format.h"
#include "third_party/blink/public/mojom/mediastream/media_devices.mojom-blink.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_track_capabilities.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_processor_options.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"

namespace blink {

namespace {

// TODO(c.padhi): Merge this method with ToWebFacingMode() in
// media_stream_constraints_util_video_device.h, see https://crbug.com/821668.
WebMediaStreamTrack::FacingMode ToWebFacingMode(mojom::FacingMode facing_mode) {
  switch (facing_mode) {
    case mojom::FacingMode::NONE:
      return WebMediaStreamTrack::FacingMode::kNone;
    case mojom::FacingMode::USER:
      return WebMediaStreamTrack::FacingMode::kUser;
    case mojom::FacingMode::ENVIRONMENT:
      return WebMediaStreamTrack::FacingMode::kEnvironment;
    case mojom::FacingMode::LEFT:
      return WebMediaStreamTrack::FacingMode::kLeft;
    case mojom::FacingMode::RIGHT:
      return WebMediaStreamTrack::FacingMode::kRight;
  }
  NOTREACHED();
  return WebMediaStreamTrack::FacingMode::kNone;
}

}  // namespace

InputDeviceInfo* InputDeviceInfo::Create(const String& device_id,
                                         const String& label,
                                         const String& group_id,
                                         MediaDeviceType device_type) {
  return MakeGarbageCollected<InputDeviceInfo>(device_id, label, group_id,
                                               device_type);
}

InputDeviceInfo::InputDeviceInfo(const String& device_id,
                                 const String& label,
                                 const String& group_id,
                                 MediaDeviceType device_type)
    : MediaDeviceInfo(device_id, label, group_id, device_type) {}

void InputDeviceInfo::SetVideoInputCapabilities(
    mojom::blink::VideoInputDeviceCapabilitiesPtr video_input_capabilities) {
  DCHECK_EQ(deviceId(), video_input_capabilities->device_id);
  // TODO(c.padhi): Merge the common logic below with
  // ComputeCapabilitiesForVideoSource() in media_stream_constraints_util.h, see
  // https://crbug.com/821668.
  platform_capabilities_.facing_mode =
      ToWebFacingMode(video_input_capabilities->facing_mode);
  if (!video_input_capabilities->formats.IsEmpty()) {
    int max_width = 1;
    int max_height = 1;
    float min_frame_rate = 1.0f;
    float max_frame_rate = min_frame_rate;
    for (const auto& format : video_input_capabilities->formats) {
      max_width = std::max(max_width, format.frame_size.width());
      max_height = std::max(max_height, format.frame_size.height());
      max_frame_rate = std::max(max_frame_rate, format.frame_rate);
    }
    platform_capabilities_.width = {1, max_width};
    platform_capabilities_.height = {1, max_height};
    platform_capabilities_.aspect_ratio = {1.0 / max_height,
                                           static_cast<double>(max_width)};
    platform_capabilities_.frame_rate = {min_frame_rate, max_frame_rate};
  }
}

void InputDeviceInfo::SetAudioInputCapabilities(
    mojom::blink::AudioInputDeviceCapabilitiesPtr audio_input_capabilities) {
  DCHECK_EQ(deviceId(), audio_input_capabilities->device_id);

  if (audio_input_capabilities->is_valid) {
    platform_capabilities_.channel_count = {1,
                                            audio_input_capabilities->channels};

    platform_capabilities_.sample_rate = {
        std::min(kAudioProcessingSampleRate,
                 audio_input_capabilities->sample_rate),
        std::max(kAudioProcessingSampleRate,
                 audio_input_capabilities->sample_rate)};
    double fallback_latency = kFallbackAudioLatencyMs / 1000;
    platform_capabilities_.latency = {
        std::min(fallback_latency,
                 audio_input_capabilities->latency.InSecondsF()),
        std::max(fallback_latency,
                 audio_input_capabilities->latency.InSecondsF())};
  }
}

MediaTrackCapabilities* InputDeviceInfo::getCapabilities() const {
  MediaTrackCapabilities* capabilities = MediaTrackCapabilities::Create();

  // If label is null, permissions have not been given and no capabilities
  // should be returned.
  if (label().IsEmpty())
    return capabilities;

  capabilities->setDeviceId(deviceId());
  capabilities->setGroupId(groupId());

  if (DeviceType() == MediaDeviceType::MEDIA_AUDIO_INPUT) {
    capabilities->setEchoCancellation({true, false});
    capabilities->setAutoGainControl({true, false});
    capabilities->setNoiseSuppression({true, false});
    // Sample size.
    LongRange* sample_size = LongRange::Create();
    sample_size->setMin(
        media::SampleFormatToBitsPerChannel(media::kSampleFormatS16));
    sample_size->setMax(
        media::SampleFormatToBitsPerChannel(media::kSampleFormatS16));
    capabilities->setSampleSize(sample_size);
    // Channel count.
    if (!platform_capabilities_.channel_count.empty()) {
      LongRange* channel_count = LongRange::Create();
      channel_count->setMin(platform_capabilities_.channel_count[0]);
      channel_count->setMax(platform_capabilities_.channel_count[1]);
      capabilities->setChannelCount(channel_count);
    }
    // Sample rate.
    if (!platform_capabilities_.sample_rate.empty()) {
      LongRange* sample_rate = LongRange::Create();
      sample_rate->setMin(platform_capabilities_.sample_rate[0]);
      sample_rate->setMax(platform_capabilities_.sample_rate[1]);
      capabilities->setSampleRate(sample_rate);
    }
    // Latency.
    if (!platform_capabilities_.latency.empty()) {
      DoubleRange* latency = DoubleRange::Create();
      latency->setMin(platform_capabilities_.latency[0]);
      latency->setMax(platform_capabilities_.latency[1]);
      capabilities->setLatency(latency);
    }
  }

  if (DeviceType() == MediaDeviceType::MEDIA_VIDEO_INPUT) {
    if (!platform_capabilities_.width.empty()) {
      LongRange* width = LongRange::Create();
      width->setMin(platform_capabilities_.width[0]);
      width->setMax(platform_capabilities_.width[1]);
      capabilities->setWidth(width);
    }
    if (!platform_capabilities_.height.empty()) {
      LongRange* height = LongRange::Create();
      height->setMin(platform_capabilities_.height[0]);
      height->setMax(platform_capabilities_.height[1]);
      capabilities->setHeight(height);
    }
    if (!platform_capabilities_.aspect_ratio.empty()) {
      DoubleRange* aspect_ratio = DoubleRange::Create();
      aspect_ratio->setMin(platform_capabilities_.aspect_ratio[0]);
      aspect_ratio->setMax(platform_capabilities_.aspect_ratio[1]);
      capabilities->setAspectRatio(aspect_ratio);
    }
    if (!platform_capabilities_.frame_rate.empty()) {
      DoubleRange* frame_rate = DoubleRange::Create();
      frame_rate->setMin(platform_capabilities_.frame_rate[0]);
      frame_rate->setMax(platform_capabilities_.frame_rate[1]);
      capabilities->setFrameRate(frame_rate);
    }
    Vector<String> facing_mode;
    switch (platform_capabilities_.facing_mode) {
      case WebMediaStreamTrack::FacingMode::kUser:
        facing_mode.push_back("user");
        break;
      case WebMediaStreamTrack::FacingMode::kEnvironment:
        facing_mode.push_back("environment");
        break;
      case WebMediaStreamTrack::FacingMode::kLeft:
        facing_mode.push_back("left");
        break;
      case WebMediaStreamTrack::FacingMode::kRight:
        facing_mode.push_back("right");
        break;
      case WebMediaStreamTrack::FacingMode::kNone:
        break;
    }
    capabilities->setFacingMode(facing_mode);
    capabilities->setResizeMode({WebMediaStreamTrack::kResizeModeNone,
                                 WebMediaStreamTrack::kResizeModeRescale});
  }
  return capabilities;
}

}  // namespace blink
