// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cors/preflight_timing_info.h"

namespace network {

namespace cors {

PreflightTimingInfo::PreflightTimingInfo() = default;
PreflightTimingInfo::PreflightTimingInfo(const PreflightTimingInfo& info) =
    default;
PreflightTimingInfo::~PreflightTimingInfo() = default;

bool PreflightTimingInfo::operator==(const PreflightTimingInfo& rhs) const {
  return start_time == rhs.start_time && finish_time == rhs.finish_time &&
         alpn_negotiated_protocol == rhs.alpn_negotiated_protocol &&
         connection_info == rhs.connection_info &&
         timing_allow_origin == rhs.timing_allow_origin &&
         transfer_size == rhs.transfer_size;
}

}  // namespace cors

}  // namespace network
