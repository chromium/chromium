// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_registry.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "media/base/audio_parameters.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/modules/mediastream/video_track_adapter_settings.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"

namespace blink {

namespace {

const char kTestStreamLabel[] = "stream_label";

class MockCDQualityAudioSource : public MediaStreamAudioSource {
 public:
  MockCDQualityAudioSource()
      : MediaStreamAudioSource(scheduler::GetSingleThreadTaskRunnerForTesting(),
                               true) {
    SetFormat(media::AudioParameters(
        media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
        media::ChannelLayoutConfig::Stereo(),
        media::AudioParameters::kAudioCDSampleRate,
        media::AudioParameters::kAudioCDSampleRate / 100));
    SetDevice(MediaStreamDevice(
        mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, "mock_audio_device_id",
        "Mock audio device", media::AudioParameters::kAudioCDSampleRate,
        media::ChannelLayoutConfig::Stereo(),
        media::AudioParameters::kAudioCDSampleRate / 100));
  }

  MockCDQualityAudioSource(const MockCDQualityAudioSource&) = delete;
  MockCDQualityAudioSource& operator=(const MockCDQualityAudioSource&) = delete;
};

}  // namespace

MockMediaStreamRegistry::MockMediaStreamRegistry() {}

void MockMediaStreamRegistry::Init() {
  MediaStreamComponentVector audio_descriptions, video_descriptions;
  String label(kTestStreamLabel);
  descriptor_ = MakeGarbageCollected<MediaStreamDescriptor>(
      label, audio_descriptions, video_descriptions);
}

MockMediaStreamVideoSource* MockMediaStreamRegistry::AddVideoTrack(
    const String& track_id,
    const VideoTrackAdapterSettings& adapter_settings,
    const std::optional<bool>& noise_reduction,
    bool is_screencast,
    double min_frame_rate) {
  auto native_source = std::make_unique<MockMediaStreamVideoSource>();
  auto* native_source_ptr = native_source.get();
  auto* source = MakeGarbageCollected<MediaStreamSource>(
      "mock video source id", MediaStreamSource::kTypeVideo,
      "mock video source name", false /* remote */, std::move(native_source));

  auto* component = MakeGarbageCollected<MediaStreamComponentImpl>(
      track_id, source,
      std::make_unique<MediaStreamVideoTrack>(
          native_source_ptr, adapter_settings, noise_reduction, is_screencast,
          min_frame_rate, nullptr /* device_settings */,
          false /* pan_tilt_zoom_allowed */,
          MediaStreamVideoSource::ConstraintsOnceCallback(),
          true /* enabled */));
  descriptor_->AddRemoteTrack(component);
  return native_source_ptr;
}

MockMediaStreamVideoSource* MockMediaStreamRegistry::AddVideoTrack(
    const String& track_id) {
  return AddVideoTrack(track_id, VideoTrackAdapterSettings(),
                       std::optional<bool>(), false /* is_screncast */,
                       0.0 /* min_frame_rate */);
}

void MockMediaStreamRegistry::AddAudioTrack(const String& track_id) {
  auto audio_source = std::make_unique<MockCDQualityAudioSource>();
  auto* audio_source_ptr = audio_source.get();
  auto* source = MakeGarbageCollected<MediaStreamSource>(
      "mock audio source id", MediaStreamSource::kTypeAudio,
      "mock audio source name", false /* remote */, std::move(audio_source));

  auto* component = MakeGarbageCollected<MediaStreamComponentImpl>(
      source,
      std::make_unique<MediaStreamAudioTrack>(true /* is_local_track */));
  CHECK(audio_source_ptr->ConnectToInitializedTrack(component));

  descriptor_->AddRemoteTrack(component);
}

}  // namespace blink
