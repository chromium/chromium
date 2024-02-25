// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_chain_util_apple.h"

#include <CFNetwork/CFProxySupport.h>
#include <CoreFoundation/CoreFoundation.h>

#include <string>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "net/base/host_port_pair.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"

namespace net {

namespace {

// Utility function to map a CFProxyType to a ProxyServer::Scheme.
// If the type is unknown, returns ProxyServer::SCHEME_INVALID.
ProxyServer::Scheme GetProxyServerScheme(CFStringRef proxy_type) {
  if (CFEqual(proxy_type, kCFProxyTypeHTTP)) {
    return ProxyServer::SCHEME_HTTP;
  }
  if (CFEqual(proxy_type, kCFProxyTypeHTTPS)) {
    // The "HTTPS" on the Mac side here means "proxy applies to https://" URLs;
    // the proxy itself is still expected to be an HTTP proxy.
    return ProxyServer::SCHEME_HTTP;
  }
  if (CFEqual(proxy_type, kCFProxyTypeSOCKS)) {
    // We can't tell whether this was v4 or v5. We will assume it is
    // v5 since that is the only version macOS X supports.
    return ProxyServer::SCHEME_SOCKS5;
  }
  return ProxyServer::SCHEME_INVALID;
}

}  // namespace

ProxyChain ProxyDictionaryToProxyChain(CFStringRef proxy_type,
                                       CFDictionaryRef dict,
                                       CFStringRef host_key,
                                       CFStringRef port_key) {
  ProxyServer::Scheme scheme = GetProxyServerScheme(proxy_type);
  if (CFEqual(proxy_type, kCFProxyTypeNone)) {
    return ProxyChain::Direct();
  }

  if (scheme == ProxyServer::SCHEME_INVALID) {
    // No hostname port to extract; we are done.
    return ProxyChain(scheme, HostPortPair());
  }

  CFStringRef host_ref =
      base::apple::GetValueFromDictionary<CFStringRef>(dict, host_key);
  if (!host_ref) {
    LOG(WARNING) << "Could not find expected key "
                 << base::SysCFStringRefToUTF8(host_key)
                 << " in the proxy dictionary";
    return ProxyChain();  // Invalid.
  }
  std::string host = base::SysCFStringRefToUTF8(host_ref);

  CFNumberRef port_ref =
      base::apple::GetValueFromDictionary<CFNumberRef>(dict, port_key);
  int port;
  if (port_ref) {
    CFNumberGetValue(port_ref, kCFNumberIntType, &port);
  } else {
    port = ProxyServer::GetDefaultPortForScheme(scheme);
  }

  return ProxyChain::FromSchemeHostAndPort(scheme, host, port);
}

}  // namespace net
