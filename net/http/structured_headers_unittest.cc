// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/structured_headers.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::structured_headers {

namespace {

using ::testing::ElementsAre;
using ::testing::Pair;

TEST(StructuredHeadersLenientTest, TrailingDecimal) {
  // RFC 8941 requires at least one digit after the decimal point.
  // Legacy Quiche allows trailing dots (e.g. "1.") if followed by EOF.
  // Rust backend replicates this in lenient mode.
  const std::string_view kInput = "1.";

  EXPECT_EQ(ParseItemStrict(kInput), std::nullopt);

  std::optional<ParameterizedItem> result = ParseItem(kInput);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->item.is_decimal());
  EXPECT_DOUBLE_EQ(result->item.GetDecimal(), 1.0);
}

TEST(StructuredHeadersLenientTest, ByteSequenceWhitespace) {
  // Legacy Quiche/Abseil skips ASCII whitespace in byte sequences,
  // provided it doesn't interfere with the 4-byte block boundary checks
  // in its synthesized padding logic.
  const std::string_view kInput1 = ": Zm9v   :";

  EXPECT_EQ(ParseItemStrict(kInput1), std::nullopt);

  std::optional<ParameterizedItem> result = ParseItem(kInput1);
  ASSERT_TRUE(result.has_value());
  const std::string* value = result->item.GetIfByteSequence();
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, "foo");

  // Replicate Abseil's ascii_isspace behavior, which includes vertical tab
  // (0x0b). The raw length between colons must be a multiple of 4 so that
  // Quiche doesn't synthesize too much padding (e.g. 8 chars results in no
  // added padding).
  const std::string_view kInput2 = ":mmmM\ncm\x0b:";

  EXPECT_EQ(ParseItemStrict(kInput2), std::nullopt);

  std::optional<ParameterizedItem> result2 = ParseItem(kInput2);
  ASSERT_TRUE(result2.has_value());
  EXPECT_TRUE(result2->item.is_byte_sequence());
}

TEST(StructuredHeadersLenientTest, ByteSequenceDotAsPadding) {
  // Abseil allows '.' as an alias for '=' in base64.
  // ":Zm9." is 5 chars (including colons), so internal string is "Zm9.".
  // Legacy Quiche pads to "Zm9.". Standard SH would require ":Zm9=:".
  const std::string_view kInput = ":Zm9.:";

  EXPECT_EQ(ParseItemStrict(kInput), std::nullopt);

  std::optional<ParameterizedItem> result = ParseItem(kInput);
  ASSERT_TRUE(result.has_value());
  const std::string* value = result->item.GetIfByteSequence();
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, "fo");
}

TEST(StructuredHeadersLenientTest, ByteSequencePartialPadding) {
  // Partial padding (e.g. 1 '=' when 2 are expected for a 4-byte block) is
  // rejected in strict mode, but accepted in lenient mode by both backends.
  const std::string_view kInput = ":Zg=:";

  EXPECT_EQ(ParseItemStrict(kInput), std::nullopt);

  std::optional<ParameterizedItem> result = ParseItem(kInput);
  ASSERT_TRUE(result.has_value());
  const std::string* value = result->item.GetIfByteSequence();
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, "f");
}

TEST(StructuredHeadersLenientTest, DuplicateParameterKeysPreserveOrder) {
  // Duplicate parameter keys must overwrite the existing value in-place
  // while preserving the original insertion order of the keys.
  std::optional<ParameterizedItem> item =
      ParseItemStrict("token;a=1;b=2;a=3;c=4;b=5");
  ASSERT_TRUE(item.has_value());
  EXPECT_THAT(item->params, ElementsAre(Pair("a", Item(int64_t{3})),
                                        Pair("b", Item(int64_t{5})),
                                        Pair("c", Item(int64_t{4}))));

  // Also test with >16 distinct keys so the small-buffer-to-HashMap transition
  // preserves order and overwrites earlier and later keys in-place.
  std::optional<ParameterizedItem> large_item = ParseItemStrict(
      "token;k0=0;k1=1;k2=2;k3=3;k4=4;k5=5;k6=6;k7=7;k8=8;k9=9;"
      "k10=10;k11=11;k12=12;k13=13;k14=14;k15=15;k16=16;k17=17;"
      "k0=100;k17=117");
  ASSERT_TRUE(large_item.has_value());
  ASSERT_EQ(large_item->params.size(), 18u);
  EXPECT_EQ(large_item->params[0], std::pair("k0", Item(int64_t{100})));
  EXPECT_EQ(large_item->params[17], std::pair("k17", Item(int64_t{117})));

  std::optional<List> list = ParseListStrict("(1 2);x=1;y=2;x=3");
  ASSERT_TRUE(list.has_value());
  ASSERT_EQ(list->size(), 1u);
  const InnerList* inner_list = (*list)[0].GetIfInnerList();
  ASSERT_TRUE(inner_list);
  EXPECT_THAT(inner_list->params, ElementsAre(Pair("x", Item(int64_t{3})),
                                              Pair("y", Item(int64_t{2}))));
}

TEST(StructuredHeadersLenientTest, DuplicateDictionaryKeysPreserveOrder) {
  // Duplicate dictionary keys must overwrite the previous member (including
  // changing between InnerList and Item and clearing prior parameters) while
  // preserving the original insertion order of the keys.
  std::optional<Dictionary> dict =
      ParseDictionaryStrict("a=(1 2);p=1, b=2;q=3, a=3, b=(4)");
  ASSERT_TRUE(dict.has_value());
  EXPECT_THAT(
      *dict, ElementsAre(Pair("a", ParameterizedMember(Item(int64_t{3}))),
                         Pair("b", ParameterizedMember(std::vector{
                                       ParameterizedItem(Item(int64_t{4}))}))));
}

}  // namespace

}  // namespace net::structured_headers
