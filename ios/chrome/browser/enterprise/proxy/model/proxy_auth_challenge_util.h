// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_PROXY_AUTH_CHALLENGE_UTIL_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_PROXY_AUTH_CHALLENGE_UTIL_H_

#import <optional>

#import "base/memory/scoped_refptr.h"
#import "net/base/auth.h"

@class NSURLProtectionSpace;
@class NSURLResponse;

namespace net {
class HttpResponseHeaders;
}  // namespace net

// Converts `failure_response` into `net::HttpResponseHeaders`. Returns nullptr
// if `failure_response` is nil or is not an `NSHTTPURLResponse`, which happens
// when WebKit issues the challenge before receiving any response.
scoped_refptr<net::HttpResponseHeaders> CreateProxyAuthHeadersFromNSURLResponse(
    NSURLResponse* failure_response);

// Converts a WebKit proxy `protection_space` into `net::AuthChallengeInfo`.
// `response_headers` may be null; when present it supplies
// `AuthChallengeInfo::realm`, which Foundation never exposes on a proxy
// protection space. `AuthChallengeInfo::challenge` is never populated.
//
// When `protection_space` reports no port, the scheme's default port is
// substituted (443 for HTTPS proxies, 80 for HTTP proxies), mirroring
// `net::ProxyServer::FromSchemeHostAndPort`.
//
// Returns `std::nullopt` when the challenge cannot be represented faithfully:
// `protection_space` is nil, is not a proxy challenge, reports a port outside
// the valid range, or yields an invalid `url::SchemeHostPort` challenger
// (empty or non-canonical host, or a proxy type such as SOCKS/FTP that has no
// standard URL scheme). Callers must fall back to the default authentication
// flow in that case.
std::optional<net::AuthChallengeInfo>
CreateAuthChallengeInfoFromProtectionSpace(
    NSURLProtectionSpace* protection_space,
    const scoped_refptr<net::HttpResponseHeaders>& response_headers);

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_PROXY_AUTH_CHALLENGE_UTIL_H_
