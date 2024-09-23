// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/legacy_tech_cookie_issue_details.h"

#include "url/gurl.h"

namespace content {

LegacyTechCookieIssueDetails::LegacyTechCookieIssueDetails() = default;
LegacyTechCookieIssueDetails::LegacyTechCookieIssueDetails(
    const GURL& transfer_or_script_url,
    const std::string& name,
    const std::string& domain,
    const std::string& path,
    AccessOperation access_operation)
    : transfer_or_script_url(transfer_or_script_url),
      name(name),
      domain(domain),
      path(path),
      access_operation(access_operation) {}

LegacyTechCookieIssueDetails::LegacyTechCookieIssueDetails(
    const LegacyTechCookieIssueDetails& other) = default;
LegacyTechCookieIssueDetails::~LegacyTechCookieIssueDetails() = default;

}  // namespace content
