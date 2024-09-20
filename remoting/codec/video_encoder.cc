// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/video_encoder.h"

#include <ostream>

#include "base/notreached.h"
#include "remoting/codec/video_encoder_verbatim.h"
#include "remoting/codec/video_encoder_vpx.h"
#include "remoting/protocol/session_config.h"

namespace remoting {

std::unique_ptr<VideoEncoder> VideoEncoder::Create(
    const protocol::SessionConfig& config) {
  const protocol::ChannelConfig& video_config = config.video_config();

  if (video_config.codec == protocol::ChannelConfig::CODEC_VP8) {
    return VideoEncoderVpx::CreateForVP8();
  } else if (video_config.codec == protocol::ChannelConfig::CODEC_VP9) {
    return VideoEncoderVpx::CreateForVP9();
  } else if (video_config.codec == protocol::ChannelConfig::CODEC_VERBATIM) {
    return std::make_unique<VideoEncoderVerbatim>();
  }

  NOTREACHED() << "Unknown codec type: " << video_config.codec;
}

}  // namespace remoting
