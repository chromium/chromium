// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/modules/mediastream/media_stream_video_sink.h"

#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"

namespace blink {

namespace {

// Calls to these methods must be done on the main render thread.
// Note that |callback| for frame delivery happens on the video task runner.
// Warning: Calling RemoveSinkFromMediaStreamTrack does not immediately stop
// frame delivery through the |callback|, since frames are being delivered on
// a different thread.
// |is_sink_secure| indicates if |sink| meets output protection requirement.
// Generally, this should be false unless you know what you are doing.
void AddSinkToMediaStreamTrack(const WebMediaStreamTrack& track,
                               WebMediaStreamSink* sink,
                               const VideoCaptureDeliverFrameCB& callback,
                               MediaStreamVideoSink::IsSecure is_secure,
                               MediaStreamVideoSink::UsesAlpha uses_alpha) {
  static_cast<MediaStreamComponent*>(track)->AddSink(sink, callback, is_secure,
                                                     uses_alpha);
}

void RemoveSinkFromMediaStreamTrack(const WebMediaStreamTrack& track,
                                    WebMediaStreamSink* sink) {
  MediaStreamVideoTrack* const video_track = MediaStreamVideoTrack::From(track);
  if (video_track)
    video_track->RemoveSink(sink);
}
}  // namespace

MediaStreamVideoSink::MediaStreamVideoSink() : WebMediaStreamSink() {}

MediaStreamVideoSink::~MediaStreamVideoSink() {
  // Ensure this sink has disconnected from the track.
  DisconnectFromTrack();
}

void MediaStreamVideoSink::ConnectToTrack(
    const WebMediaStreamTrack& track,
    const VideoCaptureDeliverFrameCB& callback,
    MediaStreamVideoSink::IsSecure is_secure,
    MediaStreamVideoSink::UsesAlpha uses_alpha) {
  DCHECK(connected_track_.IsNull());
  connected_track_ = track;
  AddSinkToMediaStreamTrack(track, this, callback, is_secure, uses_alpha);
}

void MediaStreamVideoSink::ConnectEncodedToTrack(
    const WebMediaStreamTrack& track,
    const EncodedVideoFrameCB& callback) {
  DCHECK(connected_encoded_track_.IsNull());
  connected_encoded_track_ = track;
  MediaStreamVideoTrack* const video_track = MediaStreamVideoTrack::From(track);
  DCHECK(video_track);
  video_track->AddEncodedSink(this, callback);
}

void MediaStreamVideoSink::DisconnectFromTrack() {
  RemoveSinkFromMediaStreamTrack(connected_track_, this);
  connected_track_.Reset();
}

void MediaStreamVideoSink::DisconnectEncodedFromTrack() {
  MediaStreamVideoTrack* const video_track =
      MediaStreamVideoTrack::From(connected_encoded_track_);
  if (video_track) {
    video_track->RemoveEncodedSink(this);
  }
  connected_encoded_track_.Reset();
}

void MediaStreamVideoSink::OnFrameDropped(
    media::VideoCaptureFrameDropReason reason) {
  if (connected_track_.IsNull())
    return;

  // For UMA reasons we want to log this frame as dropped, even though it was
  // delivered to the sink before being dropped. This is not considered a frame
  // drop by the MediaStreamTrack Statistics API.
  if (auto* const video_track = MediaStreamVideoTrack::From(connected_track_)) {
    video_track->OnSinkDroppedFrame(reason);
  }
}

double MediaStreamVideoSink::GetRequiredMinFramesPerSec() const {
  return 0;
}

}  // namespace blink
