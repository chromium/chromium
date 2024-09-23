// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/content_security_policy/csp_source.h"

#include <sstream>
#include <string_view>

#include "base/check_op.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "url/url_canon.h"
#include "url/url_features.h"
#include "url/url_util.h"

namespace network {

namespace {

bool HasHost(const mojom::CSPSource& source) {
  return !source.host.empty() || source.is_host_wildcard;
}

bool DecodePath(std::string_view path, std::string* output) {
  url::RawCanonOutputT<char16_t> unescaped;
  url::DecodeURLEscapeSequences(path, url::DecodeURLMode::kUTF8OrIsomorphic,
                                &unescaped);
  return base::UTF16ToUTF8(unescaped.data(), unescaped.length(), output);
}

int DefaultPortForScheme(const std::string& scheme) {
  return url::DefaultPortForScheme(scheme);
}

// NotMatching is the only negative member, the rest are different types of
// matches. NotMatching should always be 0 to let if statements work nicely
enum class PortMatchingResult {
  NotMatching,
  MatchingWildcard,
  MatchingUpgrade,
  MatchingExact
};
enum class SchemeMatchingResult { NotMatching, MatchingUpgrade, MatchingExact };

SchemeMatchingResult MatchScheme(const std::string& scheme_a,
                                 const std::string& scheme_b) {
  if (scheme_a == scheme_b)
    return SchemeMatchingResult::MatchingExact;
  if ((scheme_a == url::kHttpScheme && scheme_b == url::kHttpsScheme) ||
      (scheme_a == url::kWsScheme && scheme_b == url::kWssScheme)) {
    return SchemeMatchingResult::MatchingUpgrade;
  }
  return SchemeMatchingResult::NotMatching;
}

SchemeMatchingResult SourceAllowScheme(const mojom::CSPSource& source,
                                       const GURL& url,
                                       const mojom::CSPSource& self_source) {
  // The source doesn't specify a scheme and the current origin is unique. In
  // this case, the url doesn't match regardless of its scheme.
  if (source.scheme.empty() && self_source.scheme.empty())
    return SchemeMatchingResult::NotMatching;

  // |allowed_scheme| is guaranteed to be non-empty.
  const std::string& allowed_scheme =
      source.scheme.empty() ? self_source.scheme : source.scheme;

  return MatchScheme(allowed_scheme, url.scheme());
}

bool SourceAllowHost(const mojom::CSPSource& source, const std::string& host) {
  if (source.is_host_wildcard) {
    if (source.host.empty())
      return true;
    // TODO(arthursonzogni): Chrome used to, incorrectly, match *.x.y to x.y.
    // The renderer version of this function counts how many times it happens.
    // It might be useful to do it outside of blink too.
    // See third_party/blink/renderer/core/frame/csp/csp_source.cc
    return base::EndsWith(host, '.' + source.host);
  } else {
    return host == source.host;
  }
}

bool SourceAllowHost(const mojom::CSPSource& source, const GURL& url) {
  // Chromium currently has an issue handling non-special URLs. The url.host()
  // function returns an empty string for them. See
  // crbug.com/40063064 for details.
  //
  // In the future, once non-special URLs are fully supported, we might consider
  // checking the host information for them too.
  //
  // For now, we check `url.IsStandard()` to maintain consistent behavior
  // regardless of the url::StandardCompliantNonSpecialSchemeURLParsing feature
  // state.
  return SourceAllowHost(source, url.IsStandard() ? url.host() : "");
}

PortMatchingResult SourceAllowPort(const mojom::CSPSource& source,
                                   int port,
                                   const std::string& scheme) {
  if (source.is_port_wildcard)
    return PortMatchingResult::MatchingWildcard;

  if (source.port == port) {
    if (source.port == url::PORT_UNSPECIFIED)
      return PortMatchingResult::MatchingWildcard;
    return PortMatchingResult::MatchingExact;
  }

  if (source.port == url::PORT_UNSPECIFIED) {
    if (DefaultPortForScheme(scheme) == port)
      return PortMatchingResult::MatchingWildcard;
  }

  if (port == url::PORT_UNSPECIFIED) {
    if (source.port == DefaultPortForScheme(scheme))
      return PortMatchingResult::MatchingWildcard;
  }

  int source_port = source.port;
  if (source_port == url::PORT_UNSPECIFIED)
    source_port = DefaultPortForScheme(source.scheme);

  if (port == url::PORT_UNSPECIFIED)
    port = DefaultPortForScheme(scheme);

  if (source_port == 80 && port == 443)
    return PortMatchingResult::MatchingUpgrade;

  return PortMatchingResult::NotMatching;
}

PortMatchingResult SourceAllowPort(const mojom::CSPSource& source,
                                   const GURL& url) {
  return SourceAllowPort(source, url.EffectiveIntPort(), url.scheme());
}

bool SourceAllowPath(const mojom::CSPSource& source, const std::string& path) {
  std::string path_decoded;
  if (!DecodePath(path, &path_decoded)) {
    // TODO(arthursonzogni): try to figure out if that could happen and how to
    // handle it.
    return false;
  }

  if (source.path.empty() || (source.path == "/" && path_decoded.empty()))
    return true;

  // If the path represents a directory.
  if (base::EndsWith(source.path, "/", base::CompareCase::SENSITIVE)) {
    return base::StartsWith(path_decoded, source.path,
                            base::CompareCase::SENSITIVE);
  }

  // The path represents a file.
  return source.path == path_decoded;
}

bool SourceAllowPath(const mojom::CSPSource& source,
                     const GURL& url,
                     bool has_followed_redirect) {
  if (has_followed_redirect)
    return true;

  return SourceAllowPath(source, url.path());
}

bool requiresUpgrade(PortMatchingResult result) {
  return result == PortMatchingResult::MatchingUpgrade;
}

bool requiresUpgrade(SchemeMatchingResult result) {
  return result == SchemeMatchingResult::MatchingUpgrade;
}

bool canUpgrade(PortMatchingResult result, CSPSourceContext context) {
  return (result == PortMatchingResult::MatchingUpgrade ||
          result == PortMatchingResult::MatchingWildcard) &&
         context == CSPSourceContext::ContentSecurityPolicy;
}

bool canUpgrade(SchemeMatchingResult result, CSPSourceContext context) {
  return result == SchemeMatchingResult::MatchingUpgrade &&
         context == CSPSourceContext::ContentSecurityPolicy;
}

}  // namespace

bool CSPSourceIsSchemeOnly(const mojom::CSPSource& source) {
  return !HasHost(source);
}

bool CheckCSPSource(const mojom::CSPSource& source,
                    const GURL& url,
                    const mojom::CSPSource& self_source,
                    CSPSourceContext context,
                    bool has_followed_redirect,
                    bool is_opaque_fenced_frame) {
  // Opaque fenced frames only allow https urls.
  // TODO(crbug.com/40195488): Update the DCHECK and the return condition below
  // if opaque fenced frames can map to non-https potentially trustworthy urls.
  if (is_opaque_fenced_frame)
    DCHECK_EQ(url.scheme(), url::kHttpsScheme);

  if (CSPSourceIsSchemeOnly(source)) {
    // Opaque fenced frames only match 'https:' scheme source. Returns true for
    // 'https:' scheme source assuming that the url has been validated on the
    // fenced frame side.
    if (is_opaque_fenced_frame)
      return source.scheme == url::kHttpsScheme;

    return SourceAllowScheme(source, url, self_source) !=
           SchemeMatchingResult::NotMatching;
  }

  // Only allow 'https://*:*' for opaque fenced frames. 'https://*' is not
  // allowed as it could leak data about ports.
  if (is_opaque_fenced_frame) {
    return source.scheme == url::kHttpsScheme && source.is_host_wildcard &&
           source.is_port_wildcard;
  }

  PortMatchingResult portResult = SourceAllowPort(source, url);
  SchemeMatchingResult schemeResult =
      SourceAllowScheme(source, url, self_source);
  if (requiresUpgrade(schemeResult) && !canUpgrade(portResult, context)) {
    return false;
  }
  if (requiresUpgrade(portResult) && !canUpgrade(schemeResult, context)) {
    return false;
  }
  return schemeResult != SchemeMatchingResult::NotMatching &&
         SourceAllowHost(source, url) &&
         portResult != PortMatchingResult::NotMatching &&
         SourceAllowPath(source, url, has_followed_redirect);
}

mojom::CSPSourcePtr CSPSourcesIntersect(const mojom::CSPSource& source_a,
                                        const mojom::CSPSource& source_b) {
  // If the original source expressions didn't have a scheme, we should have
  // filled that already with origin's scheme.
  DCHECK(!source_a.scheme.empty());
  DCHECK(!source_b.scheme.empty());

  auto result = mojom::CSPSource::New();
  if (MatchScheme(source_a.scheme, source_b.scheme) !=
      SchemeMatchingResult::NotMatching) {
    result->scheme = source_b.scheme;
  } else if (MatchScheme(source_b.scheme, source_a.scheme) !=
             SchemeMatchingResult::NotMatching) {
    result->scheme = source_a.scheme;
  } else {
    return nullptr;
  }

  if (CSPSourceIsSchemeOnly(source_a)) {
    auto new_result = source_b.Clone();
    new_result->scheme = result->scheme;
    return new_result;
  } else if (CSPSourceIsSchemeOnly(source_b)) {
    auto new_result = source_a.Clone();
    new_result->scheme = result->scheme;
    return new_result;
  }

  const std::string host_a =
      (source_a.is_host_wildcard ? "*." : "") + source_a.host;
  const std::string host_b =
      (source_b.is_host_wildcard ? "*." : "") + source_b.host;
  if (SourceAllowHost(source_a, host_b)) {
    result->host = source_b.host;
    result->is_host_wildcard = source_b.is_host_wildcard;
  } else if (SourceAllowHost(source_b, host_a)) {
    result->host = source_a.host;
    result->is_host_wildcard = source_a.is_host_wildcard;
  } else {
    return nullptr;
  }

  if (source_b.is_port_wildcard) {
    result->port = source_a.port;
    result->is_port_wildcard = source_a.is_port_wildcard;
  } else if (source_a.is_port_wildcard) {
    result->port = source_b.port;
  } else if (SourceAllowPort(source_a, source_b.port, source_b.scheme) !=
                 PortMatchingResult::NotMatching &&
             // If port_a is explicitly specified but port_b is omitted, then we
             // should take port_a instead of port_b, since port_a is stricter.
             !(source_a.port != url::PORT_UNSPECIFIED &&
               source_b.port == url::PORT_UNSPECIFIED)) {
    result->port = source_b.port;
  } else if (SourceAllowPort(source_b, source_a.port, source_a.scheme) !=
             PortMatchingResult::NotMatching) {
    result->port = source_a.port;
  } else {
    return nullptr;
  }

  if (SourceAllowPath(source_a, source_b.path))
    result->path = source_b.path;
  else if (SourceAllowPath(source_b, source_a.path))
    result->path = source_a.path;
  else
    return nullptr;

  return result;
}

// Check whether |source_a| subsumes |source_b|.
bool CSPSourceSubsumes(const mojom::CSPSource& source_a,
                       const mojom::CSPSource& source_b) {
  // If the original source expressions didn't have a scheme, we should have
  // filled that already with origin's scheme.
  DCHECK(!source_a.scheme.empty());
  DCHECK(!source_b.scheme.empty());

  if (MatchScheme(source_a.scheme, source_b.scheme) ==
      SchemeMatchingResult::NotMatching) {
    return false;
  }

  if (CSPSourceIsSchemeOnly(source_a))
    return true;
  if (CSPSourceIsSchemeOnly(source_b))
    return false;

  if (!SourceAllowHost(
          source_a, (source_b.is_host_wildcard ? "*." : "") + source_b.host)) {
    return false;
  }

  if (source_b.is_port_wildcard && !source_a.is_port_wildcard)
    return false;
  PortMatchingResult port_matching =
      SourceAllowPort(source_a, source_b.port, source_b.scheme);
  if (port_matching == PortMatchingResult::NotMatching)
    return false;

  if (!SourceAllowPath(source_a, source_b.path))
    return false;

  return true;
}

std::string ToString(const mojom::CSPSource& source) {
  // scheme
  if (CSPSourceIsSchemeOnly(source))
    return source.scheme + ":";

  std::stringstream text;
  if (!source.scheme.empty())
    text << source.scheme << "://";

  // host
  if (source.is_host_wildcard) {
    if (source.host.empty())
      text << "*";
    else
      text << "*." << source.host;
  } else {
    text << source.host;
  }

  // port
  if (source.is_port_wildcard)
    text << ":*";
  if (source.port != url::PORT_UNSPECIFIED)
    text << ":" << source.port;

  // path
  text << source.path;

  return text.str();
}

}  // namespace network
