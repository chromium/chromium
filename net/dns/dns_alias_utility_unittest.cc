// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_alias_utility.h"

#include <string>
#include <vector>

#include "net/dns/public/dns_protocol.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

TEST(DnsAliasUtilityTest, SanitizeDnsAliases) {
  const struct {
    const char* dns_alias;
    const char* sanitized_dns_alias;
  } kTestCases[] = {{"localhost", nullptr},
                    {"1.2.3.4", nullptr},
                    {"a.com", "a.com"},
                    {"", nullptr},
                    {"test", "test"},
                    {"0", nullptr},
                    {"[::1]", nullptr},
                    {"::1", nullptr},
                    {"-www.e.com", "-www.e.com"},
                    {"alias.com", "alias.com"},
                    {"s .de", "s%20.de"},
                    {"www-1", "www-1"},
                    {"2a", "2a"},
                    {"a-", "a-"},
                    {"b..net", "b..net"},
                    {"a.com", nullptr},
                    {"b_o.org", "b_o.org"},
                    {"alias.com", nullptr},
                    {"1..3.2", "1..3.2"},
                    {"a,b,c", "a%2Cb%2Cc"},
                    {"f/g", nullptr},
                    {"www?", nullptr},
                    {"[3a2:401f::1]", nullptr},
                    {"0.0.1.2", nullptr},
                    {"a.b.com", "a.b.com"},
                    {"c.org", "c.org"},
                    {"123.tld", "123.tld"},
                    {"d-e.net", "d-e.net"},
                    {"f__g", "f__g"},
                    {"h", "h"}};

  std::vector<std::string> aliases;
  std::vector<std::string> expected_sanitized_aliases;

  for (const auto& test : kTestCases) {
    aliases.push_back(test.dns_alias);
    if (test.sanitized_dns_alias)
      expected_sanitized_aliases.push_back(test.sanitized_dns_alias);
  }

  std::vector<std::string> sanitized_aliases =
      dns_alias_utility::SanitizeDnsAliases(aliases);
  EXPECT_EQ(expected_sanitized_aliases, sanitized_aliases);

  std::string long_unqualified_alias(dns_protocol::kMaxCharNameLength + 1, 'x');
  std::string long_qualified_alias(dns_protocol::kMaxCharNameLength, 'x');
  long_qualified_alias += ".";
  std::vector<std::string> List_with_long_aliases(
      {long_unqualified_alias, long_qualified_alias});

  std::vector<std::string> sanitized_list_with_long_aliases =
      dns_alias_utility::SanitizeDnsAliases(List_with_long_aliases);
  EXPECT_THAT(sanitized_list_with_long_aliases,
              testing::ElementsAre(long_qualified_alias));

  std::vector<std::string> empty_aliases;
  std::vector<std::string> sanitized_empty_aliases =
      dns_alias_utility::SanitizeDnsAliases(empty_aliases);
  EXPECT_TRUE(sanitized_empty_aliases.empty());
}

}  // namespace
}  // namespace net
