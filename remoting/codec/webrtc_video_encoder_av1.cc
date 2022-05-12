// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/webrtc_video_encoder_av1.h"

#include "base/callback.h"
#include "base/notreached.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

namespace {

void DestroyAomCodecContext(aom_codec_ctx_t* codec_ctx) {
  // Codec has been initialized so we need to destroy it.
  auto error = aom_codec_destroy(codec_ctx);
  DCHECK_EQ(error, AOM_CODEC_OK);
  delete codec_ctx;
}

}  // namespace

WebrtcVideoEncoderAV1::WebrtcVideoEncoderAV1()
    : codec_(nullptr, DestroyAomCodecContext) {}
WebrtcVideoEncoderAV1::~WebrtcVideoEncoderAV1() = default;

void WebrtcVideoEncoderAV1::SetLosslessEncode(bool want_lossless) {}

void WebrtcVideoEncoderAV1::SetLosslessColor(bool want_lossless) {}

void WebrtcVideoEncoderAV1::Encode(std::unique_ptr<webrtc::DesktopFrame> frame,
                                   const FrameParams& params,
                                   EncodeCallback done) {
  NOTIMPLEMENTED();
  std::move(done).Run(EncodeResult::UNKNOWN_ERROR, nullptr);
}

}  // namespace remoting
