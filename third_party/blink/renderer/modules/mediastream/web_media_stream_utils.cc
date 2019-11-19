// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/modules/mediastream/web_media_stream_utils.h"

#include <memory>
#include <utility>

#include "media/capture/video_capturer_source.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_sink.h"
#include "third_party/blink/public/platform/web_media_stream.h"
#include "third_party/blink/public/platform/web_media_stream_source.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util.h"

namespace blink {

void RequestRefreshFrameFromVideoTrack(const WebMediaStreamTrack& video_track) {
  if (video_track.IsNull())
    return;
  MediaStreamVideoSource* const source =
      MediaStreamVideoSource::GetVideoSource(video_track.Source());
  if (source)
    source->RequestRefreshFrame();
}

void AddSinkToMediaStreamTrack(const WebMediaStreamTrack& track,
                               WebMediaStreamSink* sink,
                               const VideoCaptureDeliverFrameCB& callback,
                               bool is_sink_secure) {
  MediaStreamVideoTrack* const video_track =
      MediaStreamVideoTrack::GetVideoTrack(track);
  DCHECK(video_track);
  video_track->AddSink(sink, callback, is_sink_secure);
}

void RemoveSinkFromMediaStreamTrack(const WebMediaStreamTrack& track,
                                    WebMediaStreamSink* sink) {
  MediaStreamVideoTrack* const video_track =
      MediaStreamVideoTrack::GetVideoTrack(track);
  if (video_track)
    video_track->RemoveSink(sink);
}

}  // namespace blink
