// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/winhttp/proxy_info.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace winhttp {

TEST(WinHttpProxyInfoTest, DefaultIsDirectConnection) {
  ProxyInfo proxy_info;
  EXPECT_FALSE(proxy_info.auto_detect);
  EXPECT_TRUE(proxy_info.auto_config_url.empty());
  EXPECT_TRUE(proxy_info.proxy.empty());
  EXPECT_TRUE(proxy_info.proxy_bypass.empty());
}

TEST(WinHttpProxyInfoTest, CanCopy) {
  ProxyInfo proxy_info(true, L"Foo", L"Bar", L"Baz");
  ProxyInfo proxy_info2(proxy_info);

  EXPECT_TRUE(proxy_info.auto_detect);
  EXPECT_EQ(proxy_info.auto_detect, proxy_info2.auto_detect);
  EXPECT_STREQ(L"Foo", proxy_info.auto_config_url.c_str());
  EXPECT_STREQ(L"Foo", proxy_info2.auto_config_url.c_str());
  EXPECT_STREQ(L"Bar", proxy_info.proxy.c_str());
  EXPECT_STREQ(L"Bar", proxy_info2.proxy.c_str());
  EXPECT_STREQ(L"Baz", proxy_info.proxy_bypass.c_str());
  EXPECT_STREQ(L"Baz", proxy_info2.proxy_bypass.c_str());
}

TEST(WinHttpProxyInfoTest, CanMove) {
  ProxyInfo proxy_info{true, L"Foo", L"Bar", L"Baz"};
  ProxyInfo proxy_info2 = std::move(proxy_info);

  EXPECT_TRUE(proxy_info2.auto_detect);
  EXPECT_STREQ(L"Foo", proxy_info2.auto_config_url.c_str());
  EXPECT_STREQ(L"Bar", proxy_info2.proxy.c_str());
  EXPECT_STREQ(L"Baz", proxy_info2.proxy_bypass.c_str());
  EXPECT_TRUE(proxy_info.auto_config_url.empty());
  EXPECT_TRUE(proxy_info.proxy.empty());
  EXPECT_TRUE(proxy_info.proxy_bypass.empty());
}

}  // namespace winhttp
