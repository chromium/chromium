// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/third_party/divsufsort/divsufsort.h"

#include <algorithm>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

#include "courgette/third_party/bsdiff/bsdiff_search.h"
#include "courgette/third_party/bsdiff/paged_array.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(DivSufSortTest, Sort) {
  const char* test_strs[] = {
    "",
    "a",
    "za",
    "CACAO",
    "banana",
    "tobeornottobe",
    "The quick brown fox jumps over the lazy dog.",
    "elephantelephantelephantelephantelephant",
    "-------------------------",
    "011010011001011010010110011010010",
    "3141592653589793238462643383279502884197169399375105",
    "\xFF\xFE\xFF\xFE\xFD\x80\x30\x31\x32\x80\x30\xFF\x01\xAB\xCD",
  };

  for (const std::string& test_str : test_strs) {
    int len = static_cast<int>(test_str.length());
    const unsigned char* buf =
        reinterpret_cast<const unsigned char*>(test_str.data());

    // Generate the suffix array as I.
    courgette::PagedArray<divsuf::saidx_t> I;
    ASSERT_TRUE(I.Allocate(len + 1));
    divsuf::divsufsort_include_empty(buf, I.begin(), len);

    // Expect that I[] is a permutation of [0, len].
    std::vector<divsuf::saidx_t> I_sorted(I.begin(), I.end());
    std::sort(I_sorted.begin(), I_sorted.end());
    for (divsuf::saidx_t i = 0; i < len + 1; ++i)
      EXPECT_EQ(i, I_sorted[i]);

    // First string must be empty string.
    EXPECT_EQ(len, I[0]);

    // Expect that the |len + 1| suffixes are strictly ordered.
    const unsigned char* end = buf + len;
    for (divsuf::saidx_t i = 1; i <= len; ++i) {
      const unsigned char* suf1 = buf + I[i - 1];
      const unsigned char* suf2 = buf + I[i];
      bool is_less = std::lexicographical_compare(suf1, end, suf2, end);
      EXPECT_TRUE(is_less);
    }
  }
}

// Test with sequence that has every character.
TEST(DivSufSortTest, AllChar) {
  const int kNumChar = 256;
  std::vector<unsigned char> all_char(kNumChar);
  std::iota(all_char.begin(), all_char.end(), 0);

  {
    courgette::PagedArray<divsuf::saidx_t> I;
    ASSERT_TRUE(I.Allocate(kNumChar + 1));
    divsuf::divsufsort_include_empty(&all_char[0], I.begin(), kNumChar);
    EXPECT_EQ(kNumChar, I[0]);  // Empty character.
    for (int i = 1; i <= kNumChar; ++i)
      EXPECT_EQ(i - 1, I[i]);
  }

  std::vector<unsigned char> all_char_reverse(
      all_char.rbegin(), all_char.rend());
  {
    courgette::PagedArray<divsuf::saidx_t> I;
    ASSERT_TRUE(I.Allocate(kNumChar + 1));
    divsuf::divsufsort_include_empty(&all_char_reverse[0], I.begin(), kNumChar);
    for (int i = 0; i <= kNumChar; ++i)
      EXPECT_EQ(kNumChar - i, I[i]);
  }
}
