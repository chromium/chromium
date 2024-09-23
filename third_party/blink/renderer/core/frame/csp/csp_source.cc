// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/csp_source.h"

#include "services/network/public/mojom/content_security_policy.mojom-blink.h"
#include "third_party/blink/renderer/platform/weborigin/known_ports.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

enum class SchemeMatchingResult {
  kNotMatching,
  kMatchingUpgrade,
  kMatchingExact
};

enum class PortMatchingResult {
  kNotMatching,
  kMatchingWildcard,
  kMatchingUpgrade,
  kMatchingExact
};

SchemeMatchingResult SchemeMatches(
    const network::mojom::blink::CSPSource& source,
    const String& protocol,
    const String& self_protocol) {
  DCHECK_EQ(protocol, protocol.DeprecatedLower());
  const String& scheme =
      (source.scheme.empty() ? self_protocol : source.scheme);

  if (scheme == protocol)
    return SchemeMatchingResult::kMatchingExact;

  if ((scheme == "http" && protocol == "https") ||
      (scheme == "ws" && protocol == "wss")) {
    return SchemeMatchingResult::kMatchingUpgrade;
  }

  return SchemeMatchingResult::kNotMatching;
}

bool HostMatches(const network::mojom::blink::CSPSource& source,
                 const StringView& host) {
  if (source.is_host_wildcard) {
    if (source.host.empty()) {
      // host-part = "*"
      return true;
    }
    if (host.ToString().EndsWith(String("." + source.host))) {
      // host-part = "*." 1*host-char *( "." 1*host-char )
      return true;
    }
    return false;
  }
  return source.host == host;
}

bool HostMatches(const network::mojom::blink::CSPSource& source,
                 const KURL& url) {
  // Chromium currently has an issue handling non-special URLs. The url.Host()
  // function returns an empty string for them. See
  // crbug.com/40063064 for details.
  //
  // In the future, once non-special URLs are fully supported, we might consider
  // checking the host information for them too.
  //
  // For now, we check `url.IsStandard()` to maintain consistent behavior
  // regardless of the url::StandardCompliantNonSpecialSchemeURLParsing feature
  // state.
  if (!url.IsStandard()) {
    return HostMatches(source, "");
  }
  return HostMatches(source, url.Host());
}

bool PathMatches(const network::mojom::blink::CSPSource& source,
                 const StringView& url_path) {
  if (source.path.empty() || (source.path == "/" && url_path.empty()))
    return true;

  String path =
      DecodeURLEscapeSequences(url_path, DecodeURLMode::kUTF8OrIsomorphic);

  if (source.path.EndsWith("/"))
    return path.StartsWith(source.path);

  return path == source.path;
}

PortMatchingResult PortMatches(const network::mojom::blink::CSPSource& source,
                               const String& self_protocol,
                               int port,
                               const String& protocol) {
  if (source.is_port_wildcard)
    return PortMatchingResult::kMatchingWildcard;

  if (port == source.port) {
    if (port == url::PORT_UNSPECIFIED)
      return PortMatchingResult::kMatchingWildcard;
    return PortMatchingResult::kMatchingExact;
  }

  bool is_scheme_http;  // needed for detecting an upgrade when the port is 0
  is_scheme_http = source.scheme.empty()
                       ? "http" == self_protocol
                       : "http" == source.scheme;

  if ((source.port == 80 ||
       ((source.port == url::PORT_UNSPECIFIED || source.port == 443) &&
        is_scheme_http)) &&
      (port == 443 || (port == url::PORT_UNSPECIFIED &&
                       DefaultPortForProtocol(protocol) == 443))) {
    return PortMatchingResult::kMatchingUpgrade;
  }

  if (port == url::PORT_UNSPECIFIED) {
    if (IsDefaultPortForProtocol(source.port, protocol))
      return PortMatchingResult::kMatchingExact;
    return PortMatchingResult::kNotMatching;
  }

  if (source.port == url::PORT_UNSPECIFIED) {
    if (IsDefaultPortForProtocol(port, protocol))
      return PortMatchingResult::kMatchingExact;
    return PortMatchingResult::kNotMatching;
  }

  return PortMatchingResult::kNotMatching;
}

