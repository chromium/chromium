// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_EFFECTS_CALCULATORS_VIDEO_EFFECTS_GRAPH_CONFIG_H_
#define SERVICES_VIDEO_EFFECTS_CALCULATORS_VIDEO_EFFECTS_GRAPH_CONFIG_H_

#include <vector>

#include "third_party/dawn/include/dawn/webgpu_cpp.h"

namespace video_effects {

// The ML model we use expects 256x144 input buffer.
inline constexpr uint32_t kInferenceInputBufferWidth = 256;
inline constexpr uint32_t kInferenceInputBufferHeight = 144;
inline constexpr wgpu::TextureFormat kInferenceInputBufferFormat =
    wgpu::TextureFormat::RGBA32Float;

enum BlurState {
  kDisabled,
  kEnabled,
};

// Runtime configuration of the video effects graph. Each and every frame can
// use a different runtime config when being processed.
struct RuntimeConfig {
  BlurState blur_state;
};

// Static configuration of the video effects graph. Once a graph is created
// with a given configuration, this configuration becomes immutable.
class StaticConfig {
 public:
  StaticConfig();
  explicit StaticConfig(std::vector<uint8_t> background_segmentation_model);
  ~StaticConfig();

  StaticConfig(const StaticConfig& other) = delete;
  StaticConfig& operator=(const StaticConfig& other) = delete;

  StaticConfig(StaticConfig&& other);
  StaticConfig& operator=(StaticConfig&& other);

  const std::vector<uint8_t>& background_segmentation_model() const;

 private:
  std::vector<uint8_t> background_segmentation_model_;
};

}  // namespace video_effects

#endif  // SERVICES_VIDEO_EFFECTS_CALCULATORS_VIDEO_EFFECTS_GRAPH_CONFIG_H_
