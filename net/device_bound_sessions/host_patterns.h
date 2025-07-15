// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_HOST_PATTERNS_H_
#define NET_DEVICE_BOUND_SESSIONS_HOST_PATTERNS_H_

#include <string>

#include "net/base/net_export.h"

namespace net::device_bound_sessions {

// Returns if `host_pattern` is valid (i.e. can match any hosts). The
// `host_pattern` must either be a full domain (host piece), exactly '*', or a
// pattern containing a wildcard ('*' character) in the most-specific (leftmost)
// label position followed by a dot and the rest of the domain.
bool NET_EXPORT IsValidHostPattern(std::string_view host_pattern);

// Returns whether `host_patern` matches `host`.
bool NET_EXPORT MatchesHostPattern(std::string_view host_pattern,
                                   std::string_view host);

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_HOST_PATTERNS_H_
