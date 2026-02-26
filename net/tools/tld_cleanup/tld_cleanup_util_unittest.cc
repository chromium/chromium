// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/tld_cleanup/tld_cleanup_util.h"

#include <initializer_list>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "net/base/registry_controlled_domain_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::tld_cleanup {

using testing::ElementsAre;
using testing::Pair;

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
  std::string icann_domains =
      "foo\n"
      "bar\n";
  std::string private_domains = "";
  EXPECT_THAT(
      NormalizeDataToRuleMap(SetupData(icann_domains, private_domains)),
      Pair(NormalizeResult::kSuccess,
           ElementsAre(Pair("bar", Rule{/*exception=*/false, /*wildcard=*/false,
                                        /*is_private=*/false}),
                       Pair("foo", Rule{/*exception=*/false, /*wildcard=*/false,
                                        /*is_private=*/false}))));
}

TEST(TldCleanupUtilTest, TwoRealTldsSuccessfullyRead_WindowsEndings) {
  std::string icann_domains =
      "foo\r\n"
      "bar\r\n";
  std::string private_domains = "";
  EXPECT_THAT(
      NormalizeDataToRuleMap(SetupData(icann_domains, private_domains)),
      Pair(NormalizeResult::kSuccess,
           ElementsAre(Pair("bar", Rule{/*exception=*/false, /*wildcard=*/false,
                                        /*is_private=*/false}),
                       Pair("foo", Rule{/*exception=*/false, /*wildcard=*/false,
                                        /*is_private=*/false}))));
}

TEST(TldCleanupUtilTest, RealTldAutomaticallyAddedForSubdomain) {
  std::string icann_domains = "foo.bar\n";
  std::string private_domains = "";
  EXPECT_THAT(
      NormalizeDataToRuleMap(SetupData(icann_domains, private_domains)),
      Pair(NormalizeResult::kSuccess,
           ElementsAre(
               Pair("bar", Rule{/*exception=*/false, /*wildcard=*/false,
                                /*is_private=*/false}),
               Pair("foo.bar", Rule{/*exception=*/false, /*wildcard=*/false,
                                    /*is_private=*/false}))));
}

TEST(TldCleanupUtilTest, PrivateTldMarkedAsPrivate) {
  std::string icann_domains =
      "foo\n"
      "bar\n";
  std::string private_domains = "baz\n";
  EXPECT_THAT(
      NormalizeDataToRuleMap(SetupData(icann_domains, private_domains)),
      Pair(NormalizeResult::kSuccess,
           ElementsAre(Pair("bar", Rule{/*exception=*/false, /*wildcard=*/false,
                                        /*is_private=*/false}),
                       Pair("baz", Rule{/*exception=*/false, /*wildcard=*/false,
                                        /*is_private=*/true}),
                       Pair("foo", Rule{/*exception=*/false, /*wildcard=*/false,
                                        /*is_private=*/false}))));
}

TEST(TldCleanupUtilTest, PrivateDomainMarkedAsPrivate) {
  std::string icann_domains = "bar\n";
  std::string private_domains = "foo.bar\n";
  EXPECT_THAT(
      NormalizeDataToRuleMap(SetupData(icann_domains, private_domains)),
      Pair(NormalizeResult::kSuccess,
           ElementsAre(
               Pair("bar", Rule{/*exception=*/false, /*wildcard=*/false,
                                /*is_private=*/false}),
               Pair("foo.bar", Rule{/*exception=*/false, /*wildcard=*/false,
                                    /*is_private=*/true}))));
}

TEST(TldCleanupUtilTest, ExtraTldRuleIsNotMarkedPrivate) {
  std::string icann_domains =
      "foo.bar\n"
      "baz.bar\n";
  std::string private_domains = "qux.bar\n";
  EXPECT_THAT(
      NormalizeDataToRuleMap(SetupData(icann_domains, private_domains)),
      Pair(NormalizeResult::kSuccess,
           ElementsAre(
               Pair("bar", Rule{/*exception=*/false, /*wildcard=*/false,
                                /*is_private=*/false}),
               Pair("baz.bar", Rule{/*exception=*/false, /*wildcard=*/false,
                                    /*is_private=*/false}),
               Pair("foo.bar", Rule{/*exception=*/false, /*wildcard=*/false,
                                    /*is_private=*/false}),
               Pair("qux.bar", Rule{/*exception=*/false, /*wildcard=*/false,
                                    /*is_private=*/true}))));
}

