// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/input_device_info.h"

#include <algorithm>

#include "build/build_config.h"
#include "media/base/sample_format.h"
#include "media/capture/mojom/video_capture_types.mojom-shared.h"
#include "media/webrtc/constants.h"
#include "third_party/blink/public/mojom/mediastream/media_devices.mojom-blink.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_track.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_double_range.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_long_range.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_settings_range.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_capabilities.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util_video_device.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_processor_options.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"

namespace blink {

InputDeviceInfo::InputDeviceInfo(const String& device_id,
                                 const String& label,
                                 const String& group_id,
                                 mojom::blink::MediaDeviceType device_type)
    : MediaDeviceInfo(device_id, label, group_id, device_type) {}

void InputDeviceInfo::SetVideoInputCapabilities(
    mojom::blink::VideoInputDeviceCapabilitiesPtr video_input_capabilities) {
  DCHECK_EQ(deviceId(), video_input_capabilities->device_id);
  // TODO(c.padhi): Merge the common logic below with
  // ComputeCapabilitiesForVideoSource() in media_stream_constraints_util.h, see
  // https://crbug.com/821668.
  platform_capabilities_.facing_mode =
      ToPlatformFacingMode(video_input_capabilities->facing_mode);
  if (!video_input_capabilities->formats.empty()) {
    int max_width = 1;
    int max_height = 1;
    float min_frame_rate = 1.0f;
    float max_frame_rate = min_frame_rate;
    for (const auto& format : video_input_capabilities->formats) {
      max_width = std::max(max_width, format.frame_size.width());
      max_height = std::max(max_height, format.frame_size.height());
      max_frame_rate = std::max(max_frame_rate, format.frame_rate);
    }
    platform_capabilities_.width = {1, static_cast<uint32_t>(max_width)};
    platform_capabilities_.height = {1, static_cast<uint32_t>(max_height)};
    platform_capabilities_.aspect_ratio = {1.0 / max_height,
                                           static_cast<double>(max_width)};
    platform_capabilities_.frame_rate = {min_frame_rate, max_frame_rate};
  }
  platform_capabilities_.is_available =
      !video_input_capabilities->availability ||
      (*video_input_capabilities->availability ==
       media::mojom::CameraAvailability::kAvailable);
}

void InputDeviceInfo::SetAudioInputCapabilities(
    mojom::blink::AudioInputDeviceCapabilitiesPtr audio_input_capabilities) {
  DCHECK_EQ(deviceId(), audio_input_capabilities->device_id);

  if (audio_input_capabilities->is_valid) {
    platform_capabilities_.channel_count = {1,
                                            audio_input_capabilities->channels};

    platform_capabilities_.sample_rate = {
        std::min(media::WebRtcAudioProcessingSampleRateHz(),
                 audio_input_capabilities->sample_rate),
        std::max(media::WebRtcAudioProcessingSampleRateHz(),
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
  // should be returned. Also, if the device is marked as not available, it
  // does not expose any capabilities.
  if (label().empty() || !platform_capabilities_.is_available) {
    return capabilities;
  }

  capabilities->setDeviceId(deviceId());
  capabilities->setGroupId(groupId());

  if (DeviceType() == mojom::blink::MediaDeviceType::kMediaAudioInput) {
    capabilities->setEchoCancellation({true, false});
    capabilities->setAutoGainControl({true, false});
    capabilities->setNoiseSuppression({true, false});
    capabilities->setVoiceIsolation({true, false});
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

  if (DeviceType() == mojom::blink::MediaDeviceType::kMediaVideoInput) {
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
      case MediaStreamTrackPlatform::FacingMode::kUser:
        facing_mode.push_back("user");
        break;
      case MediaStreamTrackPlatform::FacingMode::kEnvironment:
        facing_mode.push_back("environment");
        break;
      case MediaStreamTrackPlatform::FacingMode::kLeft:
        facing_mode.push_back("left");
        break;
      case MediaStreamTrackPlatform::FacingMode::kRight:
        facing_mode.push_back("right");
        break;
      case MediaStreamTrackPlatform::FacingMode::kNone:
        break;
    }
    capabilities->setFacingMode(facing_mode);
    capabilities->setResizeMode({WebMediaStreamTrack::kResizeModeNone,
                                 WebMediaStreamTrack::kResizeModeRescale});
  }
  return capabilities;
}

}  // namespace blink
