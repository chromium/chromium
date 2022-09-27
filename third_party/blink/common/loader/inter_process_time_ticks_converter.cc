// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/loader/inter_process_time_ticks_converter.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/strings/string_number_conversions.h"

namespace blink {

InterProcessTimeTicksConverter::InterProcessTimeTicksConverter(
    LocalTimeTicks local_lower_bound,
    LocalTimeTicks local_upper_bound,
    RemoteTimeTicks remote_lower_bound,
    RemoteTimeTicks remote_upper_bound)
    : local_range_(local_upper_bound - local_lower_bound),
      remote_lower_bound_(remote_lower_bound),
      remote_upper_bound_(remote_upper_bound) {
  RemoteTimeDelta remote_range = remote_upper_bound - remote_lower_bound;

  DCHECK_LE(LocalTimeDelta(), local_range_);
  DCHECK_LE(RemoteTimeDelta(), remote_range);

  if (remote_range.ToTimeDelta() <= local_range_.ToTimeDelta()) {
    // We fit!  Center the source range on the target range.
    range_conversion_rate_ = 1.0;
    base::TimeDelta diff =
        local_range_.ToTimeDelta() - remote_range.ToTimeDelta();

    local_base_time_ =
        local_lower_bound + LocalTimeDelta::FromTimeDelta(diff / 2);
    // When converting times, remote bounds should fall within local bounds.
    DCHECK_LE(local_lower_bound, ToLocalTimeTicks(remote_lower_bound));
    DCHECK_LE(ToLocalTimeTicks(remote_upper_bound), local_upper_bound);
    return;
  }

  // Interpolate values so that remote range will be will exactly fit into the
  // local range, if possible.
  DCHECK_GT(remote_range.ToTimeDelta().InMicroseconds(), 0);
  range_conversion_rate_ =
      local_range_.ToTimeDelta() / remote_range.ToTimeDelta();
  local_base_time_ = local_lower_bound;
}

LocalTimeTicks InterProcessTimeTicksConverter::ToLocalTimeTicks(
    RemoteTimeTicks remote_time_ticks) const {
  // If input time is "null", return another "null" time.
  if (remote_time_ticks.is_null())
    return LocalTimeTicks();

  RemoteTimeDelta remote_delta = remote_time_ticks - remote_lower_bound_;

  DCHECK_LE(remote_time_ticks, remote_upper_bound_);
  return local_base_time_ + ToLocalTimeDelta(remote_delta);
}

LocalTimeDelta InterProcessTimeTicksConverter::ToLocalTimeDelta(
    RemoteTimeDelta remote_delta) const {
  DCHECK_LE(remote_lower_bound_ + remote_delta, remote_upper_bound_);

  // For remote times that come before remote time range, apply just time
  // offset and ignore scaling, so as to avoid extrapolation error for values
  // long in the past.
  if (remote_delta <= RemoteTimeDelta())
    return LocalTimeDelta::FromTimeDelta(remote_delta.ToTimeDelta());

  return std::min(local_range_,
                  LocalTimeDelta::FromTimeDelta(remote_delta.ToTimeDelta() *
                                                range_conversion_rate_));
}

base::TimeDelta InterProcessTimeTicksConverter::GetSkewForMetrics() const {
  return remote_lower_bound_.ToTimeTicks() - local_base_time_.ToTimeTicks();
}

}  // namespace blink
