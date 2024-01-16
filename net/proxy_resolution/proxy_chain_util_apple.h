// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_PROXY_CHAIN_UTIL_APPLE_H_
#define NET_PROXY_RESOLUTION_PROXY_CHAIN_UTIL_APPLE_H_

#include <CoreFoundation/CoreFoundation.h>

#include "net/base/proxy_chain.h"

namespace net {

// Utility function to pull out a host/port pair from a dictionary and return
// it as a ProxyChain object. Pass in a dictionary that has a value for the
// host key, a proxy_type, and optionally a value for the port key. In the error
// condition where the host value is especially malformed, returns an invalid
// ProxyChain.
NET_EXPORT ProxyChain ProxyDictionaryToProxyChain(CFStringRef proxy_type,
                                                  CFDictionaryRef dict,
                                                  CFStringRef host_key,
                                                  CFStringRef port_key);

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_PROXY_CHAIN_UTIL_APPLE_H_
