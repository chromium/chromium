// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/proxy/model/proxy_auth_challenge_util.h"

#import <Foundation/Foundation.h>

#import <cstdint>
#import <optional>
#import <string>
#import <string_view>

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "base/numerics/safe_conversions.h"
#import "base/strings/strcat.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "net/base/apple/http_response_headers_util.h"
#import "net/base/net_string_util.h"
#import "net/http/http_auth_challenge_tokenizer.h"
#import "net/http/http_auth_scheme.h"
#import "net/http/http_response_headers.h"
#import "net/http/http_util.h"
#import "url/gurl.h"
#import "url/scheme_host_port.h"
#import "url/url_canon.h"
#import "url/url_constants.h"

namespace {

// Header carrying the proxy authentication challenge in a 407 response.
constexpr std::string_view kProxyAuthenticateHeader = "Proxy-Authenticate";

// Challenge parameter carrying the authentication realm.
constexpr std::string_view kRealmParameter = "realm";

// Path reported for proxy challenges. Proxies authenticate the connection
// rather than a specific resource, so the path is always the root.
constexpr std::string_view kProxyAuthPath = "/";

// Returns the standard URL scheme matching the type of proxy described by
// `protection_space`, or an empty view for proxy types that have no standard
// URL scheme (SOCKS, FTP).
std::string_view ProxySchemeForProtectionSpace(
    NSURLProtectionSpace* protection_space) {
  NSString* proxy_type = protection_space.proxyType;
  if ([proxy_type isEqualToString:NSURLProtectionSpaceHTTPSProxy]) {
    return url::kHttpsScheme;
  }
  if ([proxy_type isEqualToString:NSURLProtectionSpaceHTTPProxy]) {
    return url::kHttpScheme;
  }
  return std::string_view();
}

// Normalizes the port reported by `protection_space`, whose `scheme` must be a
// standard scheme. Returns `std::nullopt` for out-of-range values.
std::optional<uint16_t> ProxyPortForProtectionSpace(
    NSURLProtectionSpace* protection_space,
    std::string_view scheme) {
  // `NSURLProtectionSpace.port` is a signed `NSInteger` supplied by WebKit.
  // Range-check before narrowing: `base::checked_cast` CHECK-fails on
  // overflow, and on a negative value.
  NSInteger raw_port = protection_space.port;
  if (!base::IsValueInRangeForNumericType<uint16_t>(raw_port)) {
    return std::nullopt;
  }
  if (raw_port != 0) {
    return base::checked_cast<uint16_t>(raw_port);
  }

  // WebKit reported no port. Substitute the scheme default, the same way
  // `net::ProxyServer::FromSchemeHostAndPort` does for an absent port.
  int default_port = url::DefaultPortForScheme(scheme);
  CHECK_GT(default_port, 0);
  return base::checked_cast<uint16_t>(default_port);
}

// Maps the `NSURLAuthenticationMethod*` of `protection_space` to the matching
// `net::k*AuthScheme` constant. Returns an empty view for methods that are not
// HTTP authentication schemes (server trust, client certificate, ...).
std::string_view AuthSchemeForProtectionSpace(
    NSURLProtectionSpace* protection_space) {
  NSString* method = protection_space.authenticationMethod;
  if ([method isEqualToString:NSURLAuthenticationMethodHTTPBasic] ||
      [method isEqualToString:NSURLAuthenticationMethodDefault]) {
    return net::kBasicAuthScheme;
  }
  if ([method isEqualToString:NSURLAuthenticationMethodHTTPDigest]) {
    return net::kDigestAuthScheme;
  }
  if ([method isEqualToString:NSURLAuthenticationMethodNTLM]) {
    return net::kNtlmAuthScheme;
  }
  if ([method isEqualToString:NSURLAuthenticationMethodNegotiate]) {
    return net::kNegotiateAuthScheme;
  }
  return std::string_view();
}

// Returns the realm advertised by `challenge`, or an empty string when it
// advertises none, when its auth-scheme is not `expected_scheme`, or when its
// parameters are malformed.
std::string ExtractRealm(std::string_view challenge,
                         std::string_view expected_scheme) {
  net::HttpAuthChallengeTokenizer tokenizer(challenge);

  if (expected_scheme.empty() ||
      !base::EqualsCaseInsensitiveASCII(tokenizer.auth_scheme(),
                                        expected_scheme)) {
    return std::string();
  }

  std::string realm;
  net::HttpUtil::NameValuePairsIterator parameters = tokenizer.param_pairs();
  while (parameters.GetNext()) {
    if (!base::EqualsCaseInsensitiveASCII(parameters.name(), kRealmParameter)) {
      continue;
    }
    // The realm is Latin-1 on the wire. A value that cannot be converted is
    // treated as a parse failure, as it is on desktop.
    if (!net::ConvertToUtf8AndNormalize(parameters.value(), net::kCharsetLatin1,
                                        &realm)) {
      return std::string();
    }
    // Deliberately no early exit: a repeated `realm` overwrites, so the last
    // one wins.
  }

  return parameters.valid() ? realm : std::string();
}

// Returns the realm advertised for `expected_scheme` by the
// `Proxy-Authenticate` headers of `response_headers`, or an empty string when
// `response_headers` is null or advertises none.
//
// Each header value is one challenge, which is how `HttpAuth` reads them:
// `proxy-authenticate` is a non-coalescing header, so `//net` never joins two
// of them. `NSHTTPURLResponse` does join them, and a joined value parses as the
// single challenge its first auth-scheme introduces.
std::string ExtractRealmFromResponseHeaders(
    const scoped_refptr<net::HttpResponseHeaders>& response_headers,
    std::string_view expected_scheme) {
  if (!response_headers) {
    return std::string();
  }

  std::string header_value;
  size_t iter = 0;
  while (response_headers->EnumerateHeader(&iter, kProxyAuthenticateHeader,
                                           &header_value)) {
    std::string realm = ExtractRealm(header_value, expected_scheme);
    if (!realm.empty()) {
      return realm;
    }
  }
  return std::string();
}

}  // namespace

