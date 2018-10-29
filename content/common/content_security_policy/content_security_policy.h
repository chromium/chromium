// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_SECURITY_POLICY_CONTENT_SECURITY_POLICY_H_
#define CONTENT_COMMON_CONTENT_SECURITY_POLICY_CONTENT_SECURITY_POLICY_H_

#include <memory>
#include <vector>

#include "content/common/content_export.h"
#include "content/common/content_security_policy/csp_directive.h"
#include "content/common/content_security_policy_header.h"
#include "url/gurl.h"

namespace content {

class CSPContext;
struct SourceLocation;

// https://www.w3.org/TR/CSP3/#framework-policy
//
// A ContentSecurityPolicy is a collection of CSPDirectives which will be
// enforced upon requests.
struct CONTENT_EXPORT ContentSecurityPolicy {
  ContentSecurityPolicy();
  ContentSecurityPolicy(const ContentSecurityPolicyHeader& header,
                        const std::vector<CSPDirective>& directives,
                        const std::vector<std::string>& report_endpoints,
                        bool use_reporting_api);
  ContentSecurityPolicy(const ContentSecurityPolicy&);
  ~ContentSecurityPolicy();

  ContentSecurityPolicyHeader header;
  std::vector<CSPDirective> directives;
  std::vector<std::string> report_endpoints;
  bool use_reporting_api;

  std::string ToString() const;

  // Return true when the |policy| allows a request to the |url| in relation to
  // the |directive| for a given |context|.
  // Note: Any policy violation are reported to the |context|.
  static bool Allow(const ContentSecurityPolicy& policy,
                    CSPDirective::Name directive,
                    const GURL& url,
                    bool has_followed_redirect,
                    bool is_response_check,
                    CSPContext* context,
                    const SourceLocation& source_location,
                    bool is_form_submission);

  // Returns true if |policy| specifies that an insecure HTTP request should be
  // upgraded to HTTPS.
  static bool ShouldUpgradeInsecureRequest(const ContentSecurityPolicy& policy);
};

}  // namespace content
#endif  // CONTENT_COMMON_CONTENT_SECURITY_POLICY_CONTENT_SECURITY_POLICY_H_
