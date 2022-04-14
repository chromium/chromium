// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/public/dns_over_https_config.h"

#include "base/values.h"
#include "net/dns/public/dns_over_https_server_config.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

const DnsOverHttpsServerConfig kServerConfig1 =
    *DnsOverHttpsServerConfig::FromString("https://example1.test");
const DnsOverHttpsServerConfig kServerConfig2 =
    *DnsOverHttpsServerConfig::FromString("https://example2.test");

TEST(DnsOverHttpsConfigTest, SingleValue) {
  DnsOverHttpsConfig config({kServerConfig1});
  EXPECT_THAT(config.servers(), testing::ElementsAre(kServerConfig1));

  base::Value expected_value(base::Value::Type::LIST);
  expected_value.Append(kServerConfig1.ToValue());
  EXPECT_EQ(expected_value, config.ToValue());

  EXPECT_EQ(config, config);
}

TEST(DnsOverHttpsConfigTest, MultiValue) {
  std::vector<DnsOverHttpsServerConfig> servers{kServerConfig1, kServerConfig2};
  DnsOverHttpsConfig config(servers);
  EXPECT_EQ(servers, config.servers());

  EXPECT_THAT(config.ToStrings(),
              testing::ElementsAre(kServerConfig1.server_template(),
                                   kServerConfig2.server_template()));
  EXPECT_EQ(kServerConfig1.server_template() + "\n" +
                kServerConfig2.server_template(),
            config.ToString());

  base::Value expected_value(base::Value::Type::LIST);
  expected_value.Append(kServerConfig1.ToValue());
  expected_value.Append(kServerConfig2.ToValue());
  EXPECT_EQ(expected_value, config.ToValue());

  EXPECT_EQ(config, config);
}

TEST(DnsOverHttpsConfigTest, Equal) {
  DnsOverHttpsConfig a({kServerConfig1});
  DnsOverHttpsConfig a2({kServerConfig1});
  DnsOverHttpsConfig b({kServerConfig1, kServerConfig2});
  DnsOverHttpsConfig b2({kServerConfig1, kServerConfig2});

  EXPECT_EQ(a, a2);
  EXPECT_EQ(b, b2);
}

TEST(DnsOverHttpsConfigTest, NotEqual) {
  DnsOverHttpsConfig a({kServerConfig1});
  DnsOverHttpsConfig b({kServerConfig2});
  DnsOverHttpsConfig c({kServerConfig1, kServerConfig2});
  DnsOverHttpsConfig d({kServerConfig2, kServerConfig1});

  EXPECT_FALSE(a == b);
  EXPECT_FALSE(a == c);
  EXPECT_FALSE(a == d);
  EXPECT_FALSE(c == d);
}

TEST(DnsOverHttpsConfigTest, FromStringSingleValue) {
  auto config =
      DnsOverHttpsConfig::FromString(kServerConfig1.server_template());
  EXPECT_THAT(config, testing::Optional(DnsOverHttpsConfig({kServerConfig1})));
}

TEST(DnsOverHttpsConfigTest, FromStringMultiValue) {
  auto config =
      DnsOverHttpsConfig::FromString(kServerConfig1.server_template() + "\n" +
                                     kServerConfig2.server_template());
  EXPECT_THAT(
      config,
      testing::Optional(DnsOverHttpsConfig({kServerConfig1, kServerConfig2})));
}

TEST(DnsOverHttpsConfigTest, FromStrings) {
  EXPECT_THAT(DnsOverHttpsConfig::FromStrings({}),
              testing::Optional(DnsOverHttpsConfig()));
  EXPECT_THAT(
      DnsOverHttpsConfig::FromStrings({kServerConfig1.server_template()}),
      testing::Optional(DnsOverHttpsConfig({kServerConfig1})));
  EXPECT_THAT(
      DnsOverHttpsConfig::FromStrings(
          {kServerConfig1.server_template(), kServerConfig2.server_template()}),
      testing::Optional(DnsOverHttpsConfig({kServerConfig1, kServerConfig2})));
}

TEST(DnsOverHttpsConfigTest, FromStringExtraWhitespace) {
  auto config = DnsOverHttpsConfig::FromString(
      "  \t" + kServerConfig1.server_template() + "    " +
      kServerConfig2.server_template() + "\n ");
  EXPECT_THAT(
      config,
      testing::Optional(DnsOverHttpsConfig({kServerConfig1, kServerConfig2})));

  auto config2 =
      DnsOverHttpsConfig::FromString(kServerConfig1.server_template() + "\t" +
                                     kServerConfig2.server_template());
  EXPECT_EQ(config2, config);
}

TEST(DnsOverHttpsConfigTest, FromStringEmpty) {
  EXPECT_FALSE(DnsOverHttpsConfig::FromString(""));
  EXPECT_EQ(DnsOverHttpsConfig(), DnsOverHttpsConfig::FromStringLax(""));
}

TEST(DnsOverHttpsConfigTest, FromStringAllInvalid) {
  EXPECT_FALSE(DnsOverHttpsConfig::FromString("foo"));
  EXPECT_FALSE(DnsOverHttpsConfig::FromStrings({"foo"}));
  EXPECT_EQ(DnsOverHttpsConfig(), DnsOverHttpsConfig::FromStringLax("foo"));

  EXPECT_FALSE(DnsOverHttpsConfig::FromString("foo bar"));
  EXPECT_FALSE(DnsOverHttpsConfig::FromStrings({"foo", "bar"}));
  EXPECT_EQ(DnsOverHttpsConfig(), DnsOverHttpsConfig::FromStringLax("foo bar"));
}

TEST(DnsOverHttpsConfigTest, FromStringSomeInvalid) {
  EXPECT_FALSE(DnsOverHttpsConfig::FromStrings(
      {"foo", kServerConfig1.server_template(), "bar"}));

  std::string some_invalid = "foo " + kServerConfig1.server_template() +
                             " bar " + kServerConfig2.server_template() +
                             " baz";
  EXPECT_FALSE(DnsOverHttpsConfig::FromString(some_invalid));
  EXPECT_EQ(DnsOverHttpsConfig({kServerConfig1, kServerConfig2}),
            DnsOverHttpsConfig::FromStringLax(some_invalid));
}

}  // namespace
}  // namespace net
