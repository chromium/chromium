// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quiche/overrides/quiche_platform_impl/quiche_time_utils_impl.h"

#include "base/time/time.h"

#include <iostream>

namespace quiche {
absl::optional<int64_t> QuicheUtcDateTimeToUnixSecondsInner(int year,
                                                            int month,
                                                            int day,
                                                            int hour,
                                                            int minute,
                                                            int second) {
  base::Time::Exploded exploded{
      year, month,
      0,  // day_of_week
      day,  hour,  minute, second,
  };
  base::Time time;
  if (!base::Time::FromUTCExploded(exploded, &time)) {
    return absl::nullopt;
  }
  return (time - base::Time::UnixEpoch()).InSeconds();
}

absl::optional<int64_t> QuicheUtcDateTimeToUnixSecondsImpl(int year,
                                                           int month,
                                                           int day,
                                                           int hour,
                                                           int minute,
                                                           int second) {
  // Handle leap seconds without letting any other irregularities happen.
  if (second == 60) {
    auto previous_second = QuicheUtcDateTimeToUnixSecondsInner(
        year, month, day, hour, minute, second - 1);
    if (!previous_second.has_value()) {
      return absl::nullopt;
    }
    return *previous_second + 1;
  }

  return QuicheUtcDateTimeToUnixSecondsInner(year, month, day, hour, minute,
                                             second);
}

}  // namespace quiche
