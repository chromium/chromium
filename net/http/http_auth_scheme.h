// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_SCHEME_H_
#define NET_HTTP_HTTP_AUTH_SCHEME_H_

#include "net/base/net_export.h"

namespace net {
NET_EXPORT extern const char kBasicAuthScheme[];
NET_EXPORT extern const char kDigestAuthScheme[];
NET_EXPORT extern const char kNtlmAuthScheme[];
NET_EXPORT extern const char kNegotiateAuthScheme[];
NET_EXPORT extern const char kSpdyProxyAuthScheme[];
NET_EXPORT extern const char kMockAuthScheme[];
}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_SCHEME_H_
