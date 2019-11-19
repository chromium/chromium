// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIASTREAM_MEDIA_STREAM_VIDEO_SINK_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIASTREAM_MEDIA_STREAM_VIDEO_SINK_H_

#include "media/capture/video_capture_types.h"
#include "third_party/blink/public/common/media/video_capture.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_sink.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"

namespace blink {

// MediaStreamVideoSink is a base class that contains implementation commonly
// useful for implementations of WebMediaStreamSink which
// connect/disconnect the sink implementation to a track to start/stop the flow
// of video frames.
//
// http://dev.w3.org/2011/webrtc/editor/getusermedia.html
// All methods calls must be made from the main render thread.
class BLINK_MODULES_EXPORT MediaStreamVideoSink : public WebMediaStreamSink {
 public:
  void OnFrameDropped(media::VideoCaptureFrameDropReason reason);

 protected:
  MediaStreamVideoSink();
  ~MediaStreamVideoSink() override;

  // A subclass should call ConnectToTrack when it is ready to receive data from
  // a video track. Before destruction, DisconnectFromTrack must be called.
  // This base class holds a reference to the WebMediaStreamTrack until
  // DisconnectFromTrack is called.
  //
  // Calls to these methods must be done on the main render thread.
  // Note that |callback| for frame delivery happens on the IO thread.
  //
  // Warning: Calling DisconnectFromTrack does not immediately stop frame
  // delivery through the |callback|, since frames are being delivered on a
  // different thread.
  //
  // |is_sink_secure| indicates if this MediaStreamVideoSink is secure (i.e.
  // meets output protection requirement). Generally, this should be false
  // unless you know what you are doing.
  void ConnectToTrack(const WebMediaStreamTrack& track,
                      const VideoCaptureDeliverFrameCB& callback,
                      bool is_sink_secure);
  void DisconnectFromTrack();

  // Returns the currently-connected track, or a null instance otherwise.
  const WebMediaStreamTrack& connected_track() const {
    return connected_track_;
  }

 private:
  // Set by ConnectToTrack() and cleared by DisconnectFromTrack().
  WebMediaStreamTrack connected_track_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIASTREAM_MEDIA_STREAM_VIDEO_SINK_H_
