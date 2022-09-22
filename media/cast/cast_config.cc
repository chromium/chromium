// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/cast_config.h"

namespace media {
namespace cast {

VideoCodecParams::VideoCodecParams() = default;
VideoCodecParams::VideoCodecParams(const VideoCodecParams& other) = default;
VideoCodecParams::VideoCodecParams(VideoCodecParams&& other) = default;
VideoCodecParams& VideoCodecParams::operator=(const VideoCodecParams& other) =
    default;
VideoCodecParams& VideoCodecParams::operator=(VideoCodecParams&& other) =
    default;
VideoCodecParams::~VideoCodecParams() = default;

FrameSenderConfig::FrameSenderConfig() = default;
FrameSenderConfig::FrameSenderConfig(const FrameSenderConfig& other) = default;
FrameSenderConfig::FrameSenderConfig(FrameSenderConfig&& other) = default;
FrameSenderConfig& FrameSenderConfig::operator=(
    const FrameSenderConfig& other) = default;
FrameSenderConfig& FrameSenderConfig::operator=(FrameSenderConfig&& other) =
    default;
FrameSenderConfig::~FrameSenderConfig() = default;

FrameReceiverConfig::FrameReceiverConfig() = default;
FrameReceiverConfig::FrameReceiverConfig(const FrameReceiverConfig& other) =
    default;
FrameReceiverConfig::FrameReceiverConfig(FrameReceiverConfig&& other) = default;
FrameReceiverConfig& FrameReceiverConfig::operator=(
    const FrameReceiverConfig& other) = default;
FrameReceiverConfig& FrameReceiverConfig::operator=(
    FrameReceiverConfig&& other) = default;
FrameReceiverConfig::~FrameReceiverConfig() = default;

}  // namespace cast
}  // namespace media