TEST(TldCleanupUtilTest, WildcardAndExceptionParsedCorrectly) {
  std::string icann_domains =
      "*.bar\n"
      "!foo.bar\n";
  std::string private_domains = "!baz.bar\n";
  EXPECT_THAT(
      NormalizeDataToRuleMap(SetupData(icann_domains, private_domains)),
      Pair(NormalizeResult::kSuccess,
           ElementsAre(
               Pair("bar", Rule{/*exception=*/false, /*wildcard=*/true,
                                /*is_private=*/false}),
               Pair("baz.bar", Rule{/*exception=*/true, /*wildcard=*/false,
                                    /*is_private=*/true}),
               Pair("foo.bar", Rule{/*exception=*/true, /*wildcard=*/false,
                                    /*is_private=*/false}))));
}

TEST(TldCleanupUtilTest, RuleSerialization) {
  EXPECT_THAT(
      RulesToGperf({
          {"domain0",
           Rule{/*exception=*/false, /*wildcard=*/false, /*is_private=*/false}},
          {"domain1",
           Rule{/*exception=*/false, /*wildcard=*/false, /*is_private=*/true}},
          {"domain2",
           Rule{/*exception=*/false, /*wildcard=*/true, /*is_private=*/false}},
          {"domain3",
           Rule{/*exception=*/false, /*wildcard=*/true, /*is_private=*/true}},
          {"domain4",
           Rule{/*exception=*/true, /*wildcard=*/false, /*is_private=*/false}},
          {"domain5",
           Rule{/*exception=*/true, /*wildcard=*/false, /*is_private=*/true}},
          {"domain6",
           Rule{/*exception=*/true, /*wildcard=*/true, /*is_private=*/false}},
          {"domain7",
           Rule{/*exception=*/true, /*wildcard=*/true, /*is_private=*/true}},
      }),
      testing::EndsWith(
          R"(%%
domain0, 0
domain1, 4
domain2, 2
domain3, 6
domain4, 1
domain5, 5
domain6, 1
domain7, 5
%%
)"));
}

TEST(TldCleanupUtilTest, RuleSerialize) {
  using enum DomainRuleTag;
  constexpr auto get_bitmask = [](std::initializer_list<DomainRuleTag> tags) {
    return DomainRuleTags(tags).ToEnumBitmask();
  };

  EXPECT_EQ(Rule(/*exception=*/false, /*wildcard=*/false, /*is_private=*/false)
                .Serialize(),
            get_bitmask({}));
  EXPECT_EQ(Rule(/*exception=*/true, /*wildcard=*/false, /*is_private=*/false)
                .Serialize(),
            get_bitmask({kException}));
  EXPECT_EQ(Rule(/*exception=*/false, /*wildcard=*/true, /*is_private=*/false)
                .Serialize(),
            get_bitmask({kWildcard}));
  // `exception` takes precedence over `wildcard`.
  EXPECT_EQ(Rule(/*exception=*/true, /*wildcard=*/true, /*is_private=*/false)
                .Serialize(),
            get_bitmask({kException}));

  EXPECT_EQ(Rule(/*exception=*/false, /*wildcard=*/false, /*is_private=*/true)
                .Serialize(),
            get_bitmask({kPrivate}));
  EXPECT_EQ(Rule(/*exception=*/true, /*wildcard=*/false, /*is_private=*/true)
                .Serialize(),
            get_bitmask({kException, kPrivate}));
  EXPECT_EQ(Rule(/*exception=*/false, /*wildcard=*/true, /*is_private=*/true)
                .Serialize(),
            get_bitmask({kWildcard, kPrivate}));
  // `exception` takes precedence over `wildcard`.
  EXPECT_EQ(Rule(/*exception=*/true, /*wildcard=*/true, /*is_private=*/true)
                .Serialize(),
            get_bitmask({kException, kPrivate}));
}

TEST(TldCleanupUtilTest, GperfIsUpToDate) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const base::FilePath source_dir =
      base::PathService::CheckedGet(base::DIR_SRC_TEST_DATA_ROOT)
          .AppendASCII("net")
          .AppendASCII("base")
          .AppendASCII("registry_controlled_domains");

  const base::FilePath temp_file = temp_dir.GetPath().AppendASCII("temp.gperf");
  ASSERT_EQ(NormalizeFile(source_dir.AppendASCII("effective_tld_names.dat"),
                          temp_file),
            NormalizeResult::kSuccess);

  EXPECT_TRUE(base::ContentsEqual(
      source_dir.AppendASCII("effective_tld_names.gperf"), temp_file));
}

}  // namespace net::tld_cleanup
