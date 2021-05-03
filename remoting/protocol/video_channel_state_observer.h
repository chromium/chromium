// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_VIDEO_CHANNEL_STATE_OBSERVER_H_
#define REMOTING_PROTOCOL_VIDEO_CHANNEL_STATE_OBSERVER_H_

#include "base/time/time.h"
#include "remoting/codec/webrtc_video_encoder.h"
#include "third_party/webrtc/api/video_codecs/video_encoder.h"

namespace remoting {
namespace protocol {

class VideoChannelStateObserver {
 public:
  // Signals to the video-scheduler that the encoder is ready to accept captured
  // frames for encoding and sending.
  virtual void OnEncoderReady() = 0;

  virtual void OnKeyFrameRequested() = 0;
  virtual void OnTargetBitrateChanged(int bitrate_kbps) = 0;
  virtual void OnRttUpdate(base::TimeDelta rtt) = 0;

  // Notifies the scheduler that the encoder wants to continue receiving
  // captured frames even if nothing has changed (so it can re-encode the frame
  // with increasing quality). If this is false, the scheduler need not send
  // "empty" frames (no update region) for encoding/sending. The initial
  // state is false, and the notification is raised whenever the state changes.
  virtual void OnTopOffActive(bool active) = 0;

  // Called when the encoder has finished encoding a frame, and before it is
  // passed to WebRTC's registered callback. |frame| is non-const so that
  // WebrtcVideoStream can add timestamps to it before sending.
  // TODO(crbug.com/1192865): Make |frame| const when standard encoding pipeline
  // is implemented.
  virtual void OnFrameEncoded(WebrtcVideoEncoder::EncodeResult encode_result,
                              WebrtcVideoEncoder::EncodedFrame* frame) = 0;

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

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_VIDEO_CHANNEL_STATE_OBSERVER_H_
