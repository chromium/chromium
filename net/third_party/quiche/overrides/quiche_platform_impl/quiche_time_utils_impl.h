// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_TIME_UTILS_IMPL_H_
#define NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_TIME_UTILS_IMPL_H_

#include <cstdint>

#include "net/base/net_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace quiche {

NET_EXPORT_PRIVATE absl::optional<int64_t> QuicheUtcDateTimeToUnixSecondsImpl(
    int year,
    int month,
    int day,
    int hour,
    int minute,
    int second);

}  // namespace quiche

#endif  // NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_TIME_UTILS_IMPL_H_
