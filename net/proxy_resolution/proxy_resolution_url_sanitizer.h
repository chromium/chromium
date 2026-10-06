// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_PROXY_RESOLUTION_URL_SANITIZER_H_
#define NET_PROXY_RESOLUTION_PROXY_RESOLUTION_URL_SANITIZER_H_

#include "net/base/net_export.h"

class GURL;

namespace net {

// Returns a sanitized copy of `url` which is safe to pass on to a proxy
// resolver, such as a PAC script or the system's proxy resolver.
//
// PAC scripts are modelled as being controllable by a network-present
// attacker (since such an attacker can influence the outcome of proxy
// auto-discovery, or modify the contents of insecurely delivered PAC scripts).
//
// As such, it is important that the full path/query of https:// URLs not be
// sent to PAC scripts, since that would give an attacker access to data that
// is ordinarily protected by TLS.
//
// Obscuring the path for http:// URLs isn't being done since it doesn't matter
// for security (attacker can already route traffic through their HTTP proxy
// and see the full URL for http:// requests).
//
// Embedded credentials and the reference fragment are always removed.
//
// TODO(crbug.com/41412888): Use the same stripping for insecure URL
// schemes.
NET_EXPORT GURL SanitizeUrlForProxyResolution(const GURL& url);

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_PROXY_RESOLUTION_URL_SANITIZER_H_
