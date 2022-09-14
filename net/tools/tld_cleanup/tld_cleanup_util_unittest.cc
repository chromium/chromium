// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/tld_cleanup/tld_cleanup_util.h"

#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::tld_cleanup {

std::string SetupData(const std::string& icann_domains,
                      const std::string& private_domains) {
  return "// ===BEGIN ICANN DOMAINS===\n" +
         icann_domains +
         "// ===END ICANN DOMAINS===\n" +
         "// ===BEGIN PRIVATE DOMAINS===\n" +
         private_domains +
         "// ===END PRIVATE DOMAINS===\n";
}

TEST(TldCleanupUtilTest, TwoRealTldsSuccessfullyRead) {
  std::string icann_domains = "foo\n"
                              "bar\n";
  std::string private_domains = "";
  std::string data = SetupData(icann_domains, private_domains);
  RuleMap rules;
  NormalizeResult result = NormalizeDataToRuleMap(data, &rules);
  ASSERT_EQ(kSuccess, result);
  ASSERT_EQ(2U, rules.size());
  RuleMap::const_iterator foo_iter = rules.find("foo");
  ASSERT_FALSE(rules.end() == foo_iter);
  EXPECT_FALSE(foo_iter->second.wildcard);
  EXPECT_FALSE(foo_iter->second.exception);
  EXPECT_FALSE(foo_iter->second.is_private);
  RuleMap::const_iterator bar_iter = rules.find("bar");
  ASSERT_FALSE(rules.end() == bar_iter);
  EXPECT_FALSE(bar_iter->second.wildcard);
  EXPECT_FALSE(bar_iter->second.exception);
  EXPECT_FALSE(bar_iter->second.is_private);
}

TEST(TldCleanupUtilTest, RealTldAutomaticallyAddedForSubdomain) {
  std::string icann_domains = "foo.bar\n";
  std::string private_domains = "";
  std::string data = SetupData(icann_domains, private_domains);
  RuleMap rules;
  NormalizeResult result = NormalizeDataToRuleMap(data, &rules);
  ASSERT_EQ(kSuccess, result);
  ASSERT_EQ(2U, rules.size());
  RuleMap::const_iterator foo_bar_iter = rules.find("foo.bar");
  ASSERT_FALSE(rules.end() == foo_bar_iter);
  EXPECT_FALSE(foo_bar_iter->second.wildcard);
  EXPECT_FALSE(foo_bar_iter->second.exception);
  EXPECT_FALSE(foo_bar_iter->second.is_private);
  RuleMap::const_iterator bar_iter = rules.find("bar");
  ASSERT_FALSE(rules.end() == bar_iter);
  EXPECT_FALSE(bar_iter->second.wildcard);
  EXPECT_FALSE(bar_iter->second.exception);
  EXPECT_FALSE(bar_iter->second.is_private);
}

TEST(TldCleanupUtilTest, PrivateTldMarkedAsPrivate) {
  std::string icann_domains = "foo\n"
                              "bar\n";
  std::string private_domains = "baz\n";
  std::string data = SetupData(icann_domains, private_domains);
  RuleMap rules;
  NormalizeResult result = NormalizeDataToRuleMap(data, &rules);
  ASSERT_EQ(kSuccess, result);
  ASSERT_EQ(3U, rules.size());
  RuleMap::const_iterator foo_iter = rules.find("foo");
  ASSERT_FALSE(rules.end() == foo_iter);
  EXPECT_FALSE(foo_iter->second.wildcard);
  EXPECT_FALSE(foo_iter->second.exception);
  EXPECT_FALSE(foo_iter->second.is_private);
  RuleMap::const_iterator bar_iter = rules.find("bar");
  ASSERT_FALSE(rules.end() == bar_iter);
  EXPECT_FALSE(bar_iter->second.wildcard);
  EXPECT_FALSE(bar_iter->second.exception);
  EXPECT_FALSE(bar_iter->second.is_private);
  RuleMap::const_iterator baz_iter = rules.find("baz");
  ASSERT_FALSE(rules.end() == baz_iter);
  EXPECT_FALSE(baz_iter->second.wildcard);
  EXPECT_FALSE(baz_iter->second.exception);
  EXPECT_TRUE(baz_iter->second.is_private);
}

