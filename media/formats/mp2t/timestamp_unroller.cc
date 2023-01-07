// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp2t/timestamp_unroller.h"

#include "base/check_op.h"

namespace media {
namespace mp2t {

TimestampUnroller::TimestampUnroller()
    : is_previous_timestamp_valid_(false),
      previous_unrolled_timestamp_(0) {
}

TimestampUnroller::~TimestampUnroller() {
}

int64_t TimestampUnroller::GetUnrolledTimestamp(int64_t timestamp) {
  // Mpeg2 TS timestamps have an accuracy of 33 bits.
  const int nbits = 33;

  // Bitmask for the high 31 bits of the unrolled timestamp.
  const int64_t unrolled_time_high_mask = 0xFFFFFFFE00000000LL;

  // |timestamp| has a precision of |nbits|
  // so make sure the highest bits are set to 0.
  DCHECK_EQ((timestamp >> nbits), 0);

  if (!is_previous_timestamp_valid_) {
    previous_unrolled_timestamp_ = timestamp;
    is_previous_timestamp_valid_ = true;
    return timestamp;
  }

  // |timestamp| is known modulo 2^33, so estimate the highest bits
  // to minimize the discontinuity with the previous unrolled timestamp.
  // Three possibilities are considered to estimate the missing high bits
  // of |timestamp|. If the bits of the previous unrolled timestamp are
  // {b63, b62, ..., b0} and bits of |timestamp| are {0, ..., 0, a32, ..., a0}
  // then the 3 possibilities are:
  // 1) t1 = {b63, ..., b33, a32, ..., a0} (apply the same offset multiple
  //    of 2^33 as the one used for the previous timestamp)
  // 2) t0 = t1 - 2^33
  // 3) t2 = t1 + 2^33
  //
  // A few remarks:
  // - the purpose of the timestamp unroller is only to unroll timestamps
  // in such a way timestamp continuity is satisfied. It can generate negative
  // values during that process.
  // - possible overflows are not considered here since 64 bits on a 90kHz
  // timescale is way enough to represent several years of playback.
  int64_t time1 =
      (previous_unrolled_timestamp_ & unrolled_time_high_mask) | timestamp;
  int64_t time0 = time1 - (1LL << nbits);
  int64_t time2 = time1 + (1LL << nbits);

  // Select the min absolute difference with the current time
  // so as to ensure time continuity.
  int64_t diff0 = time0 - previous_unrolled_timestamp_;
  int64_t diff1 = time1 - previous_unrolled_timestamp_;
  int64_t diff2 = time2 - previous_unrolled_timestamp_;
  if (diff0 < 0)
    diff0 = -diff0;
  if (diff1 < 0)
    diff1 = -diff1;
  if (diff2 < 0)
    diff2 = -diff2;

  int64_t unrolled_time;
  int64_t min_diff;
  if (diff1 < diff0) {
    unrolled_time = time1;
    min_diff = diff1;
  } else {
    unrolled_time = time0;
    min_diff = diff0;
  }
  if (diff2 < min_diff)
    unrolled_time = time2;

  // Update the state of the timestamp unroller.
  previous_unrolled_timestamp_ = unrolled_time;

  return unrolled_time;
}

void TimestampUnroller::Reset() {
  is_previous_timestamp_valid_ = false;
  previous_unrolled_timestamp_ = 0;
}

}  // namespace mp2t
}  // namespace media
