// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/network/network_utils.h"

#include "net/base/ip_address.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

TEST(NetworkUtilsTest, IsReservedIPAddress) {
  // Unreserved IPv4 addresses (in various forms).
  EXPECT_FALSE(network_utils::IsReservedIPAddress("8.8.8.8"));
  EXPECT_FALSE(network_utils::IsReservedIPAddress("99.64.0.0"));
  EXPECT_FALSE(network_utils::IsReservedIPAddress("212.15.0.0"));
  EXPECT_FALSE(network_utils::IsReservedIPAddress("212.15"));
  EXPECT_FALSE(network_utils::IsReservedIPAddress("212.15.0"));
  EXPECT_FALSE(network_utils::IsReservedIPAddress("3557752832"));

  // Reserved IPv4 addresses (in various forms).
  EXPECT_TRUE(network_utils::IsReservedIPAddress("192.168.0.0"));
  EXPECT_TRUE(network_utils::IsReservedIPAddress("192.168.0.6"));
  EXPECT_TRUE(network_utils::IsReservedIPAddress("10.0.0.5"));
  EXPECT_TRUE(network_utils::IsReservedIPAddress("10.0.0"));
  EXPECT_TRUE(network_utils::IsReservedIPAddress("10.0"));
  EXPECT_TRUE(network_utils::IsReservedIPAddress("3232235526"));

  // Unreserved IPv6 addresses.
  EXPECT_FALSE(network_utils::IsReservedIPAddress(
      "[FFC0:ba98:7654:3210:FEDC:BA98:7654:3210]"));
  EXPECT_FALSE(network_utils::IsReservedIPAddress(
      "[2000:ba98:7654:2301:EFCD:BA98:7654:3210]"));
  // IPv4-mapped to IPv6
  EXPECT_FALSE(network_utils::IsReservedIPAddress("[::ffff:8.8.8.8]"));

  // Reserved IPv6 addresses.
  EXPECT_TRUE(network_utils::IsReservedIPAddress("[::1]"));
  EXPECT_TRUE(network_utils::IsReservedIPAddress("[::192.9.5.5]"));
  EXPECT_TRUE(network_utils::IsReservedIPAddress("[::ffff:192.168.1.1]"));
  EXPECT_TRUE(network_utils::IsReservedIPAddress("[FEED::BEEF]"));
  EXPECT_TRUE(network_utils::IsReservedIPAddress(
      "[FEC0:ba98:7654:3210:FEDC:BA98:7654:3210]"));

  // Not IP addresses at all.
  EXPECT_FALSE(network_utils::IsReservedIPAddress("example.com"));
  EXPECT_FALSE(network_utils::IsReservedIPAddress("127.0.0.1.example.com"));

  // Moar IPv4
  for (int i = 0; i < 256; i++) {
    net::IPAddress address(i, 0, 0, 1);
    std::string address_string = address.ToString();
    if (i == 0 || i == 10 || i == 127 || i == 192 || i > 223) {
      EXPECT_TRUE(
          network_utils::IsReservedIPAddress(String::FromUTF8(address_string)));
    } else {
      EXPECT_FALSE(
          network_utils::IsReservedIPAddress(String::FromUTF8(address_string)));
    }
  }
}

TEST(NetworkUtilsTest, GetDomainAndRegistry) {
  EXPECT_EQ("", network_utils::GetDomainAndRegistry(
                    "", network_utils::kIncludePrivateRegistries));
  EXPECT_EQ("", network_utils::GetDomainAndRegistry(
                    ".", network_utils::kIncludePrivateRegistries));
  EXPECT_EQ("", network_utils::GetDomainAndRegistry(
                    "..", network_utils::kIncludePrivateRegistries));
  EXPECT_EQ("", network_utils::GetDomainAndRegistry(
                    "com", network_utils::kIncludePrivateRegistries));
  EXPECT_EQ("", network_utils::GetDomainAndRegistry(
                    ".com", network_utils::kIncludePrivateRegistries));
  EXPECT_EQ("", network_utils::GetDomainAndRegistry(
                    "www.example.com:8000",
                    network_utils::kIncludePrivateRegistries));

  EXPECT_EQ("", network_utils::GetDomainAndRegistry(
                    "localhost", network_utils::kIncludePrivateRegistries));
  EXPECT_EQ("", network_utils::GetDomainAndRegistry(
                    "127.0.0.1", network_utils::kIncludePrivateRegistries));

  EXPECT_EQ("example.com",
            network_utils::GetDomainAndRegistry(
                "example.com", network_utils::kIncludePrivateRegistries));
  EXPECT_EQ("example.com",
            network_utils::GetDomainAndRegistry(
                "www.example.com", network_utils::kIncludePrivateRegistries));
  EXPECT_EQ("example.com", network_utils::GetDomainAndRegistry(
                               "static.example.com",
                               network_utils::kIncludePrivateRegistries));
  EXPECT_EQ("example.com", network_utils::GetDomainAndRegistry(
                               "multilevel.www.example.com",
                               network_utils::kIncludePrivateRegistries));
  EXPECT_EQ("example.co.uk",
            network_utils::GetDomainAndRegistry(
                "www.example.co.uk", network_utils::kIncludePrivateRegistries));

  // Verify proper handling of 'private registries'.
  EXPECT_EQ("foo.appspot.com", network_utils::GetDomainAndRegistry(
                                   "www.foo.appspot.com",
                                   network_utils::kIncludePrivateRegistries));
  EXPECT_EQ("appspot.com", network_utils::GetDomainAndRegistry(
                               "www.foo.appspot.com",
                               network_utils::kExcludePrivateRegistries));

  // Verify that unknown registries are included.
  EXPECT_EQ("example.notarealregistry",
            network_utils::GetDomainAndRegistry(
                "www.example.notarealregistry",
                network_utils::kIncludePrivateRegistries));
}

}  // namespace blink
