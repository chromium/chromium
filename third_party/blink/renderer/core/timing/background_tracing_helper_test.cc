// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/background_tracing_helper.h"

#include <optional>
#include <string_view>

#include "base/hash/md5_constexpr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class BackgroundTracingHelperTest : public testing::Test {
 public:
  using SiteHashSet = BackgroundTracingHelper::SiteHashSet;

  BackgroundTracingHelperTest() = default;
  ~BackgroundTracingHelperTest() override = default;

  static size_t GetIdSuffixPos(StringView string) {
    return BackgroundTracingHelper::GetIdSuffixPos(string);
  }

  static uint32_t MD5Hash32(std::string_view string) {
    return BackgroundTracingHelper::MD5Hash32(string);
  }

  static std::pair<StringView, std::optional<uint32_t>> SplitMarkNameAndId(
      StringView mark_name) {
    return BackgroundTracingHelper::SplitMarkNameAndId(mark_name);
  }

  static SiteHashSet ParsePerformanceMarkSiteHashes(
      const std::string& allow_list) {
    return BackgroundTracingHelper::ParsePerformanceMarkSiteHashes(allow_list);
  }
  test::TaskEnvironment task_environment_;
};

TEST_F(BackgroundTracingHelperTest, GetIdSuffixPos) {
  static constexpr char kFailNoSuffix[] = "nosuffixatall";
  static constexpr char kFailNoUnderscore[] = "missingunderscore123";
  static constexpr char kFailUnderscoreOnly[] = "underscoreonly_";
  static constexpr char kFailNoPrefix[] = "_123";
  EXPECT_EQ(0u, GetIdSuffixPos(kFailNoSuffix));
  EXPECT_EQ(0u, GetIdSuffixPos(kFailNoUnderscore));
  EXPECT_EQ(0u, GetIdSuffixPos(kFailUnderscoreOnly));
  EXPECT_EQ(0u, GetIdSuffixPos(kFailNoPrefix));

  static constexpr char kSuccess0[] = "success_1";
  static constexpr char kSuccess1[] = "thisworks_123";
  EXPECT_EQ(7u, GetIdSuffixPos(kSuccess0));
  EXPECT_EQ(9u, GetIdSuffixPos(kSuccess1));
}

TEST_F(BackgroundTracingHelperTest, MD5Hash32) {
  static constexpr char kFoo[] = "foo";
  static constexpr uint32_t kFooHash = 0xacbd18db;
  static_assert(kFooHash == base::MD5Hash32Constexpr(kFoo), "unexpected hash");
  EXPECT_EQ(kFooHash, MD5Hash32(kFoo));

  static constexpr char kQuickFox[] =
      "the quick fox jumps over the lazy brown dog";
  static constexpr uint32_t kQuickFoxHash = 0x01275c33;
  static_assert(kQuickFoxHash == base::MD5Hash32Constexpr(kQuickFox),
                "unexpected hash");
  EXPECT_EQ(kQuickFoxHash, MD5Hash32(kQuickFox));
}

TEST_F(BackgroundTracingHelperTest, GetMarkHashAndSequenceNumber) {
  static constexpr char kNoSuffix[] = "trigger:foo";
  static constexpr char kInvalidSuffix0[] = "trigger:foo_";
  static constexpr char kInvalidSuffix1[] = "trigger:foo123";
  static constexpr char kHasSuffix[] = "trigger:foo_123";

  {
    auto result = SplitMarkNameAndId(kNoSuffix);
    EXPECT_EQ("foo", result.first);
    EXPECT_EQ(std::nullopt, result.second);
  }

  {
    auto result = SplitMarkNameAndId(kInvalidSuffix0);
    EXPECT_EQ("foo_", result.first);
    EXPECT_EQ(std::nullopt, result.second);
  }

  {
    auto result = SplitMarkNameAndId(kInvalidSuffix1);
    EXPECT_EQ("foo123", result.first);
    EXPECT_EQ(std::nullopt, result.second);
  }

  {
    auto result = SplitMarkNameAndId(kHasSuffix);
    EXPECT_EQ("foo", result.first);
    EXPECT_EQ(123u, result.second);
  }
}

TEST_F(BackgroundTracingHelperTest, ParsePerformanceMarkSiteHashes) {
  // A list with an too long site hash is invalid.
  EXPECT_EQ(SiteHashSet{}, ParsePerformanceMarkSiteHashes("00deadc0de"));

  // A list with a non-hex mark hash is invalid.
  EXPECT_EQ(SiteHashSet{}, ParsePerformanceMarkSiteHashes("deadc0de,nothex"));

  {
    auto hashes = ParsePerformanceMarkSiteHashes("");
    EXPECT_TRUE(hashes.empty());
  }

  {
    auto hashes = ParsePerformanceMarkSiteHashes(",abcd,");
    EXPECT_EQ(1u, hashes.size());
    EXPECT_TRUE(hashes.Contains(0x0000abcd));
  }

  {
    auto hashes = ParsePerformanceMarkSiteHashes("aabbccdd");
    EXPECT_EQ(1u, hashes.size());
    EXPECT_TRUE(hashes.Contains(0xaabbccdd));
  }

  {
    auto hashes = ParsePerformanceMarkSiteHashes("bcd,aabbccdd");
    EXPECT_EQ(2u, hashes.size());
    EXPECT_TRUE(hashes.Contains(0x00000bcd));
    EXPECT_TRUE(hashes.Contains(0xaabbccdd));
  }
}

}  // namespace blink
