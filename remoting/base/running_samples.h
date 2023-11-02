// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_RUNNING_SAMPLES_H_
#define REMOTING_BASE_RUNNING_SAMPLES_H_

#include <stddef.h>
#include <stdint.h>

#include "base/containers/circular_deque.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"

namespace remoting {

// Calculates the maximum or average of the most recent N recorded samples.
// This is typically used to smooth out random variation in point samples
// over bandwidth, frame rate, etc.
class RunningSamples {
 public:
  // Constructs a running sample helper that stores |window_size| most
  // recent samples.
  explicit RunningSamples(int window_size);

  RunningSamples(const RunningSamples&) = delete;
  RunningSamples& operator=(const RunningSamples&) = delete;

  virtual ~RunningSamples();

  // Records a point sample.
  void Record(int64_t value);

  // Returns the average over up to |window_size| of the most recent samples.
  // 0 if no sample available
  double Average() const;

  // Returns the max over up to |window_size| of the most recent samples.
  // 0 if no sample available
  int64_t Max() const;

  // Whether there is at least one record.
  bool IsEmpty() const;

 private:
  // Stores the desired window size, as size_t to avoid casting when comparing
  // with the size of |data_points_|.
  const size_t window_size_;

  // Stores the |window_size| most recently recorded samples.
  base::circular_deque<int64_t> data_points_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Holds the sum of the samples in |data_points_|.
  int64_t sum_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace remoting

#endif  // REMOTING_BASE_RUNNING_SAMPLES_H_
