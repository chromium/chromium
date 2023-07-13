// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_GPU_VIDEO_ENCODE_ACCELERATOR_HELPERS_H_
#define MEDIA_GPU_GPU_VIDEO_ENCODE_ACCELERATOR_HELPERS_H_

#include <vector>

#include "media/base/bitrate.h"
#include "media/base/video_bitrate_allocation.h"
#include "media/gpu/media_gpu_export.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class Bitrate;

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

// Create default VideoBitrateAllocation from |config|. A bitrate of each
// spatial layer (|config.spatial_layers[i].bitrate_bps| is distributed to
// temporal layers in the spatial layer based on the same bitrate division ratio
// as a software encoder. If |config.spatial_layers| is empty,
// VideoBitrateAllocation(0, 0) is set to |config.bitrate.target_bps()| as it is
// a configuration with no layers.
MEDIA_GPU_EXPORT VideoBitrateAllocation
AllocateBitrateForDefaultEncoding(const VideoEncodeAccelerator::Config& config);

// Create VideoBitrateAllocation with |num_spatial_layers|,
// |num_temporal_layers| and |bitrate|. |bitrate.target_bps()| is the bitrate of
// the entire stream. |num_temporal_layers| is the number of temporal layers in
// each spatial layer.
// First, |bitrate.target_bps()| is distributed to spatial layers based on
// libwebrtc bitrate division. Then the bitrate of each spatial layer is
// distributed to temporal layers in the spatial layer based on the same bitrate
// division ratio as a software encoder. If a variable bitrate is requested,
// that is, |bitrate.mode()| is Bitrate::Mode::Variable, the peak will be set
// equal to the |bitrate.peak_bps()|.
MEDIA_GPU_EXPORT VideoBitrateAllocation
AllocateDefaultBitrateForTesting(const size_t num_spatial_layers,
                                 const size_t num_temporal_layers,
                                 const Bitrate& bitrate);

// Create VideoBitrateAllocation with the bitrate for each spatial layer and
// |num_temporal_layers|.
VideoBitrateAllocation MEDIA_GPU_EXPORT
AllocateBitrateForDefaultEncodingWithBitrates(
    const std::vector<uint32_t>& spatial_layer_bitrates,
    const size_t num_temporal_layers,
    const bool uses_vbr);
}  // namespace media

#endif  // MEDIA_GPU_GPU_VIDEO_ENCODE_ACCELERATOR_HELPERS_H_
