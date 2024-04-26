// Copyright 2020 The Chromium Authors
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

TEST(DnsAliasUtilityTest, FixUpDnsAliases) {
  // TODO(crbug.com/40256677) ' ' (0x20: SPACE) should not be escaped.
  const std::set<std::string> kAliases = {
      "localhost", "1.2.3.4", "a.com",     "",           "test",
      "0",         "[::1]",   "::1",       "-www.e.com", "alias.com",
      "s .de",     "www-1",   "2a",        "a-",         "b..net",
      "a.com",     "b_o.org", "alias.com", "1..3.2",     "1.2.3.09",
      "foo.4",     "a,b,c",   "f/g",       "www?",       "[3a2:401f::1]",
      "0.0.1.2",   "a.b.com", "c.org",     "123.tld",    "d-e.net",
      "f__g",      "h"};
  const std::set<std::string> kExpectedFixedUpAliases = {
      "a.com",   "test",    "-www.e.com", "alias.com", "s%20.de", "www-1",
      "2a",      "a-",      "b_o.org",    "a,b,c",     "a.b.com", "c.org",
      "123.tld", "d-e.net", "f__g",       "h"};

  std::set<std::string> fixed_up_aliases =
      dns_alias_utility::FixUpDnsAliases(kAliases);
  EXPECT_EQ(kExpectedFixedUpAliases, fixed_up_aliases);

  std::string long_unqualified_alias =
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abcd";
  std::string long_qualified_alias =
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abc.";
  std::set<std::string> set_with_long_aliases(
      {long_unqualified_alias, long_qualified_alias});

  std::set<std::string> fixed_up_set_with_long_aliases =
      dns_alias_utility::FixUpDnsAliases(set_with_long_aliases);
  EXPECT_THAT(fixed_up_set_with_long_aliases,
              testing::ElementsAre(long_qualified_alias));

  std::set<std::string> empty_aliases;
  std::set<std::string> fixed_up_empty_aliases =
      dns_alias_utility::FixUpDnsAliases(empty_aliases);
  EXPECT_TRUE(fixed_up_empty_aliases.empty());
}

}  // namespace
}  // namespace net
