// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/connection_allowlist.h"

#include "components/url_pattern/simple_url_pattern_matcher.h"
#include "url/gurl.h"

namespace network {

ConnectionAllowlist::ConnectionAllowlist() = default;
ConnectionAllowlist::~ConnectionAllowlist() = default;

ConnectionAllowlist::ConnectionAllowlist(ConnectionAllowlist&&) = default;
ConnectionAllowlist& ConnectionAllowlist::operator=(ConnectionAllowlist&&) =
    default;

ConnectionAllowlist::ConnectionAllowlist(const ConnectionAllowlist&) = default;
ConnectionAllowlist& ConnectionAllowlist::operator=(
    const ConnectionAllowlist&) = default;

bool ConnectionAllowlist::operator==(const ConnectionAllowlist& other) const =
    default;

bool ConnectionAllowlistMatchesUrl(
    const ConnectionAllowlist& connection_allowlist,
    const GURL& url) {
  for (const auto& url_string : connection_allowlist.allowlist) {
    auto matcher = url_pattern::SimpleUrlPatternMatcher::Create(
        url_string, /*base_url=*/nullptr);
    if (!matcher.has_value()) {
      // TODO(crbug.com/447954811): This case should result in an issue
      // delivered to the devtools console (and ideally we'd avoid it
      // entirely by parsing these strings as URL Patterns when initially
      // parsing the header rather than here when enforcing it).
      continue;
    }
    if (matcher.value()->Match(url)) {
      return true;
    }
  }

  return false;
}

ConnectionAllowlists::ConnectionAllowlists() = default;
ConnectionAllowlists::~ConnectionAllowlists() = default;

ConnectionAllowlists::ConnectionAllowlists(ConnectionAllowlists&&) = default;
ConnectionAllowlists& ConnectionAllowlists::operator=(ConnectionAllowlists&&) =
    default;

ConnectionAllowlists::ConnectionAllowlists(const ConnectionAllowlists&) =
    default;
ConnectionAllowlists& ConnectionAllowlists::operator=(
    const ConnectionAllowlists&) = default;

bool ConnectionAllowlists::operator==(const ConnectionAllowlists& other) const =
    default;

}  // namespace network
