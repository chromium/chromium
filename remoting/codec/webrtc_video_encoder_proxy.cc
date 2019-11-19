// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/webrtc_video_encoder_proxy.h"

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

class WebrtcVideoEncoderProxy::Core {
 public:
  Core(std::unique_ptr<WebrtcVideoEncoder> encoder)
      : encoder_(std::move(encoder)),
        main_task_runner_(base::SequencedTaskRunnerHandle::Get()) {}
  ~Core() = default;

  void SetLosslessEncode(bool want_lossless) {
    encoder_->SetLosslessEncode(want_lossless);
  }

  void SetLosslessColor(bool want_lossless) {
    encoder_->SetLosslessColor(want_lossless);
  }

  void Encode(std::unique_ptr<webrtc::DesktopFrame> frame,
              const FrameParams& params,
              EncodeCallback done) {
    encoder_->Encode(std::move(frame), params,
                     base::BindOnce(&Core::OnEncoded, base::Unretained(this),
                                    std::move(done)));
  }

 private:
  void OnEncoded(EncodeCallback done,
                 EncodeResult result,
                 std::unique_ptr<WebrtcVideoEncoder::EncodedFrame> frame) {
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(done), result, std::move(frame)));
  }

  std::unique_ptr<WebrtcVideoEncoder> encoder_;
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
};

WebrtcVideoEncoderProxy::WebrtcVideoEncoderProxy(
    std::unique_ptr<WebrtcVideoEncoder> encoder,
    scoped_refptr<base::SequencedTaskRunner> encode_task_runner)
    : core_(new Core(std::move(encoder))),
      encode_task_runner_(encode_task_runner) {}

WebrtcVideoEncoderProxy::~WebrtcVideoEncoderProxy() {
  encode_task_runner_->DeleteSoon(FROM_HERE, core_.release());
}

void WebrtcVideoEncoderProxy::SetLosslessEncode(bool want_lossless) {
  encode_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Core::SetLosslessEncode,
                                base::Unretained(core_.get()), want_lossless));
}

void WebrtcVideoEncoderProxy::SetLosslessColor(bool want_lossless) {
  encode_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Core::SetLosslessColor,
                                base::Unretained(core_.get()), want_lossless));
}

void WebrtcVideoEncoderProxy::Encode(
    std::unique_ptr<webrtc::DesktopFrame> frame,
    const FrameParams& params,
    EncodeCallback done) {
  encode_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &Core::Encode, base::Unretained(core_.get()), std::move(frame),
          params,
          base::BindOnce(&WebrtcVideoEncoderProxy::OnEncoded,
                         weak_factory_.GetWeakPtr(), std::move(done))));
}

void WebrtcVideoEncoderProxy::OnEncoded(
    EncodeCallback done,
    EncodeResult result,
    std::unique_ptr<WebrtcVideoEncoder::EncodedFrame> frame) {
  std::move(done).Run(result, std::move(frame));
}

}  // namespace remoting
