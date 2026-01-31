// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_inclusion_rules_display.h"

namespace net::device_bound_sessions {

SessionInclusionRulesDisplay::SessionInclusionRulesDisplay() = default;

SessionInclusionRulesDisplay::SessionInclusionRulesDisplay(
    std::string origin,
    bool include_site,
    std::vector<UrlRuleDisplay> url_rules)
    : origin(std::move(origin)),
      include_site(include_site),
      url_rules(std::move(url_rules)) {}

SessionInclusionRulesDisplay::~SessionInclusionRulesDisplay() = default;

SessionInclusionRulesDisplay::SessionInclusionRulesDisplay(
    const SessionInclusionRulesDisplay&) = default;
SessionInclusionRulesDisplay& SessionInclusionRulesDisplay::operator=(
    const SessionInclusionRulesDisplay&) = default;

SessionInclusionRulesDisplay::SessionInclusionRulesDisplay(
    SessionInclusionRulesDisplay&&) noexcept = default;
SessionInclusionRulesDisplay& SessionInclusionRulesDisplay::operator=(
    SessionInclusionRulesDisplay&&) noexcept = default;

}  // namespace net::device_bound_sessions
