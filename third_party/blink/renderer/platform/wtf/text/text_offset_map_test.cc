// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/text_offset_map.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {

TEST(TextOffsetMapTest, MergeConstructor) {
  using Entry = TextOffsetMap::Entry;
  struct {
    Vector<Entry> map12;
    Vector<Entry> map23;
    Vector<Entry> expected;
  } kTestData[] = {
      {{}, {}, {}},
      {{{1, 2}}, {}, {{1, 2}}},
      {{{1, 2}, {3, 3}, {5, 4}}, {}, {{1, 2}, {3, 3}, {5, 4}}},
      {{}, {{1, 2}}, {{1, 2}}},
      {{}, {{1, 2}, {3, 3}, {5, 4}}, {{1, 2}, {3, 3}, {5, 4}}},

      // "abc" -> "aabc" -> "aaabc"
      {{{1, 2}}, {{2, 3}}, {{1, 3}}},
      // "abc" -> "aabc" -> "abc"
      {{{1, 2}}, {{2, 1}}, {{1, 1}}},
      // "abcde" -> "aabbcdee" -> "aabbcddee"
      {{{1, 2}, {2, 4}, {4, 7}}, {{5, 6}}, {{1, 2}, {2, 4}, {3, 6}, {4, 8}}},
      // "abcde" -> "abde" -> "aabdde"
      {{{3, 2}}, {{1, 2}, {3, 5}}, {{1, 2}, {3, 3}, {4, 5}}},

      // crbug.com/1520775
      // "ABabCDcdE" -> "ABbCDdE" -> "ABCDE"
      {{{3, 2}, {7, 5}}, {{3, 2}, {6, 4}}, {{4, 2}, {8, 4}}},
      // "ABC" -> "AaBCc" -> "AbaBCdc"
      {{{1, 2}, {3, 5}}, {{1, 2}, {4, 6}}, {{1, 3}, {3, 7}}},
  };

  for (const auto& data : kTestData) {
    SCOPED_TRACE(testing::Message() << data.map12 << " " << data.map23);
    TextOffsetMap map12;
    for (const auto& entry : data.map12) {
      map12.Append(entry.source, entry.target);
    }
    TextOffsetMap map23;
    for (const auto& entry : data.map23) {
      map23.Append(entry.source, entry.target);
    }

    TextOffsetMap merged(map12, map23);
    EXPECT_EQ(merged.Entries(), data.expected);
  }
}

}  // namespace WTF
