// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/public/dns_over_https_config.h"

#include "base/values.h"
#include "net/dns/public/dns_over_https_server_config.h"
#include "testing/gmock/include/gmock/gmock.h"
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

  base::Value::List expected_servers;
  expected_servers.Append(kServerConfig1.ToValue());
  base::Value::Dict expected_value;
  expected_value.Set("servers", std::move(expected_servers));
  EXPECT_EQ(expected_value, config.ToValue());

  EXPECT_EQ(config, config);
}

TEST(DnsOverHttpsConfigTest, MultiValue) {
  std::vector<DnsOverHttpsServerConfig> servers{kServerConfig1, kServerConfig2};
  DnsOverHttpsConfig config(servers);
  EXPECT_EQ(servers, config.servers());

  EXPECT_EQ(kServerConfig1.server_template() + "\n" +
                kServerConfig2.server_template(),
            config.ToString());

  base::Value::List expected_servers;
  expected_servers.Append(kServerConfig1.ToValue());
  expected_servers.Append(kServerConfig2.ToValue());
  base::Value::Dict expected_value;
  expected_value.Set("servers", std::move(expected_servers));
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
  EXPECT_EQ(DnsOverHttpsConfig(), DnsOverHttpsConfig::FromStringLax("foo"));

  EXPECT_FALSE(DnsOverHttpsConfig::FromString("foo bar"));
  EXPECT_EQ(DnsOverHttpsConfig(), DnsOverHttpsConfig::FromStringLax("foo bar"));
}

TEST(DnsOverHttpsConfigTest, FromStringSomeInvalid) {
  std::string some_invalid = "foo " + kServerConfig1.server_template() +
                             " bar " + kServerConfig2.server_template() +
                             " baz";
  EXPECT_FALSE(DnsOverHttpsConfig::FromString(some_invalid));
  EXPECT_EQ(DnsOverHttpsConfig({kServerConfig1, kServerConfig2}),
            DnsOverHttpsConfig::FromStringLax(some_invalid));
}

TEST(DnsOverHttpsConfigTest, Json) {
  auto parsed = DnsOverHttpsConfig::FromString(R"(
    {
      "servers": [{
        "template": "https://dnsserver.example.net/dns-query{?dns}",
        "endpoints": [{
          "ips": ["192.0.2.1", "2001:db8::1"]
        }, {
          "ips": ["192.0.2.2", "2001:db8::2"]
        }]
      }]
    }
  )");

  ASSERT_TRUE(parsed);
  EXPECT_EQ(1u, parsed->servers().size());

  auto parsed2 = DnsOverHttpsConfig::FromString(parsed->ToString());
  EXPECT_EQ(parsed, parsed2);
}

TEST(DnsOverHttpsConfigTest, JsonWithUnknownKey) {
  auto parsed = DnsOverHttpsConfig::FromString(R"(
    {
      "servers": [{
        "template": "https://dnsserver.example.net/dns-query{?dns}"
      }],
      "unknown key": "value is ignored"
    }
  )");

  ASSERT_TRUE(parsed);
  EXPECT_EQ(1u, parsed->servers().size());

  auto parsed2 = DnsOverHttpsConfig::FromString(parsed->ToString());
  EXPECT_EQ(parsed, parsed2);
}

TEST(DnsOverHttpsConfigTest, BadJson) {
  // Not JSON
  EXPECT_FALSE(DnsOverHttpsConfig::FromString("{"));

  // No servers
  EXPECT_FALSE(DnsOverHttpsConfig::FromString("{}"));

  // Not a Dict
  EXPECT_FALSE(DnsOverHttpsConfig::FromString("[]"));

  // Wrong type for "servers"
  EXPECT_FALSE(DnsOverHttpsConfig::FromString("{\"servers\": 12345}"));

  // One bad server
  EXPECT_FALSE(DnsOverHttpsConfig::FromString(R"(
    {
      "servers": [{
        "template": "https://dnsserver.example.net/dns-query{?dns}",
      }, {
        "template": "not a valid template"
      }]
    }
  )"));
}

TEST(DnsOverHttpsConfigTest, JsonLax) {
  // Valid JSON is allowed
  auto parsed = *DnsOverHttpsConfig::FromString(R"(
    {
      "servers": [{
        "template": "https://dnsserver.example.net/dns-query{?dns}",
        "endpoints": [{
          "ips": ["192.0.2.1", "2001:db8::1"]
        }, {
          "ips": ["192.0.2.2", "2001:db8::2"]
        }]
      }]
    }
  )");
  DnsOverHttpsConfig reparsed =
      DnsOverHttpsConfig::FromStringLax(parsed.ToString());
  EXPECT_EQ(parsed, reparsed);

  // Lax parsing does not accept bad servers in JSON.
  DnsOverHttpsConfig from_bad = DnsOverHttpsConfig::FromStringLax(R"(
    {
      "servers": [{
        "template": "https://dnsserver.example.net/dns-query{?dns}",
      }, {
        "template": "not a valid template"
      }]
    }
  )");
  EXPECT_THAT(from_bad.servers(), testing::IsEmpty());
}

}  // namespace
}  // namespace net
