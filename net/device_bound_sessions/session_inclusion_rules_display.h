// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_SESSION_INCLUSION_RULES_DISPLAY_H_
#define NET_DEVICE_BOUND_SESSIONS_SESSION_INCLUSION_RULES_DISPLAY_H_

#include <string>
#include <vector>

#include "net/base/net_export.h"
#include "net/device_bound_sessions/url_rule_display.h"

namespace net::device_bound_sessions {

// This class represents a display-friendly version of session inclusion rules.
// Used for DevTools.
struct NET_EXPORT SessionInclusionRulesDisplay {
  SessionInclusionRulesDisplay();
  SessionInclusionRulesDisplay(std::string origin,
                               bool include_site,
                               std::vector<UrlRuleDisplay> url_rules);
  ~SessionInclusionRulesDisplay();

  SessionInclusionRulesDisplay(const SessionInclusionRulesDisplay&);
  SessionInclusionRulesDisplay& operator=(const SessionInclusionRulesDisplay&);

  SessionInclusionRulesDisplay(SessionInclusionRulesDisplay&&) noexcept;
  SessionInclusionRulesDisplay& operator=(
      SessionInclusionRulesDisplay&&) noexcept;

  friend bool operator==(const SessionInclusionRulesDisplay&,
                         const SessionInclusionRulesDisplay&) = default;

  std::string origin;
  bool include_site = false;
  std::vector<UrlRuleDisplay> url_rules;
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_SESSION_INCLUSION_RULES_DISPLAY_H_
