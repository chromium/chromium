// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/public/util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace dns_util {

TEST(DnsPublicUtilTest, IsValidDohTemplate) {
  std::string server_method;
  EXPECT_TRUE(IsValidDohTemplate(
      "https://dnsserver.example.net/dns-query{?dns}", &server_method));
  EXPECT_EQ("GET", server_method);

  EXPECT_TRUE(IsValidDohTemplate(
      "https://dnsserver.example.net/dns-query{?dns,extra}", &server_method));
  EXPECT_EQ("GET", server_method);

  EXPECT_TRUE(IsValidDohTemplate(
      "https://dnsserver.example.net/dns-query{?query}", &server_method));
  EXPECT_EQ("POST", server_method);

  EXPECT_TRUE(IsValidDohTemplate("https://dnsserver.example.net/dns-query",
                                 &server_method));
  EXPECT_EQ("POST", server_method);

  EXPECT_TRUE(IsValidDohTemplate("https://query:{dns}@dnsserver.example.net",
                                 &server_method));
  EXPECT_EQ("GET", server_method);

  EXPECT_TRUE(IsValidDohTemplate("https://dnsserver.example.net{/dns}",
                                 &server_method));
  EXPECT_EQ("GET", server_method);

  // Invalid template format
  EXPECT_FALSE(IsValidDohTemplate(
      "https://dnsserver.example.net/dns-query{{?dns}}", &server_method));
  // Must be HTTPS
  EXPECT_FALSE(IsValidDohTemplate("http://dnsserver.example.net/dns-query",
                                  &server_method));
  EXPECT_FALSE(IsValidDohTemplate(
      "http://dnsserver.example.net/dns-query{?dns}", &server_method));
  // Template must expand to a valid URL
  EXPECT_FALSE(IsValidDohTemplate("https://{?dns}", &server_method));
  // The hostname must not contain the dns variable
  EXPECT_FALSE(
      IsValidDohTemplate("https://{dns}.dnsserver.net", &server_method));
}

}  // namespace dns_util

}  // namespace net