// Helper inline functions for Port and Scheme MatchingResult enums
bool inline RequiresUpgrade(const PortMatchingResult result) {
  return result == PortMatchingResult::kMatchingUpgrade;
}
bool inline RequiresUpgrade(const SchemeMatchingResult result) {
  return result == SchemeMatchingResult::kMatchingUpgrade;
}

bool inline CanUpgrade(const PortMatchingResult result) {
  return result == PortMatchingResult::kMatchingUpgrade ||
         result == PortMatchingResult::kMatchingWildcard;
}

bool inline CanUpgrade(const SchemeMatchingResult result) {
  return result == SchemeMatchingResult::kMatchingUpgrade;
}

}  // namespace

bool CSPSourceMatches(const network::mojom::blink::CSPSource& source,
                      const String& self_protocol,
                      const KURL& url,
                      ResourceRequest::RedirectStatus redirect_status) {
  SchemeMatchingResult schemes_match =
      SchemeMatches(source, url.Protocol(), self_protocol);
  if (schemes_match == SchemeMatchingResult::kNotMatching)
    return false;
  if (CSPSourceIsSchemeOnly(source))
    return true;
  bool paths_match =
      (redirect_status == ResourceRequest::RedirectStatus::kFollowedRedirect) ||
      PathMatches(source, url.GetPath());
  PortMatchingResult ports_match = PortMatches(
      source, self_protocol, url.HasPort() ? url.Port() : url::PORT_UNSPECIFIED,
      url.Protocol());

  // if either the scheme or the port would require an upgrade (e.g. from http
  // to https) then check that both of them can upgrade to ensure that we don't
  // run into situations where we only upgrade the port but not the scheme or
  // viceversa
  if ((RequiresUpgrade(schemes_match) || (RequiresUpgrade(ports_match))) &&
      (!CanUpgrade(schemes_match) || !CanUpgrade(ports_match))) {
    return false;
  }

  return HostMatches(source, url) &&
         ports_match != PortMatchingResult::kNotMatching && paths_match;
}

bool CSPSourceMatchesAsSelf(const network::mojom::blink::CSPSource& source,
                            const KURL& url) {
  // https://w3c.github.io/webappsec-csp/#match-url-to-source-expression
  // Step 4.
  SchemeMatchingResult schemes_match =
      SchemeMatches(source, url.Protocol(), source.scheme);

  if (url.Protocol() == "file" &&
      schemes_match == SchemeMatchingResult::kMatchingExact) {
    // Determining the origin of a file URL is left as an exercise to the reader
    // https://url.spec.whatwg.org/#concept-url-origin. Let's always match file
    // URLs against 'self' delivered from a file. This avoids inconsistencies
    // between file:/// and file://localhost/.
    return true;
  }

  bool hosts_match = HostMatches(source, url);
  PortMatchingResult ports_match = PortMatches(
      source, source.scheme, url.HasPort() ? url.Port() : url::PORT_UNSPECIFIED,
      url.Protocol());

  // check if the origin is exactly matching
  if (schemes_match == SchemeMatchingResult::kMatchingExact && hosts_match &&
      (ports_match == PortMatchingResult::kMatchingExact ||
       ports_match == PortMatchingResult::kMatchingWildcard)) {
    return true;
  }

  bool ports_match_or_defaults =
      (ports_match == PortMatchingResult::kMatchingExact ||
       ((IsDefaultPortForProtocol(source.port, source.scheme) ||
         source.port == url::PORT_UNSPECIFIED) &&
        (!url.HasPort() ||
         IsDefaultPortForProtocol(url.Port(), url.Protocol()))));

  return hosts_match && ports_match_or_defaults &&
         (url.Protocol() == "https" || url.Protocol() == "wss" ||
          source.scheme == "http");
}

bool CSPSourceIsSchemeOnly(const network::mojom::blink::CSPSource& source) {
  return source.host.empty() && (!source.is_host_wildcard);
}

}  // namespace blink
