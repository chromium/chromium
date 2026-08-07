// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_MAC_VT_HDR_METADATA_H_
#define MEDIA_GPU_MAC_VT_HDR_METADATA_H_

#include <CoreMedia/CoreMedia.h>
#include <stdint.h>

#include <vector>

#include "media/base/video_codecs.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

// Builds an Annex B SEI NAL unit from HDR static metadata in `sample_buffer`.
// Returns an empty vector when no supported metadata is present.
MEDIA_GPU_EXPORT std::vector<uint8_t> BuildHdrMetadataSeiNalu(
    VideoCodec codec,
    CMSampleBufferRef sample_buffer);

}  // namespace media

#endif  // MEDIA_GPU_MAC_VT_HDR_METADATA_H_
