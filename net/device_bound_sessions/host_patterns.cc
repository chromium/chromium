// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/host_patterns.h"

#include "url/url_util.h"

namespace net::device_bound_sessions {

bool IsValidHostPattern(std::string_view host_pattern) {
  if (host_pattern.empty()) {
    return false;
  }

  if (host_pattern == "*") {
    return true;
  }

  size_t host_part_offset = host_pattern.starts_with("*.") ? 2u : 0u;
  // Don't allow '*' except in the initial position
  return host_pattern.substr(host_part_offset).find('*') == std::string::npos;
}

bool MatchesHostPattern(std::string_view host_pattern, std::string_view host) {
  if (host_pattern == "*") {
    return true;
  }

  if (host_pattern.starts_with("*.") &&
      host.ends_with(host_pattern.substr(1)) && !url::HostIsIPAddress(host)) {
    return true;
  }

  return host == host_pattern;
}

}  // namespace net::device_bound_sessions
