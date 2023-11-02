// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "device/vr/util/sample_queue.h"

namespace device {

SampleQueue::SampleQueue(size_t window_size) : window_size_(window_size) {
  samples_.reserve(window_size);
}

SampleQueue::~SampleQueue() = default;

void SampleQueue::AddSample(int64_t value) {
  sum_ += value;

  if (samples_.size() < window_size_) {
    samples_.push_back(value);
  } else {
    sum_ -= samples_[current_index_];
    samples_[current_index_] = value;
  }

  ++current_index_;
  if (current_index_ >= window_size_) {
    current_index_ = 0;
  }
}

}  // namespace device
