// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/base64.h"

#include "base/base64.h"
#include "base/strings/escape.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "testing/gtest/include/gtest/gtest.h"

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

      // Invalid padding causes failure if kForgiving is set.
      {"abcd=", nullptr, nullptr},
      {"abcd==", nullptr, nullptr},
      {"abcd===", nullptr, nullptr},
      {"abcd====", nullptr, nullptr},
      {"abcd==============", nullptr, nullptr},
      {"abcde===", nullptr, nullptr},
      {"=", nullptr, nullptr},
      {"====", nullptr, nullptr},

      // Otherwise, inputs that are multiples of 4 always succeed, this matches
      // kStrict mode.
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
    std::string output;
    bool success = SimdutfBase64Decode(test_case.in, &output,
                                       base::Base64DecodePolicy::kForgiving);
    bool expected_success = test_case.expected_out_forgiving != nullptr;
    EXPECT_EQ(success, expected_success);
    if (expected_success) {
      EXPECT_EQ(output, test_case.expected_out_forgiving);
    }
  }
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message()
                 << "Strict: "
                 << base::EscapeAllExceptUnreserved(test_case.in));
    std::string output;
    bool success = SimdutfBase64Decode(test_case.in, &output,
                                       base::Base64DecodePolicy::kStrict);
    bool expected_success = test_case.expected_out_strict != nullptr;
    EXPECT_EQ(success, expected_success);
    if (expected_success) {
      EXPECT_EQ(output, test_case.expected_out_strict);
    }
  }
}

}  // namespace net
