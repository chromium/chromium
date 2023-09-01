// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/muxers/muxer.h"

#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace media {

Muxer::VideoParameters::VideoParameters(const VideoFrame& frame)
    : visible_rect_size(frame.visible_rect().size()),
      frame_rate(frame.metadata().frame_rate.value_or(0.0)),
      codec(VideoCodec::kUnknown),
      color_space(frame.ColorSpace()) {}

Muxer::VideoParameters::VideoParameters(
    gfx::Size visible_rect_size,
    double frame_rate,
    VideoCodec codec,
    absl::optional<gfx::ColorSpace> color_space)
    : visible_rect_size(visible_rect_size),
      frame_rate(frame_rate),
      codec(codec),
      color_space(color_space) {}

Muxer::VideoParameters::VideoParameters(const VideoParameters&) = default;

Muxer::VideoParameters::~VideoParameters() = default;

}  // namespace media
