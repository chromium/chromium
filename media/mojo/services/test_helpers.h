// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_TEST_HELPERS_H_
#define MEDIA_MOJO_SERVICES_TEST_HELPERS_H_

#include <string>

#include "media/mojo/mojom/media_types.mojom.h"

namespace media {

mojom::PredictionFeatures MakeFeatures(VideoCodecProfile profile,
                                       const gfx::Size& video_size,
                                       double frames_per_sec,
                                       const std::string& key_system = "",
                                       bool use_hw_secure_codecs = false);

mojom::PredictionFeaturesPtr MakeFeaturesPtr(VideoCodecProfile profile,
                                             const gfx::Size& video_size,
                                             double frames_per_sec,
                                             const std::string& key_system = "",
                                             bool use_hw_secure_codecs = false);

mojom::PredictionTargets MakeTargets(uint32_t frames_decoded,
                                     uint32_t frames_dropped,
                                     uint32_t frames_power_efficient);

mojom::PredictionTargetsPtr MakeTargetsPtr(uint32_t frames_decoded,
                                           uint32_t frames_dropped,
                                           uint32_t frames_power_efficient);

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_TEST_HELPERS_H_
