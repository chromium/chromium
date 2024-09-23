// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_H264_BUILDER_H_
#define MEDIA_GPU_H264_BUILDER_H_

#include "media/gpu/media_gpu_export.h"
#include "media/parsers/h264_parser.h"

namespace media {

class H26xAnnexBBitstreamBuilder;

// Build the packed bitstream of H264SPS into |bitstream_builder|.
MEDIA_GPU_EXPORT void BuildPackedH264SPS(
    H26xAnnexBBitstreamBuilder& bitstream_builder,
    const H264SPS& sps);
// Build the packed bitstream of H264PPS into |bitstream_builder|.
MEDIA_GPU_EXPORT void BuildPackedH264PPS(
    H26xAnnexBBitstreamBuilder& bitstream_builder,
    const H264SPS& sps,
    const H264PPS& pps);

}  // namespace media

#endif  // MEDIA_GPU_H264_BUILDER_H_
