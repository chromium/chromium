// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/background_tracing_helper.h"

#include "base/hash/md5_constexpr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class BackgroundTracingHelperTest : public testing::Test {
 public:
  using SiteMarkHashMap = BackgroundTracingHelper::SiteMarkHashMap;
  using MarkHashSet = BackgroundTracingHelper::MarkHashSet;

  BackgroundTracingHelperTest() = default;
  ~BackgroundTracingHelperTest() override = default;

  static size_t GetSequenceNumberPos(base::StringPiece string) {
    return BackgroundTracingHelper::GetSequenceNumberPos(string);
  }

  static uint32_t MD5Hash32(base::StringPiece string) {
    return BackgroundTracingHelper::MD5Hash32(string);
  }

  static void GetMarkHashAndSequenceNumber(base::StringPiece mark_name,
                                           uint32_t sequence_number_offset,
                                           uint32_t* mark_hash,
                                           uint32_t* sequence_number) {
    return BackgroundTracingHelper::GetMarkHashAndSequenceNumber(
        mark_name, sequence_number_offset, mark_hash, sequence_number);
  }

  static bool ParseBackgroundTracingPerformanceMarkHashes(
      const std::string& allow_list,
      SiteMarkHashMap& allow_listed_hashes) {
    return BackgroundTracingHelper::ParseBackgroundTracingPerformanceMarkHashes(
        allow_list, allow_listed_hashes);
  }
};

TEST_F(BackgroundTracingHelperTest, GetSequenceNumberPos) {
  static constexpr char kFailNoSuffix[] = "nosuffixatall";
  static constexpr char kFailNoUnderscore[] = "missingunderscore123";
  static constexpr char kFailUnderscoreOnly[] = "underscoreonly_";
  static constexpr char kFailNoPrefix[] = "_123";
  EXPECT_EQ(0u, GetSequenceNumberPos(kFailNoSuffix));
  EXPECT_EQ(0u, GetSequenceNumberPos(kFailNoUnderscore));
  EXPECT_EQ(0u, GetSequenceNumberPos(kFailUnderscoreOnly));
  EXPECT_EQ(0u, GetSequenceNumberPos(kFailNoPrefix));

  static constexpr char kSuccess0[] = "success_1";
  static constexpr char kSuccess1[] = "thisworks_123";
  EXPECT_EQ(7u, GetSequenceNumberPos(kSuccess0));
  EXPECT_EQ(9u, GetSequenceNumberPos(kSuccess1));
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
  static constexpr char kNoSuffix[] = "foo";
  static constexpr char kInvalidSuffix0[] = "foo_";
  static constexpr char kInvalidSuffix1[] = "foo123";
  static constexpr char kHasSuffix[] = "foo_123";

  uint32_t mark_hash = 0;
  uint32_t sequence_number = 0;

  GetMarkHashAndSequenceNumber(kNoSuffix, 0, &mark_hash, &sequence_number);
  EXPECT_EQ(0xacbd18dbu, mark_hash);
  EXPECT_EQ(0u, sequence_number);

  GetMarkHashAndSequenceNumber(kInvalidSuffix0, 0, &mark_hash,
                               &sequence_number);
  EXPECT_EQ(0x2023d768u, mark_hash);
  EXPECT_EQ(0u, sequence_number);

  GetMarkHashAndSequenceNumber(kInvalidSuffix1, 0, &mark_hash,
                               &sequence_number);
  EXPECT_EQ(0xef238ea0u, mark_hash);
  EXPECT_EQ(0u, sequence_number);

  GetMarkHashAndSequenceNumber(kHasSuffix, 0, &mark_hash, &sequence_number);
  EXPECT_EQ(0xacbd18db, mark_hash);
  EXPECT_EQ(123u, sequence_number);

  // Ensure that capping and offset logic works.
  GetMarkHashAndSequenceNumber(kHasSuffix, 7457, &mark_hash, &sequence_number);
  EXPECT_EQ(0xacbd18dbu, mark_hash);
  EXPECT_EQ(580u, sequence_number);
}

