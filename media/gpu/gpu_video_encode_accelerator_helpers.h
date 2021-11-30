// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_GPU_VIDEO_ENCODE_ACCELERATOR_HELPERS_H_
#define MEDIA_GPU_GPU_VIDEO_ENCODE_ACCELERATOR_HELPERS_H_

#include <vector>

#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// Helper functions for VideoEncodeAccelerator implementations in GPU process.

// Calculate the bitstream buffer size for VideoEncodeAccelerator.
// |size|: the resolution of video stream
// |bitrate|: the bit rate in bps
// |framerate|: the frame rate in fps
MEDIA_GPU_EXPORT size_t GetEncodeBitstreamBufferSize(const gfx::Size& size,
                                                     uint32_t bitrate,
                                                     uint32_t framerate);

// Get the maximum bitstream buffer size for VideoEncodeAccelerator.
// |size|: the resolution of video stream
MEDIA_GPU_EXPORT size_t GetEncodeBitstreamBufferSize(const gfx::Size& size);

// Get the frame rate fraction assigned to each temporal layer.
// |num_temporal_layers|: total number of temporal layers
MEDIA_GPU_EXPORT std::vector<uint8_t> GetFpsAllocation(
    size_t num_temporal_layers);

}  // namespace media

#endif  // MEDIA_GPU_GPU_VIDEO_ENCODE_ACCELERATOR_HELPERS_H_
