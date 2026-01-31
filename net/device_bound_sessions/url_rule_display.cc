// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/url_rule_display.h"

namespace net::device_bound_sessions {

UrlRuleDisplay::UrlRuleDisplay() = default;

UrlRuleDisplay::UrlRuleDisplay(InclusionResult rule_type,
                               const std::string& host_pattern,
                               const std::string& path_prefix)
    : rule_type(rule_type),
      host_pattern(host_pattern),
      path_prefix(path_prefix) {}

UrlRuleDisplay::~UrlRuleDisplay() = default;

UrlRuleDisplay::UrlRuleDisplay(const UrlRuleDisplay&) = default;
UrlRuleDisplay& UrlRuleDisplay::operator=(const UrlRuleDisplay&) = default;

UrlRuleDisplay::UrlRuleDisplay(UrlRuleDisplay&&) noexcept = default;
UrlRuleDisplay& UrlRuleDisplay::operator=(UrlRuleDisplay&&) noexcept = default;

}  // namespace net::device_bound_sessions
