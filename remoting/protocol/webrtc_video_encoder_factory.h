// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_VIDEO_ENCODER_FACTORY_H_
#define REMOTING_PROTOCOL_WEBRTC_VIDEO_ENCODER_FACTORY_H_

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/base/session_options.h"
#include "remoting/protocol/video_stream_event_router.h"
#include "third_party/webrtc/api/video_codecs/av1_profile.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"
#include "third_party/webrtc/api/video_codecs/video_encoder_factory.h"
#include "third_party/webrtc/api/video_codecs/vp9_profile.h"
#include "third_party/webrtc/modules/video_coding/include/video_codec_interface.h"

namespace remoting::protocol {

// This is the encoder factory that is passed to WebRTC when the peer connection
// is created. This factory creates the video encoder, which is an instance of
// WebrtcVideoEncoderWrapper which wraps a video codec from remoting/codec.
class WebrtcVideoEncoderFactory : public webrtc::VideoEncoderFactory {
 public:
  WebrtcVideoEncoderFactory();
  ~WebrtcVideoEncoderFactory() override;

  // webrtc::VideoEncoderFactory interface.
  std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(
      const webrtc::SdpVideoFormat& format) override;
  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override;

  void ApplySessionOptions(const SessionOptions& options);

  VideoStreamEventRouter& video_stream_event_router() { return event_router_; }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  std::vector<webrtc::SdpVideoFormat> supported_formats_{
      webrtc::SdpVideoFormat("VP8"),
      webrtc::SdpVideoFormat("VP9"),
      webrtc::SdpVideoFormat(
          "VP9",
          {{webrtc::kVP9FmtpProfileId,
            webrtc::VP9ProfileToString(webrtc::VP9Profile::kProfile1)}}),
      webrtc::SdpVideoFormat("AV1"),
      webrtc::SdpVideoFormat(
          "AV1",
          {{webrtc::kAV1FmtpProfile,
            webrtc::AV1ProfileToString(webrtc::AV1Profile::kProfile1)
                .data()}})};

  SessionOptions session_options_;

  // Enables events to be routed from WebRTC created video encoders to the video
  // stream representing the display being captured and encoded.
  VideoStreamEventRouter event_router_;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_WEBRTC_VIDEO_ENCODER_FACTORY_H_
