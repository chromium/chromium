// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_server_util_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#include <string>

#include "base/apple/foundation_util.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "net/base/host_port_pair.h"
#include "net/base/proxy_server.h"

namespace net {

ProxyServer ProxyDictionaryToProxyServer(ProxyServer::Scheme scheme,
                                         CFDictionaryRef dict,
                                         CFStringRef host_key,
                                         CFStringRef port_key) {
  if (scheme == ProxyServer::SCHEME_INVALID ||
      scheme == ProxyServer::SCHEME_DIRECT) {
    // No hostname port to extract; we are done.
    return ProxyServer(scheme, HostPortPair());
  }

  CFStringRef host_ref =
      base::apple::GetValueFromDictionary<CFStringRef>(dict, host_key);
  if (!host_ref) {
    LOG(WARNING) << "Could not find expected key "
                 << base::SysCFStringRefToUTF8(host_key)
                 << " in the proxy dictionary";
    return ProxyServer();  // Invalid.
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

  return ProxyServer::FromSchemeHostAndPort(scheme, host, port);
}

}  // namespace net
