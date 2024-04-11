// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/message_size_estimator.h"

namespace mojo::internal {

// The number of samples to use to determine the size of the allocation.
constexpr size_t kSampleSize = 60;

MessageSizeEstimator::MessageSizeEstimator() = default;

MessageSizeEstimator::~MessageSizeEstimator() = default;

void MessageSizeEstimator::EnablePredictiveAllocation(uint32_t message_name) {
  auto [it, _] = samples_.insert(
      {message_name, std::make_unique<SlidingWindow>(kSampleSize)});
  it->second->AddSample(0);
}

size_t MessageSizeEstimator::EstimatePayloadSize(uint32_t message_name) const {
  auto it = samples_.find(message_name);
  if (it != samples_.end()) {
    return it->second->Max();
  }
  return 0;
}

void MessageSizeEstimator::TrackPayloadSize(uint32_t message_name,
                                            size_t size) {
  auto it = samples_.find(message_name);
  if (it != samples_.end()) {
    it->second->AddSample(size);
  }
}

}  // namespace mojo::internal
