// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/suffix_array.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <initializer_list>
#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

namespace {

using SLType = InducedSuffixSort::SLType;

}  // namespace

using ustring = std::vector<unsigned char>;

constexpr uint16_t kNumChar = 256;

ustring MakeUnsignedString(const std::string& str) {
  return {str.begin(), str.end()};
}

template <class T>
std::vector<T> MakeVector(const std::initializer_list<T>& ilist) {
  return {ilist.begin(), ilist.end()};
}

void TestSlPartition(std::initializer_list<SLType> expected_sl_partition,
                     std::initializer_list<size_t> expected_lms_indices,
                     std::string str) {
  using SaisImpl = InducedSuffixSort::Implementation<size_t, uint16_t>;

  std::vector<SLType> sl_partition(str.size());
  EXPECT_EQ(expected_lms_indices.size(),
            SaisImpl::BuildSLPartition(str.begin(), str.size(), kNumChar,
                                       sl_partition.rbegin()));
  EXPECT_EQ(MakeVector(expected_sl_partition), sl_partition);

  std::vector<size_t> lms_indices(expected_lms_indices.size());
  SaisImpl::FindLmsSuffixes(expected_sl_partition, lms_indices.begin());
  EXPECT_EQ(MakeVector(expected_lms_indices), lms_indices);
}

TEST(InducedSuffixSortTest, BuildSLPartition) {
  TestSlPartition({}, {}, "");
  TestSlPartition(
      {
          SLType::LType,
      },
      {}, "a");
  TestSlPartition(
      {
          SLType::LType,
          SLType::LType,
      },
      {}, "ba");
  TestSlPartition(
      {
          SLType::SType,
          SLType::LType,
      },
      {}, "ab");
  TestSlPartition(
      {
          SLType::SType,
          SLType::SType,
          SLType::LType,
      },
      {}, "aab");
  TestSlPartition(
      {
          SLType::LType,
          SLType::LType,
          SLType::LType,
      },
      {}, "bba");
  TestSlPartition(
      {
          SLType::LType,
          SLType::SType,
          SLType::LType,
      },
      {1}, "bab");
  TestSlPartition(
      {
          SLType::LType,
          SLType::SType,
          SLType::SType,
          SLType::LType,
      },
      {1}, "baab");

  TestSlPartition(
      {
          SLType::LType,  // zucchini
          SLType::LType,  // ucchini
          SLType::SType,  // cchini
          SLType::SType,  // chini
          SLType::SType,  // hini
          SLType::SType,  // ini
          SLType::LType,  // ni
          SLType::LType,  // i
      },
      {2}, "zucchini");
}

std::vector<size_t> BucketCount(const std::initializer_list<unsigned char> str,
                                uint16_t max_key) {
  using SaisImpl = InducedSuffixSort::Implementation<size_t, uint16_t>;
  return SaisImpl::MakeBucketCount(str.begin(), str.size(), max_key);
}

TEST(InducedSuffixSortTest, BucketCount) {
  using vec = std::vector<size_t>;

  EXPECT_EQ(vec({0, 0, 0, 0}), BucketCount({}, 4));
  EXPECT_EQ(vec({1, 0, 0, 0}), BucketCount({0}, 4));
  EXPECT_EQ(vec({0, 2, 0, 1}), BucketCount({1, 1, 3}, 4));
}

std::vector<size_t> InducedSortSubstring(ustring str) {
  using SaisImpl = InducedSuffixSort::Implementation<size_t, uint16_t>;
  std::vector<SLType> sl_partition(str.size());
  size_t lms_count = SaisImpl::BuildSLPartition(
      str.begin(), str.size(), kNumChar, sl_partition.rbegin());
  std::vector<size_t> lms_indices(lms_count);
  SaisImpl::FindLmsSuffixes(sl_partition, lms_indices.begin());
  auto buckets = SaisImpl::MakeBucketCount(str.begin(), str.size(), kNumChar);

  std::vector<size_t> suffix_array(str.size());
  SaisImpl::InducedSort(str, str.size(), sl_partition, lms_indices, buckets,
                        suffix_array.begin());

  return suffix_array;
}

