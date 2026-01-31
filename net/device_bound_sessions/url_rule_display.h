// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_URL_RULE_DISPLAY_H_
#define NET_DEVICE_BOUND_SESSIONS_URL_RULE_DISPLAY_H_

#include <string>

#include "net/base/net_export.h"
#include "net/device_bound_sessions/inclusion_result.h"

namespace net::device_bound_sessions {

// This class represents a display-friendly version of a URL rule, suitable for
// showing in UI.
struct NET_EXPORT UrlRuleDisplay {
  UrlRuleDisplay();
  UrlRuleDisplay(InclusionResult rule_type,
                 const std::string& host_pattern,
                 const std::string& path_prefix);
  ~UrlRuleDisplay();

  UrlRuleDisplay(const UrlRuleDisplay&);
  UrlRuleDisplay& operator=(const UrlRuleDisplay&);

  UrlRuleDisplay(UrlRuleDisplay&&) noexcept;
  UrlRuleDisplay& operator=(UrlRuleDisplay&&) noexcept;

  friend bool operator==(const UrlRuleDisplay&,
                         const UrlRuleDisplay&) = default;

  InclusionResult rule_type = InclusionResult::kExclude;
  std::string host_pattern;
  std::string path_prefix;
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_URL_RULE_DISPLAY_H_
