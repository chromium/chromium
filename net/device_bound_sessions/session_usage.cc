// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_usage.h"

namespace net::device_bound_sessions {

SessionUsage GetMaxUsage(const base::flat_map<SessionKey, SessionUsage>&
                             device_bound_session_usage) {
  auto max_usage = SessionUsage::kNoSiteMatchNotInScope;
  for (const auto& [key, usage] : device_bound_session_usage) {
    if (usage > max_usage) {
      max_usage = usage;
    }
  }
  return max_usage;
}

}  // namespace net::device_bound_sessions
