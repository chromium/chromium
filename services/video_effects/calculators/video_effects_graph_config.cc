// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_effects/calculators/video_effects_graph_config.h"

namespace video_effects {

StaticConfig::StaticConfig(std::vector<uint8_t> background_segmentation_model)
    : background_segmentation_model_(std::move(background_segmentation_model)) {
}

StaticConfig::StaticConfig() = default;
StaticConfig::~StaticConfig() = default;

StaticConfig::StaticConfig(StaticConfig&& other) = default;
StaticConfig& StaticConfig::operator=(StaticConfig&& other) = default;

const std::vector<uint8_t>& StaticConfig::background_segmentation_model()
    const {
  return background_segmentation_model_;
}

}  // namespace video_effects
