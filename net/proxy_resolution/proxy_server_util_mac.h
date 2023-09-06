// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_PROXY_SERVER_UTIL_MAC_H_
#define NET_PROXY_RESOLUTION_PROXY_SERVER_UTIL_MAC_H_

#include <CoreFoundation/CoreFoundation.h>

#include "net/base/proxy_server.h"

namespace net {

// Utility function to pull out a host/port pair from a dictionary and return
// it as a ProxyServer object. Pass in a dictionary that has a  value for the
// host key and optionally a value for the port key. In the error condition
// where the host value is especially malformed, returns an invalid
// ProxyServer.
NET_EXPORT ProxyServer ProxyDictionaryToProxyServer(ProxyServer::Scheme scheme,
                                                    CFDictionaryRef dict,
                                                    CFStringRef host_key,
                                                    CFStringRef port_key);

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_PROXY_SERVER_UTIL_MAC_H_
