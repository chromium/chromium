// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CONTENT_SECURITY_POLICY_CONTENT_SECURITY_POLICY_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CONTENT_SECURITY_POLICY_CONTENT_SECURITY_POLICY_H_

#include <string_view>

#include "base/component_export.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

class GURL;

namespace url {
class Origin;
}

namespace net {
class HttpResponseHeaders;
}

namespace network {
class CSPContext;

namespace features {
// TODO(crbug.com/40070927): This feature flag can be removed after M126.
COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kCspStopMatchingWildcardDirectivesToFtp);
}  // namespace features

// The field |allowed_if_wildcard_does_not_match_ws| is the result assuming '*'
// doesn't match ws or wss, and |allowed_if_wildcard_does_not_match_ftp| is the
// result assuming '*' doesn't match ftp. These two are only for logging.
class COMPONENT_EXPORT(NETWORK_CPP) CSPCheckResult {
 public:
  explicit CSPCheckResult(bool);

  CSPCheckResult& operator&=(const CSPCheckResult&);

  bool operator==(const CSPCheckResult&) const;

  explicit operator bool() const;

  static CSPCheckResult Allowed();
  static CSPCheckResult Blocked();
  static CSPCheckResult AllowedOnlyIfWildcardMatchesWs();
  static CSPCheckResult AllowedOnlyIfWildcardMatchesFtp();

  bool WouldBlockIfWildcardDoesNotMatchWs() const;
  bool WouldBlockIfWildcardDoesNotMatchFtp() const;

  bool IsAllowed() const;

 private:
  CSPCheckResult(bool allowed,
                 bool allowed_if_wildcard_does_not_match_ws,
                 bool allowed_if_wildcard_does_not_match_ftp);

  bool allowed_;
  bool allowed_if_wildcard_does_not_match_ws_;
  bool allowed_if_wildcard_does_not_match_ftp_;
};

// Return the next Content Security Policy directive after |directive| in
// |original_directive|'s fallback list:
// https://w3c.github.io/webappsec-csp/#directive-fallback-list.
COMPONENT_EXPORT(NETWORK_CPP)
mojom::CSPDirectiveName CSPFallbackDirective(
    mojom::CSPDirectiveName directive,
    mojom::CSPDirectiveName original_directive);

// Parses the Content-Security-Policy headers specified in |headers| and appends
// the results into |out|.
//
// The |base_url| is used for resolving the URLs in the 'report-uri' directives.
// See https://w3c.github.io/webappsec-csp/#report-violation.
COMPONENT_EXPORT(NETWORK_CPP)
void AddContentSecurityPolicyFromHeaders(
    const net::HttpResponseHeaders& headers,
    const GURL& base_url,
    std::vector<mojom::ContentSecurityPolicyPtr>* out);

COMPONENT_EXPORT(NETWORK_CPP)
std::vector<mojom::ContentSecurityPolicyPtr> ParseContentSecurityPolicies(
    std::string_view header,
    mojom::ContentSecurityPolicyType type,
    mojom::ContentSecurityPolicySource source,
    const GURL& base_url);

// Parse and return the Allow-CSP-From header value from |headers|.
COMPONENT_EXPORT(NETWORK_CPP)
mojom::AllowCSPFromHeaderValuePtr ParseAllowCSPFromHeader(
    const net::HttpResponseHeaders& headers);

// Parses a CSP source expression.
// https://w3c.github.io/webappsec-csp/#source-lists
//
// Return false on errors.
// Adds parsing error messages to |parsing_errors|.
// Notice that this can return true and still add some parsing error message
// (for example, if there is a url with a non-empty query part).
COMPONENT_EXPORT(NETWORK_CPP)
bool ParseSource(mojom::CSPDirectiveName directive_name,
                 std::string_view expression,
                 mojom::CSPSource* csp_source,
                 std::vector<std::string>& parsing_errors);

// Return a CSPCheckResult that allows when the |policy| allows a request to the
// |url| in relation to the |directive| for a given |context|.
// Note: Any policy violation are reported to the |context|.
COMPONENT_EXPORT(NETWORK_CPP)
CSPCheckResult CheckContentSecurityPolicy(
    const mojom::ContentSecurityPolicyPtr& policy,
    mojom::CSPDirectiveName directive,
    const GURL& url,
    const GURL& url_before_redirects,
    bool has_followed_redirect,
    CSPContext* context,
    const mojom::SourceLocationPtr& source_location,
    bool is_form_submission,
    bool is_opaque_fenced_frame = false);

// Return true if the set of |policies| contains one "Upgrade-Insecure-request"
// directive.
COMPONENT_EXPORT(NETWORK_CPP)
bool ShouldUpgradeInsecureRequest(
    const std::vector<mojom::ContentSecurityPolicyPtr>& policies);

// Return true if the set of |policies| contains one "Treat-As-Public-Address"
// directive.
COMPONENT_EXPORT(NETWORK_CPP)
bool ShouldTreatAsPublicAddress(
    const std::vector<mojom::ContentSecurityPolicyPtr>& policies);

// Upgrade scheme of the |url| from HTTP to HTTPS and its port from 80 to 433
// (if needed). This is a no-op on non-HTTP and on potentially trustworthy URL.
COMPONENT_EXPORT(NETWORK_CPP)
void UpgradeInsecureRequest(GURL* url);

// Checks whether |policy| is a valid required CSP attribute according to
// https://w3c.github.io/webappsec-cspee/#iframe-csp-valid-attribute-value.
// |policy| must be a vector containing exactly one entry.
// The context can be null.
COMPONENT_EXPORT(NETWORK_CPP)
bool IsValidRequiredCSPAttr(
    const std::vector<mojom::ContentSecurityPolicyPtr>& policy,
    const mojom::ContentSecurityPolicy* context,
    std::string& error_message);

// Checks whether |policy_a| subsumes the policy list |policies_b| according to
// the algorithm https://w3c.github.io/webappsec-cspee/#subsume-policy-list.
COMPONENT_EXPORT(NETWORK_CPP)
bool Subsumes(const mojom::ContentSecurityPolicy& policy_a,
              const std::vector<mojom::ContentSecurityPolicyPtr>& policies_b);

COMPONENT_EXPORT(NETWORK_CPP)
std::string ToString(mojom::CSPDirectiveName name);

// Return true if |request_origin| is allowed by Allow-CSP-From header. Note
// that |allow_csp_from| can be a null pointer.
COMPONENT_EXPORT(NETWORK_CPP)
bool AllowCspFromAllowOrigin(
    const url::Origin& request_origin,
    const network::mojom::AllowCSPFromHeaderValue* allow_csp_from);

// Return true if the response allows the embedder to enforce arbitrary policy
// on its behalf. |required_csp| is modified so that its self_origin matches the
// correct origin. Specification:
// https://w3c.github.io/webappsec-cspee/#origin-allowed
COMPONENT_EXPORT(NETWORK_CPP)
bool AllowsBlanketEnforcementOfRequiredCSP(
    const url::Origin& request_origin,
    const GURL& response_url,
    const network::mojom::AllowCSPFromHeaderValue* allow_csp_from,
    network::mojom::ContentSecurityPolicyPtr& required_csp);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CONTENT_SECURITY_POLICY_CONTENT_SECURITY_POLICY_H_
