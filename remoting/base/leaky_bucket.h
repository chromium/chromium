// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_LEAKY_BUCKET_H_
#define REMOTING_BASE_LEAKY_BUCKET_H_

#include "base/time/time.h"

namespace remoting {

class LeakyBucket {
 public:
  static const int kUnlimitedDepth = -1;

  // |depth| specifies depth of the bucket in drops. kUnlimitedDepth indicate
  // that bucket size is unlimited. |rate| is specified in drops per second.
  LeakyBucket(int depth, int rate);

  LeakyBucket(const LeakyBucket&) = delete;
  LeakyBucket& operator=(const LeakyBucket&) = delete;

  ~LeakyBucket();

  // If the bucket can fit |drops| then adds them and returns true. Otherwise
  // returns false.
  bool RefillOrSpill(int drops, base::TimeTicks now);

  // Updates rate.
  void UpdateRate(int new_rate, base::TimeTicks now);

  // Returns time when the bucket will be empty. The returned value may be in
  // the past.
  base::TimeTicks GetEmptyTime();

  int rate() { return rate_; }

 private:
  void UpdateLevel(base::TimeTicks now);

  int depth_;
  int rate_;

  // |current_level_| stores water level at |level_updated_time_|. Updated in
  // UpdateLevel(), which is called from RefillOrSpill() and UpdateRate().
  int current_level_;
  base::TimeTicks level_updated_time_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_LEAKY_BUCKET_H_
