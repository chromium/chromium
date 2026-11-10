// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/base64.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <numeric>
#include <string>
#include <string_view>

#include "base/base64.h"
#include "base/strings/escape.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/simdutf/simdutf.h"

namespace net {

class Base64Test : public testing::Test {
 protected:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kSimdutfBase64Support);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(Base64Test, Basic) {
  const std::string kText = "hello world";
  const std::string kBase64Text = "aGVsbG8gd29ybGQ=";

  std::string decoded;
  bool ok = SimdutfBase64Decode(kBase64Text, &decoded);
  EXPECT_TRUE(ok);
  EXPECT_EQ(decoded, kText);
}

TEST_F(Base64Test, InPlace) {
  const std::string kText = "hello world";
  const std::string kBase64Text = "aGVsbG8gd29ybGQ=";

  std::string text = kBase64Text;

  bool ok = SimdutfBase64Decode(text, &text);
  EXPECT_TRUE(ok);
  EXPECT_EQ(text, kText);
}

TEST_F(Base64Test, ForgivingAndStrictDecode) {
  struct {
    const char* in;

    // nullptr indicates a decode failure.
    const char* expected_out_forgiving;
    const char* expected_out_strict;
  } kTestCases[] = {
      // Failures that should apply in all decoding modes:
      //
      // - Characters not in the base64 alphabet
      {"abc&", nullptr, nullptr},
      {"ab-d", nullptr, nullptr},
      // - input len % 4 == 1
      {"abcde", nullptr, nullptr},
      {"a", nullptr, nullptr},

      // Invalid padding is rejected in both modes.
      {"abcd=", nullptr, nullptr},
      {"abcd==", nullptr, nullptr},
      {"abcd===", nullptr, nullptr},
      {"abcd====", nullptr, nullptr},
      {"abcd==============", nullptr, nullptr},
      {"abcde===", nullptr, nullptr},
      {"=", nullptr, nullptr},
      {"====", nullptr, nullptr},

      // Valid padded input succeeds in both modes.
      {"abcd", "i\xB7\x1D", "i\xB7\x1D"},
      {"abc=", "i\xB7", "i\xB7"},
      {"abcdefgh", "i\xB7\x1Dy\xF8!", "i\xB7\x1Dy\xF8!"},

      // kForgiving mode allows for omitting padding (to a multiple of 4) if
      // len % 4 != 1.
      {"abcdef", "i\xB7\x1Dy", nullptr},
      {"abc", "i\xB7", nullptr},
      {"ab", "i", nullptr},

      // Whitespace should be allowed if kForgiving is set, matching
      // https://infra.spec.whatwg.org/#ascii-whitespace:
      // ASCII whitespace is U+0009 TAB '\t', U+000A LF '\n', U+000C FF '\f',
      // U+000D CR '\r', or U+0020 SPACE ' '.
      {" a bcd", "i\xB7\x1D", nullptr},
      {"ab\t\tc=", "i\xB7", nullptr},
      {"ab c\ndefgh", "i\xB7\x1Dy\xF8!", nullptr},
      {"a\tb\nc\f d\r", "i\xB7\x1D", nullptr},
      {"this should fail", "\xB6\x18\xAC\xB2\x1A.\x95\xD7\xDA\x8A", nullptr},

      // U+000B VT '\v' is _not_ valid whitespace to be stripped.
      {"ab\vcd", nullptr, nullptr},

      // Empty string should yield an empty result.
      {"", "", ""},
  };
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message()
                 << "Forgiving: "
                 << base::EscapeAllExceptUnreserved(test_case.in));
    std::string output = "unchanged";
    bool success = SimdutfBase64Decode(test_case.in, &output,
                                       base::Base64DecodePolicy::kForgiving);
    bool expected_success = test_case.expected_out_forgiving != nullptr;
    EXPECT_EQ(success, expected_success);
    if (expected_success) {
      EXPECT_EQ(output, test_case.expected_out_forgiving);
    } else {
      EXPECT_EQ(output, "unchanged");
    }
  }
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message()
                 << "Strict: "
                 << base::EscapeAllExceptUnreserved(test_case.in));
    std::string output = "unchanged";
    bool success = SimdutfBase64Decode(test_case.in, &output,
                                       base::Base64DecodePolicy::kStrict);
    bool expected_success = test_case.expected_out_strict != nullptr;
    EXPECT_EQ(success, expected_success);
    if (expected_success) {
      EXPECT_EQ(output, test_case.expected_out_strict);
    } else {
      EXPECT_EQ(output, "unchanged");
    }
  }
}

