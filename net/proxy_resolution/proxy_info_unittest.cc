// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_info.h"

#include "net/base/net_errors.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

TEST(ProxyInfoTest, ProxyInfoIsDirectOnly) {
  // Test the is_direct_only() predicate.
  ProxyInfo info;

  // An empty ProxyInfo is not considered direct.
  EXPECT_FALSE(info.is_direct_only());

  info.UseDirect();
  EXPECT_TRUE(info.is_direct_only());

  info.UsePacString("DIRECT");
  EXPECT_TRUE(info.is_direct_only());

  info.UsePacString("PROXY myproxy:80");
  EXPECT_FALSE(info.is_direct_only());

  info.UsePacString("DIRECT; PROXY myproxy:80");
  EXPECT_TRUE(info.is_direct());
  EXPECT_FALSE(info.is_direct_only());

  info.UsePacString("PROXY myproxy:80; DIRECT");
  EXPECT_FALSE(info.is_direct());
  EXPECT_FALSE(info.is_direct_only());
  EXPECT_EQ(2u, info.proxy_list().size());
  EXPECT_EQ("PROXY myproxy:80;DIRECT", info.proxy_list().ToPacString());
  // After falling back to direct, we shouldn't consider it DIRECT only.
  EXPECT_TRUE(info.Fallback(ERR_PROXY_CONNECTION_FAILED, NetLogWithSource()));
  EXPECT_TRUE(info.is_direct());
  EXPECT_FALSE(info.is_direct_only());
}

}  // namespace

TEST(ProxyInfoTest, UseVsOverrideProxyList) {
  ProxyInfo info;
  ProxyList proxy_list;
  proxy_list.Set("http://foo.com");
  info.OverrideProxyList(proxy_list);
  EXPECT_EQ("PROXY foo.com:80", info.proxy_list().ToPacString());
  proxy_list.Set("http://bar.com");
  info.UseProxyList(proxy_list);
  EXPECT_EQ("PROXY bar.com:80", info.proxy_list().ToPacString());
}

}  // namespace net