scoped_refptr<net::HttpResponseHeaders> CreateProxyAuthHeadersFromNSURLResponse(
    NSURLResponse* failure_response) {
  NSHTTPURLResponse* http_response =
      base::apple::ObjCCast<NSHTTPURLResponse>(failure_response);
  if (!http_response) {
    return nullptr;
  }
  return net::CreateHeadersFromNSHTTPURLResponse(http_response);
}

std::optional<net::AuthChallengeInfo>
CreateAuthChallengeInfoFromProtectionSpace(
    NSURLProtectionSpace* protection_space,
    const scoped_refptr<net::HttpResponseHeaders>& response_headers) {
  if (!protection_space || !protection_space.isProxy) {
    return std::nullopt;
  }

  // Proxy types with no standard URL scheme (SOCKS, FTP) are not supported.
  std::string_view scheme = ProxySchemeForProtectionSpace(protection_space);
  if (scheme.empty()) {
    return std::nullopt;
  }

  std::optional<uint16_t> port =
      ProxyPortForProtectionSpace(protection_space, scheme);
  if (!port.has_value()) {
    return std::nullopt;
  }

  // Foundation does not normalize `NSURLProtectionSpace.host`, while
  // `url::SchemeHostPort` rejects any host that is not already canonical.
  // Round-tripping through `GURL` applies the canonicalization.
  GURL canonical_url(
      base::StrCat({scheme, url::kStandardSchemeSeparator,
                    base::SysNSStringToUTF8(protection_space.host)}));

  url::SchemeHostPort challenger(scheme, canonical_url.host(), *port);
  if (!challenger.IsValid()) {
    return std::nullopt;
  }

  net::AuthChallengeInfo auth_info;
  auth_info.is_proxy = true;
  auth_info.challenger = std::move(challenger);
  std::string_view auth_scheme = AuthSchemeForProtectionSpace(protection_space);
  auth_info.scheme = std::string(auth_scheme);

  // The realm is read from the challenge rather than from the protection
  // space: Foundation reports a nil `realm` for every proxy protection space,
  // even when the 407 advertises one.
  std::string realm =
      ExtractRealmFromResponseHeaders(response_headers, auth_scheme);
  auth_info.realm = std::move(realm);
  auth_info.path = kProxyAuthPath;
  return auth_info;
}