TEST(InducedSuffixSortTest, InducedSortSubstring) {
  using vec = std::vector<size_t>;

  auto us = MakeUnsignedString;

  // L; a$
  EXPECT_EQ(vec({0}), InducedSortSubstring(us("a")));

  // SL; ab$, b$
  EXPECT_EQ(vec({0, 1}), InducedSortSubstring(us("ab")));

  // LL; a$, ba$
  EXPECT_EQ(vec({1, 0}), InducedSortSubstring(us("ba")));

  // SLL; a$, aba$, ba$
  EXPECT_EQ(vec({2, 0, 1}), InducedSortSubstring(us("aba")));

  // LSL; ab$, b$, ba
  EXPECT_EQ(vec({1, 2, 0}), InducedSortSubstring(us("bab")));

  // SSL; aab$, ab$, b$
  EXPECT_EQ(vec({0, 1, 2}), InducedSortSubstring(us("aab")));

  // LSSL; aab$, ab$, b$, ba
  EXPECT_EQ(vec({1, 2, 3, 0}), InducedSortSubstring(us("baab")));
}

template <class Algorithm>
void TestSuffixSort(ustring test_str) {
  std::vector<size_t> suffix_array =
      MakeSuffixArray<Algorithm>(test_str, kNumChar);
  EXPECT_EQ(test_str.size(), suffix_array.size());

  // Expect that I[] is a permutation of [0, len].
  std::vector<size_t> sorted_suffix(suffix_array.begin(), suffix_array.end());
  std::sort(sorted_suffix.begin(), sorted_suffix.end());
  for (size_t i = 0; i < test_str.size(); ++i)
    EXPECT_EQ(i, sorted_suffix[i]);

  // Expect that all suffixes are strictly ordered.
  auto end = test_str.end();
  for (size_t i = 1; i < test_str.size(); ++i) {
    auto suf1 = test_str.begin() + suffix_array[i - 1];
    auto suf2 = test_str.begin() + suffix_array[i];
    bool is_less = std::lexicographical_compare(suf1, end, suf2, end);
    EXPECT_TRUE(is_less);
  }
}

constexpr const char* test_strs[] = {
    "",
    "a",
    "aa",
    "za",
    "CACAO",
    "aaaaa",
    "banana",
    "tobeornottobe",
    "The quick brown fox jumps over the lazy dog.",
    "elephantelephantelephantelephantelephant",
    "walawalawashington",
    "-------------------------",
    "011010011001011010010110011010010",
    "3141592653589793238462643383279502884197169399375105",
    "\xFF\xFE\xFF\xFE\xFD\x80\x30\x31\x32\x80\x30\xFF\x01\xAB\xCD",
    "abccbaabccbaabccbaabccbaabccbaabccbaabccbaabccba",
    "0123456789876543210",
    "9876543210123456789",
    "aababcabcdabcdeabcdefabcdefg",
    "asdhklgalksdjghalksdjghalksdjgh",
};

TEST(SuffixSortTest, NaiveSuffixSort) {
  for (const std::string& test_str : test_strs) {
    TestSuffixSort<NaiveSuffixSort>(MakeUnsignedString(test_str));
  }
}

TEST(SuffixSortTest, InducedSuffixSortSort) {
  for (const std::string& test_str : test_strs) {
    TestSuffixSort<InducedSuffixSort>(MakeUnsignedString(test_str));
  }
}

// Test with sequence that has every character.
TEST(SuffixSortTest, AllChar) {
  std::vector<unsigned char> all_char(kNumChar);
  std::iota(all_char.begin(), all_char.end(), 0);

  {
    std::vector<size_t> suffix_array =
        MakeSuffixArray<InducedSuffixSort>(all_char, kNumChar);
    for (size_t i = 0; i < kNumChar; ++i)
      EXPECT_EQ(i, suffix_array[i]);
  }

  std::vector<unsigned char> all_char_reverse(all_char.rbegin(),
                                              all_char.rend());
  {
    std::vector<size_t> suffix_array =
        MakeSuffixArray<InducedSuffixSort>(all_char_reverse, kNumChar);
    for (size_t i = 0; i < kNumChar; ++i)
      EXPECT_EQ(kNumChar - i - 1, suffix_array[i]);
  }
}

