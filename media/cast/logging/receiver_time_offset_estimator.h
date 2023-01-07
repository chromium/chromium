// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_LOGGING_RECEIVER_TIME_OFFSET_ESTIMATOR_H_
#define MEDIA_CAST_LOGGING_RECEIVER_TIME_OFFSET_ESTIMATOR_H_

#include "base/time/time.h"
#include "media/cast/logging/raw_event_subscriber.h"

namespace media {
namespace cast {

// Estimates receiver time offset based on raw events received.
// In most cases, the sender and receiver run on different time lines.
// In order to convert receiver time back to sender time (or vice versa)
// a certain time offset has to be applied.
// An implementation of this interface listens to raw events to figure out
// the bounds for the offset value (assuming the true offset value is constant
// over the lifetime of a cast session).
// The offset values provided here should be used as follows:
// - Convert from sender to receiver time: add offset value to sender timestamp.
// - Convert from receiver to sender time: subtract offset value from receiver
//   timestamp.
class ReceiverTimeOffsetEstimator : public RawEventSubscriber {
 public:
  ~ReceiverTimeOffsetEstimator() override {}

  // If bounds are known, assigns |lower_bound| and |upper_bound| with the
  // lower bound and upper bound for the offset value, respectively.
  // Returns true if bounds are known.
  virtual bool GetReceiverOffsetBounds(base::TimeDelta* lower_bound,
                                       base::TimeDelta* upper_bound) = 0;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_LOGGING_RECEIVER_TIME_OFFSET_ESTIMATOR_H_
