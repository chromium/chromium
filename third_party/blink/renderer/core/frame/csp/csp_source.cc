// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/csp_source.h"

#include "third_party/blink/public/platform/web_content_security_policy_struct.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/known_ports.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

constexpr int CSPSource::kPortUnspecified = -1;

CSPSource::CSPSource(ContentSecurityPolicy* policy,
                     const String& scheme,
                     const String& host,
                     int port,
                     const String& path,
                     WildcardDisposition host_wildcard,
                     WildcardDisposition port_wildcard)
    : policy_(policy),
      scheme_(scheme.DeprecatedLower()),
      host_(host),
      port_(port),
      path_(path),
      host_wildcard_(host_wildcard),
      port_wildcard_(port_wildcard) {}

CSPSource::CSPSource(ContentSecurityPolicy* policy, const CSPSource& other)
    : CSPSource(policy,
                other.scheme_,
                other.host_,
                other.port_,
                other.path_,
                other.host_wildcard_,
                other.port_wildcard_) {}

bool CSPSource::Matches(const KURL& url,
                        ResourceRequest::RedirectStatus redirect_status) const {
  SchemeMatchingResult schemes_match = SchemeMatches(url.Protocol());
  if (schemes_match == SchemeMatchingResult::kNotMatching)
    return false;
  if (IsSchemeOnly())
    return true;
  bool paths_match = (redirect_status == RedirectStatus::kFollowedRedirect) ||
                     PathMatches(url.GetPath());
  PortMatchingResult ports_match = PortMatches(
      url.HasPort() ? url.Port() : kPortUnspecified, url.Protocol());

  // if either the scheme or the port would require an upgrade (e.g. from http
  // to https) then check that both of them can upgrade to ensure that we don't
  // run into situations where we only upgrade the port but not the scheme or
  // viceversa
  if ((RequiresUpgrade(schemes_match) || (RequiresUpgrade(ports_match))) &&
      (!CanUpgrade(schemes_match) || !CanUpgrade(ports_match))) {
    return false;
  }

  return HostMatches(url.Host()) &&
         ports_match != PortMatchingResult::kNotMatching && paths_match;
}

bool CSPSource::MatchesAsSelf(const KURL& url) {
  // https://w3c.github.io/webappsec-csp/#match-url-to-source-expression
  // Step 4.
  SchemeMatchingResult schemes_match = SchemeMatches(url.Protocol());
  bool hosts_match = HostMatches(url.Host());
  PortMatchingResult ports_match = PortMatches(
      url.HasPort() ? url.Port() : kPortUnspecified, url.Protocol());

  // check if the origin is exactly matching
  if (schemes_match == SchemeMatchingResult::kMatchingExact && hosts_match &&
      (ports_match == PortMatchingResult::kMatchingExact ||
       ports_match == PortMatchingResult::kMatchingWildcard)) {
    return true;
  }

  String self_scheme =
      (scheme_.IsEmpty() ? policy_->GetSelfProtocol() : scheme_);

  bool ports_match_or_defaults =
      (ports_match == PortMatchingResult::kMatchingExact ||
       ((IsDefaultPortForProtocol(port_, self_scheme) ||
         port_ == kPortUnspecified) &&
        (!url.HasPort() ||
         IsDefaultPortForProtocol(url.Port(), url.Protocol()))));

  if (hosts_match && ports_match_or_defaults &&
      (url.Protocol() == "https" || url.Protocol() == "wss" ||
       self_scheme == "http")) {
    return true;
  }

  return false;
}

CSPSource::SchemeMatchingResult CSPSource::SchemeMatches(
    const String& protocol) const {
  DCHECK_EQ(protocol, protocol.DeprecatedLower());
  const String& scheme =
      (scheme_.IsEmpty() ? policy_->GetSelfProtocol() : scheme_);

  if (scheme == protocol)
    return SchemeMatchingResult::kMatchingExact;

  if ((scheme == "http" && protocol == "https") ||
      (scheme == "ws" && protocol == "wss")) {
    return SchemeMatchingResult::kMatchingUpgrade;
  }

  return SchemeMatchingResult::kNotMatching;
}

bool CSPSource::HostMatches(const String& host) const {
  bool match;

  bool equal_hosts = EqualIgnoringASCIICase(host_, host);
  if (host_wildcard_ == kHasWildcard) {
    if (host_.IsEmpty()) {
      // host-part = "*"
      match = true;
    } else {
      // host-part = "*." 1*host-char *( "." 1*host-char )
      match = host.EndsWithIgnoringCase(String("." + host_));
    }
  } else {
    // host-part = 1*host-char *( "." 1*host-char )
    match = equal_hosts;
  }

  return match;
}

bool CSPSource::PathMatches(const String& url_path) const {
  if (path_.IsEmpty() || (path_ == "/" && url_path.IsEmpty()))
    return true;

  String path =
      DecodeURLEscapeSequences(url_path, DecodeURLMode::kUTF8OrIsomorphic);

  if (path_.EndsWith("/"))
    return path.StartsWith(path_);

  return path == path_;
}

CSPSource::PortMatchingResult CSPSource::PortMatches(
    int port,
    const String& protocol) const {
  if (port_wildcard_ == kHasWildcard)
    return PortMatchingResult::kMatchingWildcard;

  if (port == port_) {
    if (port == kPortUnspecified)
      return PortMatchingResult::kMatchingWildcard;
    return PortMatchingResult::kMatchingExact;
  }

  bool is_scheme_http;  // needed for detecting an upgrade when the port is 0
  is_scheme_http = scheme_.IsEmpty() ? policy_->ProtocolEqualsSelf("http")
                                     : EqualIgnoringASCIICase("http", scheme_);

  if ((port_ == 80 ||
       ((port_ == kPortUnspecified || port_ == 443) && is_scheme_http)) &&
      (port == 443 ||
       (port == kPortUnspecified && DefaultPortForProtocol(protocol) == 443))) {
    return PortMatchingResult::kMatchingUpgrade;
  }

  if (port == kPortUnspecified) {
    if (IsDefaultPortForProtocol(port_, protocol))
      return PortMatchingResult::kMatchingExact;

    return PortMatchingResult::kNotMatching;
  }

  if (port_ == kPortUnspecified) {
    if (IsDefaultPortForProtocol(port, protocol))
      return PortMatchingResult::kMatchingExact;

    return PortMatchingResult::kNotMatching;
  }

  return PortMatchingResult::kNotMatching;
}

bool CSPSource::IsSchemeOnly() const {
  return host_.IsEmpty() && (host_wildcard_ == kNoWildcard);
}

network::mojom::blink::CSPSourcePtr CSPSource::ExposeForNavigationalChecks()
    const {
  return network::mojom::blink::CSPSource::New(
      scheme_ ? scheme_ : "",             // scheme
      host_ ? host_ : "",                 // host
      port_ == kPortUnspecified ? -1      /* url::PORT_UNSPECIFIED */
                                : port_,  // port
      path_ ? path_ : "",                 // path
      host_wildcard_ == kHasWildcard,     // is_host_wildcard
      port_wildcard_ == kHasWildcard      // is_port_wildcard
  );
}

void CSPSource::Trace(Visitor* visitor) const {
  visitor->Trace(policy_);
}

}  // namespace blink
