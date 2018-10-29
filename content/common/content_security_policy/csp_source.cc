// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/common/content_security_policy/csp_context.h"
#include "url/url_canon.h"
#include "url/url_util.h"

namespace content {

namespace {

bool DecodePath(const base::StringPiece& path, std::string* output) {
  url::RawCanonOutputT<base::char16> unescaped;
  url::DecodeURLEscapeSequences(path.data(), path.size(), &unescaped);
  return base::UTF16ToUTF8(unescaped.data(), unescaped.length(), output);
}

int DefaultPortForScheme(const std::string& scheme) {
  return url::DefaultPortForScheme(scheme.data(), scheme.size());
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

SchemeMatchingResult SourceAllowScheme(const CSPSource& source,
                                       const GURL& url,
                                       CSPContext* context) {
  // The source doesn't specify a scheme and the current origin is unique. In
  // this case, the url doesn't match regardless of its scheme.
  if (source.scheme.empty() && !context->self_source())
    return SchemeMatchingResult::NotMatching;

  // |allowed_scheme| is guaranteed to be non-empty.
  const std::string& allowed_scheme =
      source.scheme.empty() ? context->self_source()->scheme : source.scheme;

  if (url.SchemeIs(allowed_scheme))
    return SchemeMatchingResult::MatchingExact;

  // Implicitly allow using a more secure version of a protocol when the
  // non-secure one is allowed.
  if ((allowed_scheme == url::kHttpScheme && url.SchemeIs(url::kHttpsScheme)) ||
      (allowed_scheme == url::kWsScheme && url.SchemeIs(url::kWssScheme))) {
    return SchemeMatchingResult::MatchingUpgrade;
  }
  return SchemeMatchingResult::NotMatching;
}

bool SourceAllowHost(const CSPSource& source, const GURL& url) {
  if (source.is_host_wildcard) {
    if (source.host.empty())
      return true;
    // TODO(arthursonzogni): Chrome used to, incorrectly, match *.x.y to x.y.
    // The renderer version of this function count how many times it happens.
    // It might be useful to do it outside of blink too.
    // See third_party/WebKit/Source/core/frame/csp/CSPSource.cpp
    return base::EndsWith(url.host(), '.' + source.host,
                          base::CompareCase::INSENSITIVE_ASCII);
  } else
    return url.host() == source.host;
}

PortMatchingResult SourceAllowPort(const CSPSource& source, const GURL& url) {
  int url_port = url.EffectiveIntPort();

  if (source.is_port_wildcard)
    return PortMatchingResult::MatchingWildcard;

  if (source.port == url_port) {
    if (source.port == url::PORT_UNSPECIFIED)
      return PortMatchingResult::MatchingWildcard;
    return PortMatchingResult::MatchingExact;
  }

  if (source.port == url::PORT_UNSPECIFIED) {
    if (DefaultPortForScheme(url.scheme()) == url_port) {
      return PortMatchingResult::MatchingWildcard;
    }
    return PortMatchingResult::NotMatching;
  }

  int source_port = source.port;
  if (source_port == url::PORT_UNSPECIFIED)
    source_port = DefaultPortForScheme(source.scheme);

  if (source_port == 80 && url_port == 443)
    return PortMatchingResult::MatchingUpgrade;

  return PortMatchingResult::NotMatching;
}

bool SourceAllowPath(const CSPSource& source,
                     const GURL& url,
                     bool has_followed_redirect) {
  if (has_followed_redirect)
    return true;

  if (source.path.empty() || url.path().empty())
    return true;

  std::string url_path;
  if (!DecodePath(url.path(), &url_path)) {
    // TODO(arthursonzogni): try to figure out if that could happen and how to
    // handle it.
    return false;
  }

  // If the path represents a directory.
  if (base::EndsWith(source.path, "/", base::CompareCase::SENSITIVE))
    return base::StartsWith(url_path, source.path,
                            base::CompareCase::SENSITIVE);

  // The path represents a file.
  return source.path == url_path;
}

bool inline requiresUpgrade(const PortMatchingResult result) {
  return result == PortMatchingResult::MatchingUpgrade;
}
bool inline requiresUpgrade(const SchemeMatchingResult result) {
  return result == SchemeMatchingResult::MatchingUpgrade;
}
bool inline canUpgrade(const PortMatchingResult result) {
  return result == PortMatchingResult::MatchingUpgrade ||
         result == PortMatchingResult::MatchingWildcard;
}
bool inline canUpgrade(const SchemeMatchingResult result) {
  return result == SchemeMatchingResult::MatchingUpgrade;
}

}  // namespace

CSPSource::CSPSource()
    : scheme(),
      host(),
      is_host_wildcard(false),
      port(url::PORT_UNSPECIFIED),
      is_port_wildcard(false),
      path() {}

CSPSource::CSPSource(const std::string& scheme,
                     const std::string& host,
                     bool is_host_wildcard,
                     int port,
                     bool is_port_wildcard,
                     const std::string& path)
    : scheme(scheme),
      host(host),
      is_host_wildcard(is_host_wildcard),
      port(port),
      is_port_wildcard(is_port_wildcard),
      path(path) {
  DCHECK(!HasPort() || HasHost());  // port => host
  DCHECK(!HasPath() || HasHost());  // path => host
  DCHECK(!is_port_wildcard || port == url::PORT_UNSPECIFIED);
}

CSPSource::CSPSource(const CSPSource& source) = default;
CSPSource::~CSPSource() = default;

// static
bool CSPSource::Allow(const CSPSource& source,
                      const GURL& url,
                      CSPContext* context,
                      bool has_followed_redirect) {
  if (source.IsSchemeOnly())
    return SourceAllowScheme(source, url, context) !=
           SchemeMatchingResult::NotMatching;

  PortMatchingResult portResult = SourceAllowPort(source, url);
  SchemeMatchingResult schemeResult = SourceAllowScheme(source, url, context);

  if (requiresUpgrade(schemeResult) && !canUpgrade(portResult))
    return false;
  if (requiresUpgrade(portResult) && !canUpgrade(schemeResult))
    return false;

  return schemeResult != SchemeMatchingResult::NotMatching &&
         SourceAllowHost(source, url) &&
         portResult != PortMatchingResult::NotMatching &&
         SourceAllowPath(source, url, has_followed_redirect);
}

std::string CSPSource::ToString() const {
  // scheme
  if (IsSchemeOnly())
    return scheme + ":";

  std::stringstream text;
  if (!scheme.empty())
    text << scheme << "://";

  // host
  if (is_host_wildcard) {
    if (host.empty())
      text << "*";
    else
      text << "*." << host;
  } else {
    text << host;
  }

  // port
  if (is_port_wildcard)
    text << ":*";
  if (port != url::PORT_UNSPECIFIED)
    text << ":" << port;

  // path
  text << path;

  return text.str();
}

bool CSPSource::IsSchemeOnly() const {
  return !HasHost();
}

bool CSPSource::HasPort() const {
  return port != url::PORT_UNSPECIFIED || is_port_wildcard;
}

bool CSPSource::HasHost() const {
  return !host.empty() || is_host_wildcard;
}

bool CSPSource::HasPath() const {
  return !path.empty();
}

}  // namespace content
