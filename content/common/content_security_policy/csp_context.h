// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_SECURITY_POLICY_CSP_CONTEXT_H_
#define CONTENT_COMMON_CONTENT_SECURITY_POLICY_CSP_CONTEXT_H_

#include <vector>

#include "base/optional.h"
#include "content/common/content_export.h"
#include "content/common/content_security_policy/content_security_policy.h"
#include "content/common/content_security_policy_header.h"
#include "content/common/navigation_params.h"
#include "services/network/public/mojom/content_security_policy.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

struct CSPViolationParams;

// A CSPContext represents the system on which the Content-Security-Policy are
// enforced. One must define via its virtual methods how to report violations
// and what is the set of scheme that bypass the CSP. Its main implementation
// is in content/browser/frame_host/render_frame_host_impl.h
class CONTENT_EXPORT CSPContext {
 public:
  // This enum represents what set of policies should be checked by
  // IsAllowedByCsp().
  enum CheckCSPDisposition {
    // Only check report-only policies.
    CHECK_REPORT_ONLY_CSP,
    // Only check enforced policies. (Note that enforced policies can still
    // trigger reports.)
    CHECK_ENFORCED_CSP,
    // Check all policies.
    CHECK_ALL_CSP,
  };

  CSPContext();
  virtual ~CSPContext();

  // Check if an |url| is allowed by the set of Content-Security-Policy. It will
  // report any violation by:
  // * displaying a console message.
  // * triggering the "SecurityPolicyViolation" javascript event.
  // * sending a JSON report to any uri defined with the "report-uri" directive.
  // Returns true when the request can proceed, false otherwise.
  bool IsAllowedByCsp(CSPDirective::Name directive_name,
                      const GURL& url,
                      bool has_followed_redirect,
                      bool is_response_check,
                      const SourceLocation& source_location,
                      CheckCSPDisposition check_csp_disposition,
                      bool is_form_submission);

  // Returns true if the request URL needs to be modified (e.g. upgraded to
  // HTTPS) according to the CSP.
  bool ShouldModifyRequestUrlForCsp(bool is_suresource_or_form_submssion);

  // If the scheme of |url| is HTTP, this upgrades it to HTTPS, otherwise it
  // doesn't modify it.
  void ModifyRequestUrlForCsp(GURL* url);

  void SetSelf(const url::Origin origin);
  void SetSelf(const CSPSource& self_source);

  // When a CSPSourceList contains 'self', the url is allowed when it match the
  // CSPSource returned by this function.
  // Sometimes there is no 'self' source. It means that the current origin is
  // unique and no urls will match 'self' whatever they are.
  // Note: When there is a 'self' source, its scheme is guaranteed to be
  // non-empty.
  const base::Optional<CSPSource>& self_source() { return self_source_; }

  virtual void ReportContentSecurityPolicyViolation(
      const CSPViolationParams& violation_params);

  void ResetContentSecurityPolicies() { policies_.clear(); }
  void AddContentSecurityPolicy(const ContentSecurityPolicy& policy) {
    policies_.push_back(policy);
  }

  virtual bool SchemeShouldBypassCSP(const base::StringPiece& scheme);

  // For security reasons, some urls must not be disclosed cross-origin in
  // violation reports. This includes the blocked url and the url of the
  // initiator of the navigation. This information is potentially transmitted
  // between different renderer processes.
  // TODO(arthursonzogni): Stop hiding sensitive parts of URLs in console error
  // messages as soon as there is a way to send them to the devtools process
  // without the round trip in the renderer process.
  // See https://crbug.com/721329
  virtual void SanitizeDataForUseInCspViolation(
      bool has_followed_redirect,
      CSPDirective::Name directive,
      GURL* blocked_url,
      SourceLocation* source_location) const;

 private:
  base::Optional<CSPSource> self_source_;
  std::vector<ContentSecurityPolicy> policies_;

  DISALLOW_COPY_AND_ASSIGN(CSPContext);
};

// Used in CSPContext::ReportViolation()
struct CONTENT_EXPORT CSPViolationParams {
  CSPViolationParams();
  CSPViolationParams(const std::string& directive,
                     const std::string& effective_directive,
                     const std::string& console_message,
                     const GURL& blocked_url,
                     const std::vector<std::string>& report_endpoints,
                     bool use_reporting_api,
                     const std::string& header,
                     network::mojom::ContentSecurityPolicyType disposition,
                     bool after_redirect,
                     const SourceLocation& source_location);
  CSPViolationParams(const CSPViolationParams& other);
  ~CSPViolationParams();

  // The name of the directive that violates the policy. |directive| might be a
  // directive that serves as a fallback to the |effective_directive|.
  std::string directive;

  // The name the effective directive that was checked against.
  std::string effective_directive;

  // The console message to be displayed to the user.
  std::string console_message;

  // The URL that was blocked by the policy.
  GURL blocked_url;

  // The set of endpoints where a report of the violation should be sent.
  // Based on 'use_reporting_api' it can be either a set of group_names (when
  // 'use_reporting_api' = true) or a set of URLs. This means that it's not
  // possible to use both methods of reporting. This is by design.
  std::vector<std::string> report_endpoints;

  // Whether to use the reporting api or not.
  bool use_reporting_api;

  // The raw content security policy header that was violated.
  std::string header;

  // Each policy has an associated disposition, which is either "enforce" or
  // "report".
  network::mojom::ContentSecurityPolicyType disposition;

  // Whether or not the violation happens after a redirect.
  bool after_redirect;

  // The source code location that triggered the blocked navigation.
  SourceLocation source_location;
};

}  // namespace content
#endif  // CONTENT_COMMON_CONTENT_SECURITY_POLICY_CSP_CONTEXT_H_
