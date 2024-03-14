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
  if (message_name >= samples_.size()) {
    samples_.resize(message_name + 1);
  }
  samples_[message_name].emplace(kSampleSize);
  samples_[message_name]->AddSample(0);
}

size_t MessageSizeEstimator::EstimatePayloadSize(uint32_t message_name) const {
  if (message_name < samples_.size() && samples_[message_name]) {
    return samples_[message_name]->Max();
  }
  return 0;
}

void MessageSizeEstimator::TrackPayloadSize(uint32_t message_name,
                                            size_t size) {
  if (message_name < samples_.size() && samples_[message_name]) {
    samples_[message_name]->AddSample(size);
  }
}

}  // namespace mojo::internal
