// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/test_helpers.h"

namespace media {

mojom::PredictionFeatures MakeFeatures(VideoCodecProfile profile,
                                       const gfx::Size& video_size,
                                       double frames_per_sec,
                                       const std::string& key_system,
                                       bool use_hw_secure_codecs) {
  mojom::PredictionFeatures features;
  features.profile = profile;
  features.video_size = video_size;
  features.frames_per_sec = frames_per_sec;
  features.key_system = key_system;
  features.use_hw_secure_codecs = use_hw_secure_codecs;
  return features;
}

mojom::PredictionFeaturesPtr MakeFeaturesPtr(VideoCodecProfile profile,
                                             const gfx::Size& video_size,
                                             double frames_per_sec,
                                             const std::string& key_system,
                                             bool use_hw_secure_codecs) {
  mojom::PredictionFeaturesPtr features = mojom::PredictionFeatures::New();
  *features = MakeFeatures(profile, video_size, frames_per_sec, key_system,
                           use_hw_secure_codecs);
  return features;
}

mojom::PredictionTargets MakeTargets(uint32_t frames_decoded,
                                     uint32_t frames_dropped,
                                     uint32_t frames_power_efficient) {
  mojom::PredictionTargets targets;
  targets.frames_decoded = frames_decoded;
  targets.frames_dropped = frames_dropped;
  targets.frames_power_efficient = frames_power_efficient;
  return targets;
}

mojom::PredictionTargetsPtr MakeTargetsPtr(uint32_t frames_decoded,
                                           uint32_t frames_dropped,
                                           uint32_t frames_power_efficient) {
  mojom::PredictionTargetsPtr targets = mojom::PredictionTargets::New();
  *targets =
      MakeTargets(frames_decoded, frames_dropped, frames_power_efficient);
  return targets;
}

}  // namespace media
