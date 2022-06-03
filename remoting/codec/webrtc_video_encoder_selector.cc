// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/webrtc_video_encoder_selector.h"

#include "base/check_op.h"
#include "remoting/base/constants.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

WebrtcVideoEncoderSelector::WebrtcVideoEncoderSelector() = default;
WebrtcVideoEncoderSelector::~WebrtcVideoEncoderSelector() = default;

std::unique_ptr<WebrtcVideoEncoder>
WebrtcVideoEncoderSelector::CreateEncoder() {
  if (last_codec_ == -1) {
    // First CreateEncoder() function call, checks |preferred_codec_| first.
    if (preferred_codec_ != -1) {
      if (encoders_[preferred_codec_].first.Run(profile_)) {
        last_codec_ = preferred_codec_;
        return encoders_[preferred_codec_].second.Run();
      }
    }
  } else if (last_codec_ == preferred_codec_) {
    // Last codec is |preferred_codec_|, let's start from the first codec.
    last_codec_ = -1;
  }

  for (int i = last_codec_ + 1; i < static_cast<int>(encoders_.size()); i++) {
    if (i == preferred_codec_) {
      continue;
    }

    if (encoders_[i].first.Run(profile_)) {
      last_codec_ = i;
      return encoders_[i].second.Run();
    }
  }

  return nullptr;
}

void WebrtcVideoEncoderSelector::SetDesktopFrame(
    const webrtc::DesktopFrame& frame) {
  // TODO(zijiehe): The frame rate should not be fixed.
  profile_.frame_rate = kTargetFrameRate;
  gfx::Size new_resolution(frame.size().width(), frame.size().height());
  if (new_resolution != profile_.resolution) {
    profile_.resolution = new_resolution;
    last_codec_ = -1;
  }
}

void WebrtcVideoEncoderSelector::SetPreferredCodec(int codec) {
  DCHECK_GE(codec, 0);
  DCHECK_LT(codec, static_cast<int>(encoders_.size()));
  preferred_codec_ = codec;

  // Reset so that the next call to CreateEncoder() creates an encoder which
  // matches the one negotiated over SDP. Otherwise, repeated calls to
  // CreateEncoder() would cycle through every codec that could encode at the
  // current resolution, even when they do not match the codec "created" by
  // WebRTC via the DummyVideoEncoderFactory.

  // TODO(crbug.com/1115789): Review the CreateEncoder() logic to ensure it only
  // creates encoders that are compatible with the SDP-selected codec.
  last_codec_ = -1;
}

int WebrtcVideoEncoderSelector::RegisterEncoder(
    WebrtcVideoEncoderSelector::IsProfileSupportedFunction is_supported,
    WebrtcVideoEncoderSelector::CreateEncoderFunction creator) {
  encoders_.push_back(std::make_pair(is_supported, creator));
  return static_cast<int>(encoders_.size()) - 1;
}

}  // namespace remoting
