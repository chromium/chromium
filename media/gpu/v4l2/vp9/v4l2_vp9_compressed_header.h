// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_VP9_V4L2_VP9_COMPRESSED_HEADER_H_
#define MEDIA_GPU_V4L2_VP9_V4L2_VP9_COMPRESSED_HEADER_H_

#include <optional>

#include "media/gpu/v4l2/vp9/vp9_v4l2_compressed_header_types.h"

namespace media {

struct Vp9FrameHeader;

// Parses the VP9 compressed header for V4L2 stateless decoding.
// Returns the parsed compressed-header result on success. If the frame does not
// have a compressed header, returns a result initialized from |frame_hdr|.
// Returns std::nullopt if parsing fails or the compressed header range is
// invalid
std::optional<Vp9V4L2CompressedParseResult> ParseVp9CompressedHeaderForV4L2(
    const Vp9FrameHeader& frame_hdr);

}  // namespace media

#endif  // MEDIA_GPU_V4L2_VP9_V4L2_VP9_COMPRESSED_HEADER_H_
