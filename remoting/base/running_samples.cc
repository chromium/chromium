// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/running_samples.h"

#include <algorithm>

#include "base/check_op.h"

namespace remoting {

RunningSamples::RunningSamples(int window_size) : window_size_(window_size) {
  DCHECK_GT(window_size, 0);
}

RunningSamples::~RunningSamples() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void RunningSamples::Record(int64_t value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  data_points_.push_back(value);
  sum_ += value;

  if (data_points_.size() > window_size_) {
    sum_ -= data_points_.front();
    data_points_.pop_front();
  }
}

double RunningSamples::Average() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (data_points_.empty()) {
    return 0;
  }
  return static_cast<double>(sum_) / data_points_.size();
}

int64_t RunningSamples::Max() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (data_points_.empty()) {
    return 0;
  }

  return *std::max_element(data_points_.begin(), data_points_.end());
}

bool RunningSamples::IsEmpty() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return data_points_.empty();
}

}  // namespace remoting
