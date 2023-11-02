// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/third_party/bsdiff/bsdiff_search.h"

#include <cstring>

#include "courgette/third_party/bsdiff/paged_array.h"
#include "courgette/third_party/divsufsort/divsufsort.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(BSDiffSearchTest, Search) {
  // Initialize main string and the suffix array.
  // Positions:      000000000011111111111222222222333333333344444
  //                 012345678901234567890123456789012345678901234
  const char* str = "the quick brown fox jumps over the lazy dog.";
  int size = static_cast<int>(::strlen(str));
  const unsigned char* buf = reinterpret_cast<const unsigned char*>(str);
  courgette::PagedArray<divsuf::saidx_t> I;
  ASSERT_TRUE(I.Allocate(size + 1));
  divsuf::divsufsort_include_empty(buf, I.begin(), size);

  // Specific queries.
  const struct {
    int exp_match_pos;  // -1 means "don't care".
    int exp_match_size;
    const char* query_str;
  } test_cases[] = {
      // Entire string: exact and unique.
      {0, 44, "the quick brown fox jumps over the lazy dog."},
      // Empty string: exact and non-unique.
      {-1, 0, ""},
      // Exact and unique suffix matches.
      {43, 1, "."},
      {31, 13, "the lazy dog."},
      // Exact and unique non-suffix matches.
      {4, 5, "quick"},
      {0, 9, "the quick"},  // Unique prefix.
      // Partial and unique matches.
      {16, 10, "fox jumps with the hosps"},  // Unique prefix.
      {18, 1, "xyz"},
      // Exact and non-unique match: take lexicographical first.
      {-1, 3, "the"},  // Non-unique prefix.
      {-1, 1, " "},
      // Partial and non-unique match: no guarantees on |match.pos|!
      {-1, 4, "the apple"},  // query      < "the l"... < "the q"...
      {-1, 4, "the opera"},  // "the l"... < query      < "the q"...
      {-1, 4, "the zebra"},  // "the l"... < "the q"... < query
      // Prefix match dominates suffix match (unique).
      {26, 5, "over quick brown fox"},
      // Empty matchs.
      {-1, 0, ","},
      {-1, 0, "1234"},
      {-1, 0, "THE QUICK BROWN FOX"},
      {-1, 0, "(the"},
  };

  for (size_t idx = 0; idx < std::size(test_cases); ++idx) {
    const auto& test_case = test_cases[idx];
    int query_size = static_cast<int>(::strlen(test_case.query_str));
    const unsigned char* query_buf =
        reinterpret_cast<const unsigned char*>(test_case.query_str);

    // Perform the search.
    bsdiff::SearchResult match =
        bsdiff::search<courgette::PagedArray<divsuf::saidx_t>&>(
            I, buf, size, query_buf, query_size);

    // Check basic properties and match with expected values.
    EXPECT_GE(match.size, 0);
    EXPECT_LE(match.size, query_size);
    if (match.size > 0) {
      EXPECT_GE(match.pos, 0);
      EXPECT_LE(match.pos, size - match.size);
      EXPECT_EQ(0, ::memcmp(buf + match.pos, query_buf, match.size));
    }
    if (test_case.exp_match_pos >= 0) {
      EXPECT_EQ(test_case.exp_match_pos, match.pos);
    }
    EXPECT_EQ(test_case.exp_match_size, match.size);
  }
}

TEST(BSDiffSearchTest, SearchExact) {
  const char* test_cases[] = {
      "a",
      "aa",
      "az",
      "za",
      "aaaaa",
      "CACAO",
      "banana",
      "tobeornottobe",
      "the quick brown fox jumps over the lazy dog.",
      "elephantelephantelephantelephantelephant",
      "011010011001011010010110011010010",
  };
  for (size_t idx = 0; idx < std::size(test_cases); ++idx) {
    int size = static_cast<int>(::strlen(test_cases[idx]));
    const unsigned char* buf =
        reinterpret_cast<const unsigned char*>(test_cases[idx]);
    courgette::PagedArray<divsuf::saidx_t> I;
    ASSERT_TRUE(I.Allocate(size + 1));
    divsuf::divsufsort_include_empty(buf, I.begin(), size);

    // Test exact matches for every non-empty substring.
    for (int lo = 0; lo < size; ++lo) {
      for (int hi = lo + 1; hi <= size; ++hi) {
        std::string query(buf + lo, buf + hi);
        int query_size = static_cast<int>(query.length());
        ASSERT_EQ(query_size, hi - lo);
        const unsigned char* query_buf =
            reinterpret_cast<const unsigned char*>(query.c_str());
        bsdiff::SearchResult match =
            bsdiff::search<courgette::PagedArray<divsuf::saidx_t>&>(
                I, buf, size, query_buf, query_size);

        EXPECT_EQ(query_size, match.size);
        EXPECT_GE(match.pos, 0);
        EXPECT_LE(match.pos, size - match.size);
        std::string suffix(buf + match.pos, buf + size);
        EXPECT_EQ(suffix.substr(0, query_size), query);
      }
    }
  }
}
