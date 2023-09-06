// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_server_util_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>

#include "net/base/proxy_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

// Test convert ProxyDictionary To ProxyServer with invalid inputs.
// https://crbug.com/1478580
TEST(ProxyServerUtilMacTest, InvalidProxyDictionaryToProxyServer) {
  CFStringRef host_key = CFStringCreateWithCString(
      kCFAllocatorDefault, "HttpHost", kCFStringEncodingUTF8);
  CFStringRef port_key = CFStringCreateWithCString(
      kCFAllocatorDefault, "HttpPort", kCFStringEncodingUTF8);
  CFStringRef keys[] = {host_key};
  CFStringRef values[] = {CFStringCreateWithCString(
      kCFAllocatorDefault, "127.1110.0.1", kCFStringEncodingUTF8)};
  CFDictionaryRef invalid_ip_dict = CFDictionaryCreate(
      kCFAllocatorDefault, (const void**)keys, (const void**)values, 1,
      &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  ProxyServer proxy_server = ProxyDictionaryToProxyServer(
      ProxyServer::SCHEME_HTTP, invalid_ip_dict, host_key, port_key);
  EXPECT_FALSE(proxy_server.is_valid());
}

}  // namespace net
