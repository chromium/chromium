// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_SESSION_DISPLAY_H_
#define NET_DEVICE_BOUND_SESSIONS_SESSION_DISPLAY_H_

#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/device_bound_sessions/cookie_craving_display.h"
#include "net/device_bound_sessions/session_inclusion_rules_display.h"
#include "net/device_bound_sessions/session_key.h"
#include "url/gurl.h"

namespace net::device_bound_sessions {

// This class represents a display-friendly version of a DBSC session. Used for
// DevTools.
struct NET_EXPORT SessionDisplay {
  SessionDisplay();
  SessionDisplay(SessionKey key,
                 const GURL& refresh_url,
                 SessionInclusionRulesDisplay inclusion_rules,
                 std::vector<CookieCravingDisplay> cookie_cravings,
                 base::Time expiry_date,
                 std::optional<std::string> cached_challenge,
                 std::vector<std::string> allowed_refresh_initiators);
  ~SessionDisplay();

  SessionDisplay(const SessionDisplay&);
  SessionDisplay& operator=(const SessionDisplay&);

  SessionDisplay(SessionDisplay&&) noexcept;
  SessionDisplay& operator=(SessionDisplay&&) noexcept;

  friend bool operator==(const SessionDisplay&,
                         const SessionDisplay&) = default;
  SessionKey key;
  GURL refresh_url;
  SessionInclusionRulesDisplay inclusion_rules;
  std::vector<CookieCravingDisplay> cookie_cravings;
  base::Time expiry_date;
  std::optional<std::string> cached_challenge;
  std::vector<std::string> allowed_refresh_initiators;
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_SESSION_DISPLAY_H_