void TestSuffixLowerBound(ustring base_str, ustring search_str) {
  std::vector<size_t> suffix_array =
      MakeSuffixArray<NaiveSuffixSort>(base_str, kNumChar);

  auto pos = SuffixLowerBound(suffix_array, base_str.begin(),
                              search_str.begin(), search_str.end());

  auto end = base_str.end();
  if (pos != suffix_array.begin()) {
    // Previous suffix is less than |search_str|.
    auto suf = base_str.begin() + pos[-1];
    bool is_less = std::lexicographical_compare(suf, end, search_str.begin(),
                                                search_str.end());
    EXPECT_TRUE(is_less);
  }
  if (pos != suffix_array.end()) {
    // Current suffix is greater of equal to |search_str|.
    auto suf = base_str.begin() + *pos;
    bool is_less = std::lexicographical_compare(suf, end, search_str.begin(),
                                                search_str.end());
    EXPECT_FALSE(is_less);
  }
}

TEST(SuffixArrayTest, LowerBound) {
  auto us = MakeUnsignedString;

  TestSuffixLowerBound(us(""), us(""));
  TestSuffixLowerBound(us(""), us("a"));
  TestSuffixLowerBound(us("b"), us(""));
  TestSuffixLowerBound(us("b"), us("a"));
  TestSuffixLowerBound(us("b"), us("c"));
  TestSuffixLowerBound(us("b"), us("bc"));
  TestSuffixLowerBound(us("aa"), us("a"));
  TestSuffixLowerBound(us("aa"), us("aa"));

  ustring sentence = us("the quick brown fox jumps over the lazy dog.");
  // Entire string: exact and unique.
  TestSuffixLowerBound(sentence, sentence);
  // Empty string: exact and non-unique.
  TestSuffixLowerBound(sentence, us(""));
  // Exact and unique suffix matches.
  TestSuffixLowerBound(sentence, us("."));
  TestSuffixLowerBound(sentence, us("the lazy dog."));
  // Exact and unique non-suffix matches.
  TestSuffixLowerBound(sentence, us("quick"));
  TestSuffixLowerBound(sentence, us("the quick"));
  // Partial and unique matches.
  TestSuffixLowerBound(sentence, us("fox jumps with the hosps"));
  TestSuffixLowerBound(sentence, us("xyz"));
  // Exact and non-unique match: take lexicographical first.
  TestSuffixLowerBound(sentence, us("the"));
  TestSuffixLowerBound(sentence, us(" "));
  // Partial and non-unique match.
  // query      < "the l"... < "the q"...
  TestSuffixLowerBound(sentence, us("the apple"));
  // "the l"... < query      < "the q"...
  TestSuffixLowerBound(sentence, us("the opera"));
  // "the l"... < "the q"... < query
  TestSuffixLowerBound(sentence, us("the zebra"));
  // Prefix match dominates suffix match (unique).
  TestSuffixLowerBound(sentence, us("over quick brown fox"));
  // Empty matchs.
  TestSuffixLowerBound(sentence, us(","));
  TestSuffixLowerBound(sentence, us("1234"));
  TestSuffixLowerBound(sentence, us("THE QUICK BROWN FOX"));
  TestSuffixLowerBound(sentence, us("(the"));
}

TEST(SuffixArrayTest, LowerBoundExact) {
  for (const std::string& test_str : test_strs) {
    ustring test_ustr = MakeUnsignedString(test_str);

    std::vector<size_t> suffix_array =
        MakeSuffixArray<InducedSuffixSort>(test_ustr, kNumChar);

    for (size_t lo = 0; lo < test_str.size(); ++lo) {
      for (size_t hi = lo + 1; hi <= test_str.size(); ++hi) {
        ustring query(test_ustr.begin() + lo, test_ustr.begin() + hi);
        ASSERT_EQ(query.size(), hi - lo);
        auto pos = SuffixLowerBound(suffix_array, test_ustr.begin(),
                                    query.begin(), query.end());
        EXPECT_TRUE(
            std::equal(query.begin(), query.end(), test_ustr.begin() + *pos));
      }
    }
  }
}

}  // namespace zucchini
