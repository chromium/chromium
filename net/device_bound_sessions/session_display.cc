// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_display.h"

namespace net::device_bound_sessions {

SessionDisplay::SessionDisplay() = default;

SessionDisplay::SessionDisplay(
    SessionKey key,
    const GURL& refresh_url,
    SessionInclusionRulesDisplay inclusion_rules,
    std::vector<CookieCravingDisplay> cookie_cravings,
    base::Time expiry_date,
    std::optional<std::string> cached_challenge,
    std::vector<std::string> allowed_refresh_initiators)
    : key(std::move(key)),
      refresh_url(refresh_url),
      inclusion_rules(std::move(inclusion_rules)),
      cookie_cravings(std::move(cookie_cravings)),
      expiry_date(std::move(expiry_date)),
      cached_challenge(std::move(cached_challenge)),
      allowed_refresh_initiators(std::move(allowed_refresh_initiators)) {}

SessionDisplay::~SessionDisplay() = default;

SessionDisplay::SessionDisplay(const SessionDisplay&) = default;
SessionDisplay& SessionDisplay::operator=(const SessionDisplay&) = default;

SessionDisplay::SessionDisplay(SessionDisplay&&) noexcept = default;
SessionDisplay& SessionDisplay::operator=(SessionDisplay&&) noexcept = default;

}  // namespace net::device_bound_sessions
