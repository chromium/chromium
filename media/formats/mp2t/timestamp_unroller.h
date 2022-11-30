// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP2T_TIMESTAMP_UNROLLER_H_
#define MEDIA_FORMATS_MP2T_TIMESTAMP_UNROLLER_H_

#include <stdint.h>

#include "media/base/media_export.h"

namespace media {
namespace mp2t {

class MEDIA_EXPORT TimestampUnroller {
 public:
  TimestampUnroller();

  TimestampUnroller(const TimestampUnroller&) = delete;
  TimestampUnroller& operator=(const TimestampUnroller&) = delete;

  ~TimestampUnroller();

  // Given that |timestamp| is coded using 33 bits (accuracy of MPEG-2 TS
  // timestamps), GetUnrolledTimestamp returns the corresponding unrolled
  // timestamp.
  // The unrolled timestamp is defined by:
  // |timestamp| + k * (2 ^ 33)
  // where k is estimated so that the unrolled timestamp is as close as
  // possible to the previous unrolled timestamp returned by this function
  // (if this function has not been called before, it will return the timestamp
  // unmodified).
  int64_t GetUnrolledTimestamp(int64_t timestamp);

  // Reset the TimestampUnroller to its initial state.
  void Reset();

 private:
  // Indicate whether the value of |previous_unrolled_timestamp_| is valid.
  bool is_previous_timestamp_valid_;

  // This is the last output of GetUnrolledTimestamp.
  int64_t previous_unrolled_timestamp_;
};

}  // namespace mp2t
}  // namespace media

#endif  // MEDIA_FORMATS_MP2T_TIMESTAMP_UNROLLER_H_
