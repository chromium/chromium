// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_VIDEO_CHANNEL_STATE_OBSERVER_H_
#define REMOTING_PROTOCOL_VIDEO_CHANNEL_STATE_OBSERVER_H_

#include "remoting/codec/webrtc_video_encoder.h"
#include "third_party/webrtc/api/video_codecs/video_encoder.h"

namespace remoting::protocol {

class VideoChannelStateObserver {
 public:
  // Called after the encoded frame is sent via the WebRTC registered callback.
  // The result contains the frame ID assigned by WebRTC if successfully sent.
  // This is only called if the encoder successfully returned a non-null
  // frame. |result| is the result returned by the registered callback.
  virtual void OnEncodedFrameSent(
      webrtc::EncodedImageCallback::Result result,
      const WebrtcVideoEncoder::EncodedFrame& frame) = 0;

 protected:
  virtual ~VideoChannelStateObserver() = default;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_VIDEO_CHANNEL_STATE_OBSERVER_H_