TEST_F(BackgroundTracingHelperTest,
       ParseBackgroundTracingPerformanceMarkHashes) {
  SiteMarkHashMap hashes;
  constexpr uint32_t kSiteHash1 = 0xdeadc0de;

  // A list with a valid site hash not followed by an '=' is invalid.
  EXPECT_FALSE(ParseBackgroundTracingPerformanceMarkHashes("deadc0de", hashes));
  EXPECT_TRUE(hashes.empty());

  // A list with invalid characters in the site hash is invalid.
  EXPECT_FALSE(
      ParseBackgroundTracingPerformanceMarkHashes("nothex=aabbccdd", hashes));
  EXPECT_TRUE(hashes.empty());

  // A list with an.IsEmpty site hash is invalid.
  EXPECT_FALSE(
      ParseBackgroundTracingPerformanceMarkHashes("=aabbccdd", hashes));
  EXPECT_TRUE(hashes.empty());

  // A list with an too long site hash is invalid.
  EXPECT_FALSE(ParseBackgroundTracingPerformanceMarkHashes(
      "00deadc0de=aabbccdd", hashes));
  EXPECT_TRUE(hashes.empty());

  // A list with no mark hashes is invalid.
  EXPECT_FALSE(
      ParseBackgroundTracingPerformanceMarkHashes("deadc0de=", hashes));
  EXPECT_TRUE(hashes.empty());

  // A list with an.IsEmpty mark hash is invalid.
  EXPECT_FALSE(ParseBackgroundTracingPerformanceMarkHashes("deadc0de=,aabbccdd",
                                                           hashes));
  EXPECT_TRUE(hashes.empty());

  // A list with a too long mark hash is invalid.
  EXPECT_FALSE(ParseBackgroundTracingPerformanceMarkHashes(
      "deadc0de=aabbccddee", hashes));
  EXPECT_TRUE(hashes.empty());

  // A list with a non-hex mark hash is invalid.
  EXPECT_FALSE(
      ParseBackgroundTracingPerformanceMarkHashes("deadc0de=nothex", hashes));
  EXPECT_TRUE(hashes.empty());

  // Parsing an empty list is valid, but the return should be empty as well.
  EXPECT_TRUE(ParseBackgroundTracingPerformanceMarkHashes("", hashes));
  EXPECT_TRUE(hashes.empty());

  // Expect a single mark hash to be parsed.
  EXPECT_TRUE(
      ParseBackgroundTracingPerformanceMarkHashes("dEADc0de=aabbccdd", hashes));
  EXPECT_EQ(1u, hashes.size());
  EXPECT_TRUE(hashes.Contains(kSiteHash1));
  {
    const auto& mark_hashes = hashes.find(kSiteHash1)->value;
    EXPECT_EQ(1u, mark_hashes.size());
    EXPECT_TRUE(mark_hashes.Contains(0xaabbccdd));
  }

  // Expect multiple mark hashes to be parsed.
  EXPECT_TRUE(ParseBackgroundTracingPerformanceMarkHashes(
      "dEADc0de=aabbccdd,bcd", hashes));
  EXPECT_EQ(1u, hashes.size());
  EXPECT_TRUE(hashes.Contains(kSiteHash1));
  {
    const auto& mark_hashes = hashes.find(kSiteHash1)->value;
    EXPECT_EQ(2u, mark_hashes.size());
    EXPECT_TRUE(mark_hashes.Contains(0x00000bcd));
    EXPECT_TRUE(mark_hashes.Contains(0xaabbccdd));
  }

  // Expect even more mark hashes to be parsed, and allow a trailing ';' to be
  // ignored.
  EXPECT_TRUE(ParseBackgroundTracingPerformanceMarkHashes(
      "dEADc0de=aabbccdd,bcd,bbCCddee;", hashes));
  EXPECT_EQ(1u, hashes.size());
  EXPECT_TRUE(hashes.Contains(kSiteHash1));
  {
    const auto& mark_hashes = hashes.find(kSiteHash1)->value;
    EXPECT_EQ(3u, mark_hashes.size());
    EXPECT_TRUE(mark_hashes.Contains(0x00000bcd));
    EXPECT_TRUE(mark_hashes.Contains(0xaabbccdd));
    EXPECT_TRUE(mark_hashes.Contains(0xbbccddee));
  }

  // Expect a list with multiple sites to be parsed.
  constexpr uint32_t kSiteHash2 = 0xa0b0c0d0;
  constexpr uint32_t kSiteHash3 = 0xb0b0b0b0;
  EXPECT_TRUE(ParseBackgroundTracingPerformanceMarkHashes(
      "a0b0c0d0=aabbccdd;dEADc0de=aabbccdd,00000bcd,bbCCddee;b0b0b0b0=abc,b0e",
      hashes));
  EXPECT_EQ(3u, hashes.size());
  EXPECT_TRUE(hashes.Contains(kSiteHash1));
  EXPECT_TRUE(hashes.Contains(kSiteHash2));
  EXPECT_TRUE(hashes.Contains(kSiteHash3));
  {
    const auto& mark_hashes = hashes.find(kSiteHash1)->value;
    EXPECT_EQ(3u, mark_hashes.size());
    EXPECT_TRUE(mark_hashes.Contains(0x00000bcd));
    EXPECT_TRUE(mark_hashes.Contains(0xaabbccdd));
    EXPECT_TRUE(mark_hashes.Contains(0xbbccddee));
  }
  {
    const auto& mark_hashes = hashes.find(kSiteHash2)->value;
    EXPECT_EQ(1u, mark_hashes.size());
    EXPECT_TRUE(mark_hashes.Contains(0xaabbccdd));
  }
  {
    const auto& mark_hashes = hashes.find(kSiteHash3)->value;
    EXPECT_EQ(2u, mark_hashes.size());
    EXPECT_TRUE(mark_hashes.Contains(0x00000abc));
    EXPECT_TRUE(mark_hashes.Contains(0x00000b0e));
  }
}

}  // namespace blink
