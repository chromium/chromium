// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_encoder.h"

#include "media/base/video_frame.h"

namespace media {

VideoEncoderOutput::VideoEncoderOutput() = default;
VideoEncoderOutput::VideoEncoderOutput(VideoEncoderOutput&&) = default;
VideoEncoderOutput::~VideoEncoderOutput() = default;

VideoEncoder::VideoEncoder() = default;
VideoEncoder::~VideoEncoder() = default;

VideoEncoder::Options::Options() = default;
VideoEncoder::Options::Options(const Options&) = default;
VideoEncoder::Options::~Options() = default;

VideoEncoder::PendingEncode::PendingEncode() = default;
VideoEncoder::PendingEncode::PendingEncode(PendingEncode&&) = default;
VideoEncoder::PendingEncode::~PendingEncode() = default;

}  // namespace media