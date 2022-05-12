// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_WEBRTC_VIDEO_ENCODER_AV1_H_
#define REMOTING_CODEC_WEBRTC_VIDEO_ENCODER_AV1_H_

#include "base/callback.h"
#include "remoting/codec/webrtc_video_encoder.h"
#include "third_party/libaom/source/libaom/aom/aom_encoder.h"
#include "third_party/libaom/source/libaom/aom/aomcx.h"

namespace remoting {

// AV1 encoder implementation for WebRTC transport, params are optimized for
// real-time screen sharing.
class WebrtcVideoEncoderAV1 : public WebrtcVideoEncoder {
 public:
  WebrtcVideoEncoderAV1();
  WebrtcVideoEncoderAV1(const WebrtcVideoEncoderAV1&) = delete;
  WebrtcVideoEncoderAV1& operator=(const WebrtcVideoEncoderAV1&) = delete;
  ~WebrtcVideoEncoderAV1() override;

  // WebrtcVideoEncoder interface.
  void SetLosslessEncode(bool want_lossless) override;
  void SetLosslessColor(bool want_lossless) override;
  void Encode(std::unique_ptr<webrtc::DesktopFrame> frame,
              const FrameParams& params,
              EncodeCallback done) override;

 private:
  using aom_codec_unique_ptr =
      std::unique_ptr<aom_codec_ctx_t, void (*)(aom_codec_ctx_t*)>;

  aom_codec_unique_ptr codec_;
};

}  // namespace remoting

#endif  // REMOTING_CODEC_WEBRTC_VIDEO_ENCODER_AV1_H_
