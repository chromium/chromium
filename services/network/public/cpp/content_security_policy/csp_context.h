// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CONTENT_SECURITY_POLICY_CSP_CONTEXT_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CONTENT_SECURITY_POLICY_CSP_CONTEXT_H_

#include <string_view>

#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

class GURL;

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

  // Check if an |url| is allowed by the set of Content-Security-Policy
  // |policies|. It will report any violation by:
  // - displaying a console message.
  // - triggering the "SecurityPolicyViolation" javascript event.
  // - sending a JSON report to any uri defined with the "report-uri" directive.
  // Return a CSPCheckResult that allows when the request can proceed, false
  // otherwise.
  // The field |allowed_if_wildcard_does_not_match_ws| is true assuming '*'
  // doesn't match ws or wss, and |allowed_if_wildcard_does_not_match_ftp|
  // assumes
  // '*' doesn't match ftp. These two are only for logging purposes.
  // Note that when |is_opaque_fenced_frame| is true only https scheme source
  // will be matched and |url| might be disregarded.
  CSPCheckResult IsAllowedByCsp(
      const std::vector<mojom::ContentSecurityPolicyPtr>& policies,
      mojom::CSPDirectiveName directive_name,
      const GURL& url,
      const GURL& url_before_redirects,
      bool has_followed_redirect,
      const mojom::SourceLocationPtr& source_location,
      CheckCSPDisposition check_csp_disposition,
      bool is_form_submission,
      bool is_opaque_fenced_frame = false);

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
      mojom::CSPDirectiveName directive,
      GURL* blocked_url,
      mojom::SourceLocation* source_location) const;

  // Returns true if the request URL needs to be modified (e.g. upgraded to
  // HTTPS) according to the CSP.
  bool ShouldModifyRequestUrlForCsp(bool is_subresource_or_form_submssion);

  // This is declared virtual only so that it can be overridden for unit
  // testing.
  virtual bool SchemeShouldBypassCSP(std::string_view scheme);
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CONTENT_SECURITY_POLICY_CSP_CONTEXT_H_
