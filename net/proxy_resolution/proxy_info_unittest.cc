// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_info.h"

#include "net/base/net_errors.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
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
  EXPECT_EQ("PROXY myproxy:80;DIRECT", info.proxy_list().ToDebugString());
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
  EXPECT_EQ("PROXY foo.com:80", info.proxy_list().ToDebugString());
  proxy_list.Set("http://bar.com");
  info.UseProxyList(proxy_list);
  EXPECT_EQ("PROXY bar.com:80", info.proxy_list().ToDebugString());
}

TEST(ProxyInfoTest, IsForIpProtection) {
  ProxyInfo info;

  ProxyChain regular_proxy_chain =
      ProxyChain::FromSchemeHostAndPort(ProxyServer::SCHEME_HTTP, "foo", 80);
  info.UseProxyChain(regular_proxy_chain);
  EXPECT_FALSE(info.is_for_ip_protection());

  ProxyChain ip_protection_proxy_chain = ProxyChain::ForIpProtection({
      ProxyServer::FromSchemeHostAndPort(ProxyServer::SCHEME_HTTPS, "proxy1",
                                         std::nullopt),
      ProxyServer::FromSchemeHostAndPort(ProxyServer::SCHEME_HTTPS, "proxy2",
                                         std::nullopt),
  });
  info.UseProxyChain(ip_protection_proxy_chain);
  EXPECT_TRUE(info.is_for_ip_protection());
  info.UseProxyChain(regular_proxy_chain);
  EXPECT_FALSE(info.is_for_ip_protection());
}

TEST(ProxyInfoTest, UseProxyChain) {
  ProxyInfo info;
  ProxyChain proxy_chain =
      ProxyChain::FromSchemeHostAndPort(ProxyServer::SCHEME_HTTP, "foo", 80);
  info.UseProxyChain(proxy_chain);
  EXPECT_EQ("PROXY foo:80", info.proxy_list().ToDebugString());
}

}  // namespace net