TEST(TldCleanupUtilTest, PrivateDomainMarkedAsPrivate) {
  std::string icann_domains = "bar\n";
  std::string private_domains = "foo.bar\n";
  std::string data = SetupData(icann_domains, private_domains);
  RuleMap rules;
  NormalizeResult result = NormalizeDataToRuleMap(data, &rules);
  ASSERT_EQ(kSuccess, result);
  ASSERT_EQ(2U, rules.size());
  RuleMap::const_iterator bar_iter = rules.find("bar");
  ASSERT_FALSE(rules.end() == bar_iter);
  EXPECT_FALSE(bar_iter->second.wildcard);
  EXPECT_FALSE(bar_iter->second.exception);
  EXPECT_FALSE(bar_iter->second.is_private);
  RuleMap::const_iterator foo_bar_iter = rules.find("foo.bar");
  ASSERT_FALSE(rules.end() == foo_bar_iter);
  EXPECT_FALSE(foo_bar_iter->second.wildcard);
  EXPECT_FALSE(foo_bar_iter->second.exception);
  EXPECT_TRUE(foo_bar_iter->second.is_private);
}

TEST(TldCleanupUtilTest, ExtraTldRuleIsNotMarkedPrivate) {
  std::string icann_domains = "foo.bar\n"
                              "baz.bar\n";
  std::string private_domains = "qux.bar\n";
  std::string data = SetupData(icann_domains, private_domains);
  RuleMap rules;
  NormalizeResult result = NormalizeDataToRuleMap(data, &rules);
  ASSERT_EQ(kSuccess, result);
  ASSERT_EQ(4U, rules.size());
  RuleMap::const_iterator foo_bar_iter = rules.find("foo.bar");
  ASSERT_FALSE(rules.end() == foo_bar_iter);
  EXPECT_FALSE(foo_bar_iter->second.wildcard);
  EXPECT_FALSE(foo_bar_iter->second.exception);
  EXPECT_FALSE(foo_bar_iter->second.is_private);
  RuleMap::const_iterator baz_bar_iter = rules.find("baz.bar");
  ASSERT_FALSE(rules.end() == baz_bar_iter);
  EXPECT_FALSE(baz_bar_iter->second.wildcard);
  EXPECT_FALSE(baz_bar_iter->second.exception);
  EXPECT_FALSE(baz_bar_iter->second.is_private);
  RuleMap::const_iterator bar_iter = rules.find("bar");
  ASSERT_FALSE(rules.end() == bar_iter);
  EXPECT_FALSE(bar_iter->second.wildcard);
  EXPECT_FALSE(bar_iter->second.exception);
  EXPECT_FALSE(bar_iter->second.is_private);
  RuleMap::const_iterator qux_bar_iter = rules.find("qux.bar");
  ASSERT_FALSE(rules.end() == qux_bar_iter);
  EXPECT_FALSE(qux_bar_iter->second.wildcard);
  EXPECT_FALSE(qux_bar_iter->second.exception);
  EXPECT_TRUE(qux_bar_iter->second.is_private);
}

TEST(TldCleanupUtilTest, WildcardAndExceptionParsedCorrectly) {
  std::string icann_domains = "*.bar\n"
                              "!foo.bar\n";
  std::string private_domains = "!baz.bar\n";
  std::string data = SetupData(icann_domains, private_domains);
  RuleMap rules;
  NormalizeResult result = NormalizeDataToRuleMap(data, &rules);
  ASSERT_EQ(kSuccess, result);
  ASSERT_EQ(3U, rules.size());
  RuleMap::const_iterator foo_bar_iter = rules.find("bar");
  ASSERT_FALSE(rules.end() == foo_bar_iter);
  EXPECT_TRUE(foo_bar_iter->second.wildcard);
  EXPECT_FALSE(foo_bar_iter->second.exception);
  EXPECT_FALSE(foo_bar_iter->second.is_private);
  RuleMap::const_iterator bar_iter = rules.find("foo.bar");
  ASSERT_FALSE(rules.end() == bar_iter);
  EXPECT_FALSE(bar_iter->second.wildcard);
  EXPECT_TRUE(bar_iter->second.exception);
  EXPECT_FALSE(bar_iter->second.is_private);
  RuleMap::const_iterator baz_bar_iter = rules.find("baz.bar");
  ASSERT_FALSE(rules.end() == baz_bar_iter);
  EXPECT_FALSE(baz_bar_iter->second.wildcard);
  EXPECT_TRUE(baz_bar_iter->second.exception);
  EXPECT_TRUE(baz_bar_iter->second.is_private);
}

}  // namespace net::tld_cleanup
