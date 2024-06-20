// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_H265_BUILDER_H_
#define MEDIA_GPU_H265_BUILDER_H_

#include "media/gpu/media_gpu_export.h"
#include "media/parsers/h265_parser.h"

namespace media {

class H26xAnnexBBitstreamBuilder;

MEDIA_GPU_EXPORT void BuildPackedH265ProfileTierLevel(
    H26xAnnexBBitstreamBuilder& builder,
    const H265ProfileTierLevel& profile_tier_level,
    bool profile_present_flag,
    uint8_t max_num_sub_layers_minus1);

MEDIA_GPU_EXPORT void BuildPackedH265VPS(H26xAnnexBBitstreamBuilder& builder,
                                         const H265VPS& vps);

MEDIA_GPU_EXPORT void BuildPackedH265SPS(H26xAnnexBBitstreamBuilder& builder,
                                         const H265SPS& sps);

MEDIA_GPU_EXPORT void BuildPackedH265PPS(H26xAnnexBBitstreamBuilder& builder,
                                         const H265PPS& pps);

}  // namespace media

#endif  // MEDIA_GPU_H265_BUILDER_H_
