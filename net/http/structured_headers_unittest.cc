// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/structured_headers.h"

#include <optional>
#include <string_view>

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace net::structured_headers {

class StructuredHeadersLenientTest : public testing::TestWithParam<bool> {
 public:
  StructuredHeadersLenientTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(kStructuredHeadersInRust);
    } else {
      feature_list_.InitAndDisableFeature(kStructuredHeadersInRust);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         StructuredHeadersLenientTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "Rust" : "Cpp";
                         });

TEST_P(StructuredHeadersLenientTest, TrailingDecimal) {
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

TEST_P(StructuredHeadersLenientTest, ByteSequenceWhitespace) {
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

TEST_P(StructuredHeadersLenientTest, ByteSequenceDotAsPadding) {
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

void ParseItemParity(const std::string_view input, const bool strict) {
  std::optional<ParameterizedItem> cpp_result;
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(kStructuredHeadersInRust);
    cpp_result = strict ? ParseItemStrict(input) : ParseItem(input);
  }

  std::optional<ParameterizedItem> rust_result;
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(kStructuredHeadersInRust);
    rust_result = strict ? ParseItemStrict(input) : ParseItem(input);
  }

  ASSERT_EQ(cpp_result.has_value(), rust_result.has_value())
      << "ParseItem discrepancy for input: " << input;
  if (cpp_result.has_value()) {
    EXPECT_EQ(*cpp_result, *rust_result)
        << "ParseItem value mismatch for input: " << input
        << "\nCPP:  " << *net::structured_headers::SerializeItem(*cpp_result)
        << "\nRust: " << *net::structured_headers::SerializeItem(*rust_result);
  }
}

FUZZ_TEST(StructuredHeadersTest, ParseItemParity)
    .WithDomains(fuzztest::Arbitrary<std::string_view>().WithSeeds({
                     R"(1)",                  // Integer
                     R"(1.0)",                // Decimal
                     R"(token;abc=123;def)",  // Parameterized Token (implicit
                                              // boolean param)
                     R"("string")",           // String
                     R"(:aGVsbG8=:)",         // Byte Sequence
                     R"(?1)",                 // Boolean
                 }),
                 fuzztest::Arbitrary<bool>());

void ParseListParity(const std::string_view input, const bool strict) {
  std::optional<List> cpp_result;
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(kStructuredHeadersInRust);
    cpp_result = strict ? ParseListStrict(input) : ParseList(input);
  }

  std::optional<List> rust_result;
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(kStructuredHeadersInRust);
    rust_result = strict ? ParseListStrict(input) : ParseList(input);
  }

  ASSERT_EQ(cpp_result.has_value(), rust_result.has_value())
      << "ParseList discrepancy for input: " << input;
  if (cpp_result.has_value()) {
    EXPECT_EQ(*cpp_result, *rust_result)
        << "ParseList value mismatch for input: " << input
        << "\nCPP:  " << *net::structured_headers::SerializeList(*cpp_result)
        << "\nRust: " << *net::structured_headers::SerializeList(*rust_result);
  }
}

FUZZ_TEST(StructuredHeadersTest, ParseListParity)
    .WithDomains(fuzztest::Arbitrary<std::string_view>().WithSeeds({
                     R"(foo)",            // Single item
                     R"(())",             // Empty inner list
                     R"((foo))",          // Inner list with one item
                     R"((foo bar))",      // Inner list with multiple items
                     R"(foo;a=1)",        // Single item with parameter
                     R"((foo bar);a=1)",  // Inner list with parameter
                 }),
                 fuzztest::Arbitrary<bool>());

void ParseDictionaryParity(const std::string_view input, const bool strict) {
  std::optional<Dictionary> cpp_result;
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(kStructuredHeadersInRust);
    cpp_result = strict ? ParseDictionaryStrict(input) : ParseDictionary(input);
  }

  std::optional<Dictionary> rust_result;
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(kStructuredHeadersInRust);
    rust_result =
        strict ? ParseDictionaryStrict(input) : ParseDictionary(input);
  }

  ASSERT_EQ(cpp_result.has_value(), rust_result.has_value())
      << "ParseDictionary discrepancy for input: " << input;
  if (cpp_result.has_value()) {
    EXPECT_EQ(*cpp_result, *rust_result)
        << "ParseDictionary value mismatch for input: " << input << "\nCPP:  "
        << *net::structured_headers::SerializeDictionary(*cpp_result)
        << "\nRust: "
        << *net::structured_headers::SerializeDictionary(*rust_result);
  }
}

FUZZ_TEST(StructuredHeadersTest, ParseDictionaryParity)
    .WithDomains(fuzztest::Arbitrary<std::string_view>().WithSeeds({
                     R"(en="Applepie", da=:w4ZibGV0w6ZydGUK:)",  //
                     R"(a=1)",                                   //
                     R"(a=(1 2))",                               //
                     R"(a=1, b=2)",                              //
                     R"(a=1;b=2, c=3;d=4)",                      //
                     R"(a)",  // Implicit boolean value
                 }),
                 fuzztest::Arbitrary<bool>());

}  // namespace net::structured_headers