TEST_F(Base64Test, FailurePreservesOutput) {
  const auto inputs = std::to_array<std::string>({
      // Keep a valid prefix long enough for SIMD decoding before the error.
      std::string(256, 'A') + "AAA&",
      std::string(256, 'A') + "A===",
      std::string(256, 'A') + "AA\vA",
      std::string(256, 'A') + "A",
  });
  for (const auto& input : inputs) {
    SCOPED_TRACE(base::EscapeAllExceptUnreserved(input));
    for (auto policy : {base::Base64DecodePolicy::kStrict,
                        base::Base64DecodePolicy::kForgiving}) {
      SCOPED_TRACE(static_cast<int>(policy));
      std::string output = "unchanged";
      EXPECT_FALSE(SimdutfBase64Decode(input, &output, policy));
      EXPECT_EQ(output, "unchanged");

      std::string in_place = input;
      EXPECT_FALSE(SimdutfBase64Decode(in_place, &in_place, policy));
      EXPECT_EQ(in_place, input);
    }
  }
}

TEST_F(Base64Test, BinaryOutputAndAliasedView) {
  constexpr std::string_view kPrefix = "prefix";
  // Exercise empty output, padding, small-string boundaries and larger buffers.
  for (size_t size :
       {0, 1, 2, 15, 16, 22, 23, 24, 31, 32, 33, 255, 256, 257, 4096}) {
    SCOPED_TRACE(size);
    std::string expected(size, '\0');
    std::ranges::iota(expected, uint8_t{0});
    const std::string encoded = base::Base64Encode(expected);
    for (auto policy : {base::Base64DecodePolicy::kStrict,
                        base::Base64DecodePolicy::kForgiving}) {
      SCOPED_TRACE(static_cast<int>(policy));
      std::string output = "unchanged";
      ASSERT_TRUE(SimdutfBase64Decode(encoded, &output, policy));
      EXPECT_EQ(output, expected);
      // std::string::operator[] allows size(), which reads the NUL terminator.
      EXPECT_EQ(output[output.size()], '\0');

      // Keep small encoded inputs inline to exercise self-aliasing with SSO.
      std::string in_place = encoded;
      ASSERT_TRUE(SimdutfBase64Decode(in_place, &in_place, policy));
      EXPECT_EQ(in_place, expected);
      EXPECT_EQ(in_place[in_place.size()], '\0');

      in_place.assign(kPrefix).append(encoded).append("suffix");
      const std::string_view input =
          std::string_view(in_place).substr(kPrefix.size(), encoded.size());
      ASSERT_TRUE(SimdutfBase64Decode(input, &in_place, policy));
      EXPECT_EQ(in_place, expected);
      EXPECT_EQ(in_place[in_place.size()], '\0');
    }

    SCOPED_TRACE("Whitespace-padded input");
    const std::string whitespace_encoded = " " + encoded + "  ";
    // Whitespace makes the buffer estimate larger than the decoded output.
    // This exercises shrinking past an uninitialized tail, including to zero.
    ASSERT_GT(simdutf::maximal_binary_length_from_base64(
                  whitespace_encoded.data(), whitespace_encoded.size()),
              expected.size());
    std::string output = "unchanged";
    ASSERT_TRUE(SimdutfBase64Decode(whitespace_encoded, &output,
                                    base::Base64DecodePolicy::kForgiving));
    EXPECT_EQ(output, expected);
    EXPECT_EQ(output[output.size()], '\0');

    std::string in_place = whitespace_encoded;
    ASSERT_TRUE(SimdutfBase64Decode(in_place, &in_place,
                                    base::Base64DecodePolicy::kForgiving));
    EXPECT_EQ(in_place, expected);
    EXPECT_EQ(in_place[in_place.size()], '\0');
  }
}

}  // namespace net
