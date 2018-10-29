// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/media_stream_utils.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/guid.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/renderer/media/stream/external_media_stream_audio_source.h"
#include "content/renderer/media/stream/media_stream_constraints_util.h"
#include "content/renderer/media/stream/media_stream_video_capturer_source.h"
#include "content/renderer/media/stream/media_stream_video_source.h"
#include "content/renderer/media/stream/media_stream_video_track.h"
#include "media/base/audio_capturer_source.h"
#include "media/capture/video_capturer_source.h"
#include "third_party/blink/public/platform/web_media_stream.h"
#include "third_party/blink/public/platform/web_media_stream_source.h"

namespace content {

bool AddVideoTrackToMediaStream(
    std::unique_ptr<media::VideoCapturerSource> video_source,
    bool is_remote,
    blink::WebMediaStream* web_media_stream) {
  DCHECK(video_source.get());
  if (!web_media_stream || web_media_stream->IsNull()) {
    DLOG(ERROR) << "WebMediaStream is null";
    return false;
  }

  media::VideoCaptureFormats preferred_formats =
      video_source->GetPreferredFormats();
  MediaStreamVideoSource* const media_stream_source =
      new MediaStreamVideoCapturerSource(
          MediaStreamSource::SourceStoppedCallback(), std::move(video_source));
  const blink::WebString track_id =
      blink::WebString::FromUTF8(base::GenerateGUID());
  blink::WebMediaStreamSource web_media_stream_source;
  web_media_stream_source.Initialize(
      track_id, blink::WebMediaStreamSource::kTypeVideo, track_id, is_remote);
  // Takes ownership of |media_stream_source|.
  web_media_stream_source.SetExtraData(media_stream_source);
  web_media_stream_source.SetCapabilities(ComputeCapabilitiesForVideoSource(
      track_id, preferred_formats,
      media::VideoFacingMode::MEDIA_VIDEO_FACING_NONE,
      false /* is_device_capture */));
  web_media_stream->AddTrack(MediaStreamVideoTrack::CreateVideoTrack(
      media_stream_source, MediaStreamVideoSource::ConstraintsCallback(),
      true));
  return true;
}

bool AddAudioTrackToMediaStream(
    scoped_refptr<media::AudioCapturerSource> audio_source,
    int sample_rate,
    media::ChannelLayout channel_layout,
    int frames_per_buffer,
    bool is_remote,
    blink::WebMediaStream* web_media_stream) {
  DCHECK(audio_source.get());
  if (!web_media_stream || web_media_stream->IsNull()) {
    DLOG(ERROR) << "WebMediaStream is null";
    return false;
  }

  const media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout,
      sample_rate, frames_per_buffer);
  if (!params.IsValid()) {
    DLOG(ERROR) << "Invalid audio parameters.";
    return false;
  }

  blink::WebMediaStreamSource web_media_stream_source;
  const blink::WebString track_id =
      blink::WebString::FromUTF8(base::GenerateGUID());
  web_media_stream_source.Initialize(
      track_id, blink::WebMediaStreamSource::kTypeAudio, track_id, is_remote);
  MediaStreamAudioSource* const media_stream_source =
      new ExternalMediaStreamAudioSource(std::move(audio_source), sample_rate,
                                         channel_layout, frames_per_buffer,
                                         is_remote);
  // Takes ownership of |media_stream_source|.
  web_media_stream_source.SetExtraData(media_stream_source);

  blink::WebMediaStreamSource::Capabilities capabilities;
  capabilities.device_id = track_id;
  capabilities.echo_cancellation = std::vector<bool>({false});
  capabilities.auto_gain_control = std::vector<bool>({false});
  capabilities.noise_suppression = std::vector<bool>({false});
  web_media_stream_source.SetCapabilities(capabilities);

  blink::WebMediaStreamTrack web_media_stream_track;
  web_media_stream_track.Initialize(web_media_stream_source);
  if (!media_stream_source->ConnectToTrack(web_media_stream_track))
    return false;
  web_media_stream->AddTrack(web_media_stream_track);
  return true;
}

void RequestRefreshFrameFromVideoTrack(
    const blink::WebMediaStreamTrack& video_track) {
  if (video_track.IsNull())
    return;
  MediaStreamVideoSource* const source =
      MediaStreamVideoSource::GetVideoSource(video_track.Source());
  if (source)
    source->RequestRefreshFrame();
}

}  // namespace content
