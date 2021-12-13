// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/public/dns_over_https_server_config.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

TEST(DnsOverHttpsServerConfigTest, ValidWithGet) {
  const std::string input = "https://example/{?dns}";
  const auto parsed = net::DnsOverHttpsServerConfig::FromString(input);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(input, parsed->server_template());
  EXPECT_FALSE(parsed->use_post());
}

TEST(DnsOverHttpsServerConfigTest, ValidWithPost) {
  const std::string input = "https://example/";
  const auto parsed = net::DnsOverHttpsServerConfig::FromString(input);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(input, parsed->server_template());
  EXPECT_TRUE(parsed->use_post());
}

TEST(DnsOverHttpsServerConfigTest, Invalid) {
  const std::string input = "http://example/{?dns}";
  const auto parsed = net::DnsOverHttpsServerConfig::FromString(input);
  EXPECT_FALSE(parsed.has_value());
}

TEST(DnsOverHttpsServerConfigTest, Empty) {
  const auto parsed = net::DnsOverHttpsServerConfig::FromString("");
  EXPECT_FALSE(parsed.has_value());
}

}  // namespace
}  // namespace net
