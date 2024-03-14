// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_H264_BUILDER_H_
#define MEDIA_GPU_H264_BUILDER_H_

#include "media/gpu/media_gpu_export.h"
#include "media/video/h264_parser.h"

namespace media {

class H264BitstreamBuffer;

// Build the packed bitstream of H264SPS into |bitstream_buffer|.
MEDIA_GPU_EXPORT void BuildPackedH264SPS(H264BitstreamBuffer& bitstream_buffer,
                                         const H264SPS& sps);
// Build the packed bitstream of H264PPS into |bitstream_buffer|.
MEDIA_GPU_EXPORT void BuildPackedH264PPS(H264BitstreamBuffer& bitstream_buffer,
                                         const H264SPS& sps,
                                         const H264PPS& pps);

}  // namespace media

#endif  // MEDIA_GPU_H264_BUILDER_H_
