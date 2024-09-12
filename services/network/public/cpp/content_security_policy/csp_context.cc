// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/content_security_policy/csp_context.h"

#include "base/containers/contains.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "url/url_util.h"

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
  NOTREACHED_IN_MIGRATION();
  return true;
}

}  // namespace

CSPContext::CSPContext() = default;
CSPContext::~CSPContext() = default;

CSPCheckResult CSPContext::IsAllowedByCsp(
    const std::vector<mojom::ContentSecurityPolicyPtr>& policies,
    mojom::CSPDirectiveName directive_name,
    const GURL& url,
    const GURL& url_before_redirects,
    bool has_followed_redirect,
    const mojom::SourceLocationPtr& source_location,
    CheckCSPDisposition check_csp_disposition,
    bool is_form_submission,
    bool is_opaque_fenced_frame) {
  CSPCheckResult result = CSPCheckResult::Allowed();
  for (const auto& policy : policies) {
    if (ShouldCheckPolicy(policy, check_csp_disposition)) {
      result &= CheckContentSecurityPolicy(
          policy, directive_name, url, url_before_redirects,
          has_followed_redirect, this, source_location, is_form_submission,
          is_opaque_fenced_frame);
    }
  }

  DCHECK(result.IsAllowed() ||
         check_csp_disposition != CSPContext::CHECK_REPORT_ONLY_CSP);

  return result;
}

bool CSPContext::SchemeShouldBypassCSP(std::string_view scheme) {
  // Blink uses its SchemeRegistry to check if a scheme should be bypassed.
  // It can't be used on the browser process. It is used for two things:
  // 1) Bypassing the "chrome-extension" scheme when chrome is built with the
  //    extensions support.
  // 2) Bypassing arbitrary scheme for testing purpose only in blink and in V8.
  // TODO(arthursonzogni): url::GetBypassingCSPScheme() is used instead of the
  // blink::SchemeRegistry. It contains 1) but not 2).
  const auto& bypassing_schemes = url::GetCSPBypassingSchemes();
  return base::Contains(bypassing_schemes, scheme);
}

void CSPContext::SanitizeDataForUseInCspViolation(
    mojom::CSPDirectiveName directive,
    GURL* blocked_url,
    network::mojom::SourceLocation* source_location) const {}

void CSPContext::ReportContentSecurityPolicyViolation(
    network::mojom::CSPViolationPtr violation) {}

}  // namespace network
