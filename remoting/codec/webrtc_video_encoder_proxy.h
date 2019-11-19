// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_WEBRTC_VIDEO_ENCODER_PROXY_H_
#define REMOTING_CODEC_WEBRTC_VIDEO_ENCODER_PROXY_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "remoting/codec/webrtc_video_encoder.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace remoting {

// WebrtcVideoEncoder implementation that runs encoder on a background thread.
class WebrtcVideoEncoderProxy : public WebrtcVideoEncoder {
 public:
  WebrtcVideoEncoderProxy(
      std::unique_ptr<WebrtcVideoEncoder> encoder,
      scoped_refptr<base::SequencedTaskRunner> encode_task_runner);
  ~WebrtcVideoEncoderProxy() override;

  // WebrtcVideoEncoder interface.
  void SetLosslessEncode(bool want_lossless) override;
  void SetLosslessColor(bool want_lossless) override;
  void Encode(std::unique_ptr<webrtc::DesktopFrame> frame,
              const FrameParams& params,
              EncodeCallback done) override;

 private:
  class Core;

  void OnEncoded(EncodeCallback done,
                 EncodeResult result,
                 std::unique_ptr<WebrtcVideoEncoder::EncodedFrame> frame);

  std::unique_ptr<Core> core_;
  scoped_refptr<base::SequencedTaskRunner> encode_task_runner_;
  base::WeakPtrFactory<WebrtcVideoEncoderProxy> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_CODEC_WEBRTC_VIDEO_ENCODER_PROXY_H_
