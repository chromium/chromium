// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CONTENT_SECURITY_POLICY_CSP_CONTEXT_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CONTENT_SECURITY_POLICY_CSP_CONTEXT_H_

#include "services/network/public/mojom/content_security_policy.mojom.h"

class GURL;

namespace url {
class Origin;
}

namespace network {

// A CSPContext represents the Document where the Content-Security-Policy are
// checked. One must define via its virtual methods how to report violations
// and what is the set of scheme that bypass the CSP. Its main implementation
// is in content/browser/renderer_host/render_frame_host_impl.h
class COMPONENT_EXPORT(NETWORK_CPP) CSPContext {
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
  // - displaying a console message.
  // - triggering the "SecurityPolicyViolation" javascript event.
  // - sending a JSON report to any uri defined with the "report-uri" directive.
  // Returns true when the request can proceed, false otherwise.
  bool IsAllowedByCsp(mojom::CSPDirectiveName directive_name,
                      const GURL& url,
                      bool has_followed_redirect,
                      bool is_response_check,
                      const mojom::SourceLocationPtr& source_location,
                      CheckCSPDisposition check_csp_disposition,
                      bool is_form_submission);

  // Called when IsAllowedByCsp return false. Implementer of CSPContext must
  // display an error message and send reports using |violation|.
  virtual void ReportContentSecurityPolicyViolation(
      mojom::CSPViolationPtr violation);

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
      mojom::CSPDirectiveName directive,
      GURL* blocked_url,
      mojom::SourceLocation* source_location) const;

  // Returns true if the request URL needs to be modified (e.g. upgraded to
  // HTTPS) according to the CSP.
  bool ShouldModifyRequestUrlForCsp(bool is_subresource_or_form_submssion);

  virtual bool SchemeShouldBypassCSP(const base::StringPiece& scheme);

  // TODO(arthursonzogni): This is an interface. Stop storing object in it.
  void ResetContentSecurityPolicies() { policies_.clear(); }
  void AddContentSecurityPolicy(mojom::ContentSecurityPolicyPtr policy) {
    policies_.push_back(std::move(policy));
  }
  const std::vector<mojom::ContentSecurityPolicyPtr>&
  ContentSecurityPolicies() {
    return policies_;
  }

  void SetSelf(const url::Origin& origin);
  void SetSelf(mojom::CSPSourcePtr self_source);

  // When a CSPSourceList contains 'self', the url is allowed when it match the
  // CSPSource returned by this function.
  // Sometimes there is no 'self' source. It means that the current origin is
  // unique and no urls will match 'self' whatever they are.
  // Note: When there is a 'self' source, its scheme is guaranteed to be
  // non-empty.
  const mojom::CSPSourcePtr& self_source() { return self_source_; }

 private:
  // TODO(arthursonzogni): This is an interface. Stop storing object in it.
  mojom::CSPSourcePtr self_source_;  // Nullable.
  std::vector<mojom::ContentSecurityPolicyPtr> policies_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CONTENT_SECURITY_POLICY_CSP_CONTEXT_H_
