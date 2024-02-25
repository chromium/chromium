// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/transformed_string.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/case_map.h"
#include "third_party/blink/renderer/platform/wtf/text/text_offset_map.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

TEST(TransformedStringTest, CreateLengthMap) {
  const struct TestData {
    const char* locale;
    const char16_t* source;
    const char16_t* expected_string;
    const Vector<unsigned> expected_map;
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
    EXPECT_EQ(data.expected_map,
              TransformedString::CreateLengthMap(
                  source.length(), transformed.length(), offset_map));
  }
}

// crbug.com/1519398
TEST(TransformedStringTest, CreateLengthMapCombiningMark) {
  TextOffsetMap offset_map;
  // Unlike text-transform property, -webki-text-security property can shrink a
  // long grapheme cluster.
  offset_map.Append(1000u, 1u);
  Vector<unsigned> length_map =
      TransformedString::CreateLengthMap(1000u, 1u, offset_map);
  EXPECT_EQ(1u, length_map.size());
  EXPECT_EQ(1000u, length_map[0]);
}

}  // namespace blink
