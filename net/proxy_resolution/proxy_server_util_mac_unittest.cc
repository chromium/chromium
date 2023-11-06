// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_server_util_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>

#include "base/apple/scoped_cftyperef.h"
#include "net/base/proxy_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

// Test convert ProxyDictionary To ProxyServer with invalid inputs.
// https://crbug.com/1478580
TEST(ProxyServerUtilMacTest, InvalidProxyDictionaryToProxyServer) {
  CFStringRef host_key = CFSTR("HttpHost");
  CFStringRef port_key = CFSTR("HttpPort");
  CFStringRef value = CFSTR("127.1110.0.1");
  const void* keys[] = {host_key};
  const void* values[] = {value};
  base::apple::ScopedCFTypeRef<CFDictionaryRef> invalid_ip_dict(
      CFDictionaryCreate(kCFAllocatorDefault, keys, values, 1,
                         &kCFTypeDictionaryKeyCallBacks,
                         &kCFTypeDictionaryValueCallBacks));
  ProxyServer proxy_server = ProxyDictionaryToProxyServer(
      ProxyServer::SCHEME_HTTP, invalid_ip_dict.get(), host_key, port_key);
  EXPECT_FALSE(proxy_server.is_valid());
}

}  // namespace net
