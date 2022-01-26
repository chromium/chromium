// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/public/dns_over_https_server_config.h"

#include <string>

#include "net/dns/public/dns_over_https_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

TEST(DnsOverHttpsServerConfigTest, ValidWithGet) {
  auto parsed = DnsOverHttpsServerConfig::FromString(
      "https://dnsserver.example.net/dns-query{?dns}");
  EXPECT_THAT(parsed, testing::Optional(testing::Property(
                          &DnsOverHttpsServerConfig::use_post, false)));

  parsed = DnsOverHttpsServerConfig::FromString(
      "https://dnsserver.example.net/dns-query{?dns,extra}");
  EXPECT_THAT(parsed, testing::Optional(testing::Property(
                          &DnsOverHttpsServerConfig::use_post, false)));

  parsed = DnsOverHttpsServerConfig::FromString(
      "https://query:{dns}@dnsserver.example.net");
  EXPECT_THAT(parsed, testing::Optional(testing::Property(
                          &DnsOverHttpsServerConfig::use_post, false)));

  parsed = DnsOverHttpsServerConfig::FromString(
      "https://dnsserver.example.net{/dns}");
  EXPECT_THAT(parsed, testing::Optional(testing::Property(
                          &DnsOverHttpsServerConfig::use_post, false)));
}

TEST(DnsOverHttpsServerConfigTest, ValidWithPost) {
  auto parsed = DnsOverHttpsServerConfig::FromString(
      "https://dnsserver.example.net/dns-query{?query}");
  EXPECT_THAT(parsed, testing::Optional(testing::Property(
                          &DnsOverHttpsServerConfig::use_post, true)));

  parsed = DnsOverHttpsServerConfig::FromString(
      "https://dnsserver.example.net/dns-query");
  EXPECT_THAT(parsed, testing::Optional(testing::Property(
                          &DnsOverHttpsServerConfig::use_post, true)));
}

TEST(DnsOverHttpsServerConfigTest, Invalid) {
  // Invalid template format
  EXPECT_FALSE(DnsOverHttpsServerConfig::FromString(
      "https://dnsserver.example.net/dns-query{{?dns}}"));
  // Must be HTTPS
  EXPECT_FALSE(DnsOverHttpsServerConfig::FromString(
      "http://dnsserver.example.net/dns-query"));
  EXPECT_FALSE(DnsOverHttpsServerConfig::FromString(
      "http://dnsserver.example.net/dns-query{?dns}"));
  // Template must expand to a valid URL
  EXPECT_FALSE(DnsOverHttpsServerConfig::FromString("https://{?dns}"));
  // The hostname must not contain the dns variable
  EXPECT_FALSE(
      DnsOverHttpsServerConfig::FromString("https://{dns}.dnsserver.net"));
}

TEST(DnsOverHttpsServerConfigTest, Empty) {
  EXPECT_FALSE(net::DnsOverHttpsServerConfig::FromString(""));
}

}  // namespace
}  // namespace net
