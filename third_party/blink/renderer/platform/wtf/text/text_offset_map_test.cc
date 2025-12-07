// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/text_offset_map.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/case_map.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

TEST(TextOffsetMapTest, MergeConstructor) {
  using Entry = TextOffsetMap::Entry;
  struct {
    wtf_size_t length1;
    Vector<Entry> map12;
    wtf_size_t length2;
    Vector<Entry> map23;
    wtf_size_t length3;
    Vector<Entry> expected;
  } kTestData[] = {
      {3, {}, 3, {}, 3, {}},
      {3, {{1, 2}}, 4, {}, 4, {{1, 2}}},
      {7, {{1, 2}, {3, 3}, {5, 4}}, 6, {}, 6, {{1, 2}, {3, 3}, {5, 4}}},
      {3, {}, 3, {{1, 2}}, 4, {{1, 2}}},
      {7, {}, 7, {{1, 2}, {3, 3}, {5, 4}}, 6, {{1, 2}, {3, 3}, {5, 4}}},

      // "abc" -> "aabc" -> "aaabc"
      {3, {{1, 2}}, 4, {{2, 3}}, 5, {{1, 3}}},
      // "abc" -> "aabc" -> "abc"
      {3, {{1, 2}}, 4, {{2, 1}}, 3, {}},
      // "abcde" -> "aabbcdee" -> "aabbcddee"
      {5,
       {{1, 2}, {2, 4}, {4, 7}},
       8,
       {{5, 6}},
       9,
       {{1, 2}, {2, 4}, {3, 6}, {4, 8}}},
      // "abcde" -> "abde" -> "aabdde"
      {5, {{3, 2}}, 4, {{1, 2}, {3, 5}}, 6, {{1, 2}, {3, 3}, {4, 5}}},

      // crbug.com/1520775
      // "ABabCDcdE" -> "ABbCDdE" -> "ABCDE"
      {9, {{3, 2}, {7, 5}}, 7, {{3, 2}, {6, 4}}, 5, {{4, 2}, {8, 4}}},
      // "ABC" -> "AaBCc" -> "AbaBCdc"
      {3, {{1, 2}, {3, 5}}, 5, {{1, 2}, {4, 6}}, 7, {{1, 3}, {3, 7}}},

      // crbug.com/379254069
      // "A" -> "Aab" -> "Ab"
      {1, {{1, 3}}, 3, {{2, 1}}, 2, {{1, 2}}},

      // crbug.com/396666438
      // The first mapping produce two code points from one, and the latter in
      // them and the next code point are paired as a graphmeme cluster.
      // "ABcD" -> "ABbcD" -> "ABbD"
      {4, {{2, 3}}, 5, {{4, 3}}, 4, {{3, 2}, {3, 3}}},
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

    TextOffsetMap merged(data.length1, map12, data.length2, map23,
                         data.length3);
    EXPECT_EQ(merged.Entries(), data.expected);
  }
}

TEST(TextOffsetMapTest, CreateLengthMap) {
  const struct TestData {
    const char* locale;
    const char16_t* source;
    const char16_t* expected_string;
    const Vector<TextOffsetMap::Length> expected_map;
  } kTestData[] = {
      {"", u"", u"", {}},
      {"", u"z", u"Z", {}},
      {"lt", u"i\u0307i\u0307", u"II", {2, 2}},
      {"lt", u"zi\u0307zzi\u0307z", u"ZIZZIZ", {1, 2, 1, 1, 2, 1}},
      {"lt", u"i\u0307\u00DFi\u0307", u"ISSI", {2, 1, 0, 2}},
      {"lt", u"\u00DF\u00DF", u"SSSS", {1, 0, 1, 0}},
      {"lt", u"z\u00DFzzz\u00DFz", u"ZSSZZZSSZ", {1, 1, 0, 1, 1, 1, 1, 0, 1}},
      {"lt", u"\u00DFi\u0307\u00DF", u"SSISS", {1, 0, 2, 1, 0}},
  };

  for (const auto& data : kTestData) {
    SCOPED_TRACE(data.source);
    TextOffsetMap offset_map;
    String source = String(data.source);
    String transformed =
        CaseMap(AtomicString(data.locale)).ToUpper(source, &offset_map);
    EXPECT_EQ(String(data.expected_string), transformed);
    EXPECT_EQ(data.expected_map, offset_map.CreateLengthMap(
                                     source.length(), transformed.length()));
  }
}

// crbug.com/1519398
TEST(TextOffsetMapTest, CreateLengthMapCombiningMark) {
  TextOffsetMap offset_map;
  // Unlike text-transform property, -webki-text-security property can shrink a
  // long grapheme cluster.
  offset_map.Append(1000u, 1u);
  Vector<TextOffsetMap::Length> length_map =
      offset_map.CreateLengthMap(1000u, 1u);
  EXPECT_EQ(1u, length_map.size());
  EXPECT_EQ(1000u, length_map[0]);
}

}  // namespace blink
