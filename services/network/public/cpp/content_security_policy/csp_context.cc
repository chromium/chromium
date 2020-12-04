// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/content_security_policy/csp_context.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"

namespace network {

namespace {

// Helper function that returns true if |policy| should be checked under
// |check_csp_disposition|.
bool ShouldCheckPolicy(const mojom::ContentSecurityPolicyPtr& policy,
                       CSPContext::CheckCSPDisposition check_csp_disposition) {
  switch (check_csp_disposition) {
    case CSPContext::CHECK_REPORT_ONLY_CSP:
      return policy->header->type == mojom::ContentSecurityPolicyType::kReport;
    case CSPContext::CHECK_ENFORCED_CSP:
      return policy->header->type == mojom::ContentSecurityPolicyType::kEnforce;
    case CSPContext::CHECK_ALL_CSP:
      return true;
  }
  NOTREACHED();
  return true;
}

}  // namespace

CSPContext::CSPContext() = default;
CSPContext::~CSPContext() = default;

bool CSPContext::IsAllowedByCsp(mojom::CSPDirectiveName directive_name,
                                const GURL& url,
                                bool has_followed_redirect,
                                bool is_response_check,
                                const mojom::SourceLocationPtr& source_location,
                                CheckCSPDisposition check_csp_disposition,
                                bool is_form_submission) {
  bool allow = true;
  for (const auto& policy : policies_) {
    if (ShouldCheckPolicy(policy, check_csp_disposition)) {
      allow &= CheckContentSecurityPolicy(
          policy, directive_name, url, has_followed_redirect, is_response_check,
          this, source_location, is_form_submission);
    }
  }

  DCHECK(allow || check_csp_disposition != CSPContext::CHECK_REPORT_ONLY_CSP);

  return allow;
}

bool CSPContext::SchemeShouldBypassCSP(const base::StringPiece& scheme) {
  return false;
}

void CSPContext::SanitizeDataForUseInCspViolation(
    bool has_followed_redirect,
    mojom::CSPDirectiveName directive,
    GURL* blocked_url,
    network::mojom::SourceLocation* source_location) const {}

void CSPContext::ReportContentSecurityPolicyViolation(
    network::mojom::CSPViolationPtr violation) {}

}  // namespace network
