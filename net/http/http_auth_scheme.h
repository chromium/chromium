// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_SCHEME_H_
#define NET_HTTP_HTTP_AUTH_SCHEME_H_

namespace net {
inline constexpr char kBasicAuthScheme[] = "basic";
inline constexpr char kDigestAuthScheme[] = "digest";
inline constexpr char kNtlmAuthScheme[] = "ntlm";
inline constexpr char kNegotiateAuthScheme[] = "negotiate";
inline constexpr char kSpdyProxyAuthScheme[] = "spdyproxy";
inline constexpr char kMockAuthScheme[] = "mock";
}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_SCHEME_H_
