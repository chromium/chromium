// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/abr_algorithm.h"

#include <algorithm>
#include <array>

#include "base/check_op.h"

namespace media::hls {

EwmaAbrAlgorithm::~EwmaAbrAlgorithm() = default;

void EwmaAbrAlgorithm::UpdateNetworkSpeed(uint64_t network_bps) {
  weighted_old_ = weighted_old_ * 0.8 + network_bps * 0.2;
  weighted_new_ = weighted_new_ * 0.2 + network_bps * 0.8;
}

uint64_t EwmaAbrAlgorithm::GetABRSpeed() const {
  return std::min(weighted_new_, weighted_old_);
}

FixedAbrAlgorithm::FixedAbrAlgorithm(uint64_t network_bps)
    : fixed_bps_(network_bps) {}

FixedAbrAlgorithm::~FixedAbrAlgorithm() = default;

void FixedAbrAlgorithm::UpdateNetworkSpeed(uint64_t network_bps) {
  fixed_bps_ = network_bps;
}

uint64_t FixedAbrAlgorithm::GetABRSpeed() const {
  return fixed_bps_;
}

}  // namespace media::hls
