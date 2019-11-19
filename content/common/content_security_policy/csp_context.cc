// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_security_policy/csp_context.h"
#include "content/public/common/origin_util.h"

namespace content {

namespace {

// Helper function that returns true if |policy| should be checked under
// |check_csp_disposition|.
bool ShouldCheckPolicy(const ContentSecurityPolicy& policy,
                       CSPContext::CheckCSPDisposition check_csp_disposition) {
  switch (check_csp_disposition) {
    case CSPContext::CHECK_REPORT_ONLY_CSP:
      return policy.header.type ==
             network::mojom::ContentSecurityPolicyType::kReport;
    case CSPContext::CHECK_ENFORCED_CSP:
      return policy.header.type ==
             network::mojom::ContentSecurityPolicyType::kEnforce;
    case CSPContext::CHECK_ALL_CSP:
      return true;
  }
  NOTREACHED();
  return true;
}

}  // namespace

CSPContext::CSPContext() {}
CSPContext::~CSPContext() {}

bool CSPContext::IsAllowedByCsp(CSPDirective::Name directive_name,
                                const GURL& url,
                                bool has_followed_redirect,
                                bool is_response_check,
                                const SourceLocation& source_location,
                                CheckCSPDisposition check_csp_disposition,
                                bool is_form_submission) {
  if (SchemeShouldBypassCSP(url.scheme_piece()))
    return true;

  bool allow = true;
  for (const auto& policy : policies_) {
    if (ShouldCheckPolicy(policy, check_csp_disposition)) {
      allow &= ContentSecurityPolicy::Allow(
          policy, directive_name, url, has_followed_redirect, is_response_check,
          this, source_location, is_form_submission);
    }
  }

  DCHECK(allow || check_csp_disposition != CSPContext::CHECK_REPORT_ONLY_CSP);

  return allow;
}

bool CSPContext::ShouldModifyRequestUrlForCsp(
    bool is_subresource_or_form_submission) {
  for (const auto& policy : policies_) {
    if (ContentSecurityPolicy::ShouldUpgradeInsecureRequest(policy) &&
        is_subresource_or_form_submission) {
      return true;
    }
  }
  return false;
}

void CSPContext::ModifyRequestUrlForCsp(GURL* url) {
  if (url->SchemeIs(url::kHttpScheme) && !IsOriginSecure(*url)) {
    // Updating the URL's scheme also implicitly updates the URL's port from 80
    // to 443 if needed.
    GURL::Replacements replacements;
    replacements.SetSchemeStr(url::kHttpsScheme);
    *url = url->ReplaceComponents(replacements);
  }
}

void CSPContext::SetSelf(const url::Origin origin) {
  self_source_.reset();

  // When the origin is unique, no URL should match with 'self'. That's why
  // |self_source_| stays undefined here.
  if (origin.opaque())
    return;

  if (origin.scheme() == url::kFileScheme) {
    self_source_ = CSPSource(url::kFileScheme, "", false, url::PORT_UNSPECIFIED,
                             false, "");
    return;
  }

  self_source_ = CSPSource(
      origin.scheme(), origin.host(), false,
      origin.port() == 0 ? url::PORT_UNSPECIFIED : origin.port(), false, "");

  DCHECK_NE("", self_source_->scheme);
}

void CSPContext::SetSelf(const CSPSource& self_source) {
  self_source_ = self_source;
}

bool CSPContext::SchemeShouldBypassCSP(const base::StringPiece& scheme) {
  return false;
}

void CSPContext::SanitizeDataForUseInCspViolation(
    bool has_followed_redirect,
    CSPDirective::Name directive,
    GURL* blocked_url,
    SourceLocation* source_location) const {
  return;
}

void CSPContext::ReportContentSecurityPolicyViolation(
    const CSPViolationParams& violation_params) {
  return;
}

CSPViolationParams::CSPViolationParams() = default;

CSPViolationParams::CSPViolationParams(
    const std::string& directive,
    const std::string& effective_directive,
    const std::string& console_message,
    const GURL& blocked_url,
    const std::vector<std::string>& report_endpoints,
    bool use_reporting_api,
    const std::string& header,
    network::mojom::ContentSecurityPolicyType disposition,
    bool after_redirect,
    const SourceLocation& source_location)
    : directive(directive),
      effective_directive(effective_directive),
      console_message(console_message),
      blocked_url(blocked_url),
      report_endpoints(report_endpoints),
      use_reporting_api(use_reporting_api),
      header(header),
      disposition(disposition),
      after_redirect(after_redirect),
      source_location(source_location) {}

CSPViolationParams::CSPViolationParams(const CSPViolationParams& other) =
    default;
CSPViolationParams::~CSPViolationParams() {}

}  // namespace content
