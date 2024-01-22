// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/test/transfer_test_utils.h"

#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_capturer_source.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_video_capturer_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"

namespace blink {

void SetFromTransferredStateImplForTesting(
    MediaStreamTrack::FromTransferredStateImplForTesting impl) {
  MediaStreamTrack::GetFromTransferredStateImplForTesting() = std::move(impl);
}

ScopedMockMediaStreamTrackFromTransferredState::
    ScopedMockMediaStreamTrackFromTransferredState() {
  SetFromTransferredStateImplForTesting(
      WTF::BindRepeating(&ScopedMockMediaStreamTrackFromTransferredState::Impl,
                         // The destructor removes this callback.
                         WTF::Unretained(this)));
}
ScopedMockMediaStreamTrackFromTransferredState::
    ~ScopedMockMediaStreamTrackFromTransferredState() {
  SetFromTransferredStateImplForTesting(base::NullCallback());
}

MediaStreamTrack* ScopedMockMediaStreamTrackFromTransferredState::Impl(
    const MediaStreamTrack::TransferredValues& data) {
  last_argument = data;
  return return_value;
}

MediaStreamComponent* MakeTabCaptureVideoComponentForTest(
    LocalFrame* frame,
    base::UnguessableToken session_id) {
  auto mock_source = std::make_unique<MediaStreamVideoCapturerSource>(
      frame->GetTaskRunner(TaskType::kInternalMediaRealTime), frame,
      MediaStreamVideoCapturerSource::SourceStoppedCallback(),
      std::make_unique<MockVideoCapturerSource>());
  auto platform_track = std::make_unique<MediaStreamVideoTrack>(
      mock_source.get(),
      WebPlatformMediaStreamSource::ConstraintsOnceCallback(),
      /*enabled=*/true);

  MediaStreamDevice device(mojom::blink::MediaStreamType::DISPLAY_VIDEO_CAPTURE,
                           "device_id", "device_name");
  device.set_session_id(session_id);
  device.display_media_info = media::mojom::DisplayMediaInformation::New(
      media::mojom::DisplayCaptureSurfaceType::BROWSER,
      /*logical_surface=*/true, media::mojom::CursorCaptureType::NEVER,
      /*capture_handle=*/nullptr,
      /*initial_zoom_level=*/100);
  mock_source->SetDevice(device);
  MediaStreamSource* source = MakeGarbageCollected<MediaStreamSource>(
      "test_id", MediaStreamSource::StreamType::kTypeVideo, "test_name",
      /*remote=*/false, std::move(mock_source));
  MediaStreamComponent* component =
      MakeGarbageCollected<MediaStreamComponentImpl>("component_id", source,
                                                     std::move(platform_track));
  component->SetContentHint(WebMediaStreamTrack::ContentHintType::kVideoMotion);
  return component;
}

MediaStreamComponent* MakeTabCaptureAudioComponentForTest(
    base::UnguessableToken session_id) {
  auto mock_source = std::make_unique<MediaStreamAudioSource>(
      blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
      /*is_local_source=*/true);
  auto platform_track =
      std::make_unique<MediaStreamAudioTrack>(/*is_local_track=*/true);

  MediaStreamDevice device(mojom::blink::MediaStreamType::DISPLAY_AUDIO_CAPTURE,
                           "device_id", "device_name");
  device.set_session_id(session_id);
  device.display_media_info = media::mojom::DisplayMediaInformation::New(
      media::mojom::DisplayCaptureSurfaceType::BROWSER,
      /*logical_surface=*/true, media::mojom::CursorCaptureType::NEVER,
      /*capture_handle=*/nullptr,
      /*initial_zoom_level=*/100);
  mock_source->SetDevice(device);
  MediaStreamSource* source = MakeGarbageCollected<MediaStreamSource>(
      "test_id", MediaStreamSource::StreamType::kTypeAudio, "test_name",
      /*remote=*/false, std::move(mock_source));
  MediaStreamComponent* component =
      MakeGarbageCollected<MediaStreamComponentImpl>("component_id", source,
                                                     std::move(platform_track));
  component->SetContentHint(WebMediaStreamTrack::ContentHintType::kAudioSpeech);
  return component;
}

}  // namespace blink
