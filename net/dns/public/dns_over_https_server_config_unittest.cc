// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/public/dns_over_https_server_config.h"

#include <string>
#include <string_view>

#include "base/json/json_reader.h"
#include "net/base/ip_address.h"
#include "net/dns/public/dns_over_https_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

const IPAddress ip1(192, 0, 2, 1);
const IPAddress ip2(0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1);
const IPAddress ip3(192, 0, 2, 2);
const IPAddress ip4(0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2);
const DnsOverHttpsServerConfig::Endpoints endpoints{{ip1, ip2}, {ip3, ip4}};

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

TEST(DnsOverHttpsServerConfigTest, Simple) {
  auto parsed = DnsOverHttpsServerConfig::FromString(
      "https://dnsserver.example.net/dns-query{?dns}");
  EXPECT_THAT(parsed, testing::Optional(testing::Property(
                          &DnsOverHttpsServerConfig::IsSimple, true)));
}

TEST(DnsOverHttpsServerConfigTest, ToValueSimple) {
  auto parsed = DnsOverHttpsServerConfig::FromString(
      "https://dnsserver.example.net/dns-query{?dns}");
  ASSERT_TRUE(parsed);

  base::Value expected = *base::JSONReader::Read(R"(
    {
      "template": "https://dnsserver.example.net/dns-query{?dns}"
    }
  )");
  EXPECT_EQ(expected.GetDict(), parsed->ToValue());
}

TEST(DnsOverHttpsServerConfigTest, ToValueWithEndpoints) {
  auto parsed = DnsOverHttpsServerConfig::FromString(
      "https://dnsserver.example.net/dns-query{?dns}", endpoints);
  ASSERT_TRUE(parsed);

  EXPECT_THAT(parsed, testing::Optional(testing::Property(
                          &DnsOverHttpsServerConfig::IsSimple, false)));
  EXPECT_THAT(parsed, testing::Optional(testing::Property(
                          &DnsOverHttpsServerConfig::endpoints, endpoints)));

  base::Value expected = *base::JSONReader::Read(
      R"({
        "template": "https://dnsserver.example.net/dns-query{?dns}",
        "endpoints": [{
          "ips": ["192.0.2.1", "2001:db8::1"]
        }, {
          "ips": ["192.0.2.2", "2001:db8::2"]
        }]
      })");
  EXPECT_EQ(expected.GetDict(), parsed->ToValue());
}

TEST(DnsOverHttpsServerConfigTest, FromValueSimple) {
  base::Value input = *base::JSONReader::Read(R"(
    {
      "template": "https://dnsserver.example.net/dns-query{?dns}"
    }
  )");

  auto parsed =
      DnsOverHttpsServerConfig::FromValue(std::move(input).TakeDict());

  auto expected = DnsOverHttpsServerConfig::FromString(
      "https://dnsserver.example.net/dns-query{?dns}");
  EXPECT_EQ(expected, parsed);
}

TEST(DnsOverHttpsServerConfigTest, FromValueWithEndpoints) {
  base::Value input = *base::JSONReader::Read(R"(
    {
      "template": "https://dnsserver.example.net/dns-query{?dns}",
      "endpoints": [{
        "ips": ["192.0.2.1", "2001:db8::1"]
      }, {
        "ips": ["192.0.2.2", "2001:db8::2"]
      }]
    }
  )");

  auto parsed =
      DnsOverHttpsServerConfig::FromValue(std::move(input).TakeDict());

  auto expected = DnsOverHttpsServerConfig::FromString(
      "https://dnsserver.example.net/dns-query{?dns}", endpoints);
  EXPECT_EQ(expected, parsed);
}

TEST(DnsOverHttpsServerConfigTest, FromValueWithUnknownKey) {
  base::Value input = *base::JSONReader::Read(R"(
    {
      "template": "https://dnsserver.example.net/dns-query{?dns}",
      "unknown key": "value is ignored"
    }
  )");

  auto parsed =
      DnsOverHttpsServerConfig::FromValue(std::move(input).TakeDict());

  auto expected = DnsOverHttpsServerConfig::FromString(
      "https://dnsserver.example.net/dns-query{?dns}");
  EXPECT_EQ(expected, parsed);
}

TEST(DnsOverHttpsServerConfigTest, FromValueInvalid) {
  // Empty dict
  EXPECT_FALSE(DnsOverHttpsServerConfig::FromValue(base::Value::Dict()));

  // Wrong scheme
  std::string_view input = R"(
    {
      "template": "http://dnsserver.example.net/dns-query{?dns}"
    }
  )";
  EXPECT_FALSE(DnsOverHttpsServerConfig::FromValue(
      std::move(base::JSONReader::Read(input)->GetDict())));

  // Wrong template type
  input = R"({"template": 12345})";
  EXPECT_FALSE(DnsOverHttpsServerConfig::FromValue(
      std::move(base::JSONReader::Read(input)->GetDict())));

  // Wrong endpoints type
  input = R"(
    {
      "template": "https://dnsserver.example.net/dns-query{?dns}",
      "endpoints": {
         "ips": ["192.0.2.1", "2001:db8::1"]
      }
    }
  )";
  EXPECT_FALSE(DnsOverHttpsServerConfig::FromValue(
      std::move(base::JSONReader::Read(input)->GetDict())));

  // Wrong "ips" type
  input = R"(
    {
      "template": "https://dnsserver.example.net/dns-query{?dns}",
      "endpoints": [{
        "ips": "192.0.2.1"
      }]
    }
  )";
  EXPECT_FALSE(DnsOverHttpsServerConfig::FromValue(
      std::move(base::JSONReader::Read(input)->GetDict())));

  // Wrong IP type
  input = R"(
    {
      "template": "https://dnsserver.example.net/dns-query{?dns}",
      "endpoints": [{
        "ips": ["2001:db8::1", 192.021]
      }]
    }
  )";
  EXPECT_FALSE(DnsOverHttpsServerConfig::FromValue(
      std::move(base::JSONReader::Read(input)->GetDict())));

  // Bad IP address
  input = R"(
    {
      "template": "https://dnsserver.example.net/dns-query{?dns}",
     "endpoints": [{
        "ips": ["2001:db8::1", "256.257.258.259"]
      }]
    }
  )";
  EXPECT_FALSE(DnsOverHttpsServerConfig::FromValue(
      std::move(base::JSONReader::Read(input)->GetDict())));
}

}  // namespace
}  // namespace net
