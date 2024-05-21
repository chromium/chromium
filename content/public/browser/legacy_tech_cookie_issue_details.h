// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_LEGACY_TECH_COOKIE_ISSUE_DETAILS_H_
#define CONTENT_PUBLIC_BROWSER_LEGACY_TECH_COOKIE_ISSUE_DETAILS_H_

#include <string>

#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

// Container to send Cookie Issue Details to Legacy Tech Report.
struct CONTENT_EXPORT LegacyTechCookieIssueDetails {
  enum AccessOperation { kRead, kWrite };
  LegacyTechCookieIssueDetails();
  LegacyTechCookieIssueDetails(const GURL& transfer_or_script_url,
                               const std::string& name,
                               const std::string& domain,
                               const std::string& path,
                               AccessOperation access_operation);
  LegacyTechCookieIssueDetails(const LegacyTechCookieIssueDetails& other);
  ~LegacyTechCookieIssueDetails();

  bool operator==(const LegacyTechCookieIssueDetails&) const = default;

  GURL transfer_or_script_url;
  std::string name;
  std::string domain;
  std::string path;
  AccessOperation access_operation;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_LEGACY_TECH_COOKIE_ISSUE_DETAILS_H_
