// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/wtf/text/base64.h"

#include <optional>

#include "base/containers/span.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace WTF {

TEST(Base64Test, Encode) {
  struct {
    const char* in;
    Vector<char> expected_out;
  } kTestCases[] = {{"", {}},
                    {"i", {'a', 'Q', '=', '='}},
                    {"i\xB7", {'a', 'b', 'c', '='}},
                    {"i\xB7\x1D", {'a', 'b', 'c', 'd'}}};

  for (const auto& test : kTestCases) {
    base::span<const uint8_t> in =
        base::as_bytes(base::make_span(test.in, strlen(test.in)));

    Vector<char> out_vec;
    Base64Encode(in, out_vec);
    EXPECT_EQ(out_vec, test.expected_out);

    String out_str = Base64Encode(in);
    EXPECT_EQ(out_str,
              String(test.expected_out.data(), test.expected_out.size()));
  }
}

TEST(Base64Test, DecodeNoPaddingValidation) {
  struct {
    const char* in;
    const char* expected_out;
  } kTestCases[] = {
      // Inputs that are multiples of 4 always succeed.
      {"abcd", "i\xB7\x1D"},
      {"abc=", "i\xB7"},
      {"abcdefgh", "i\xB7\x1Dy\xF8!"},

      // Lack of proper padding (to a multiple of 4) always succeeds if
      // len % 4 != 1.
      {"abcdef", "i\xB7\x1Dy"},
      {"abc", "i\xB7"},
      {"ab", "i"},

      // Invalid padding is ignored if kNoPaddingValidation is set.
      {"abcd=", "i\xB7\x1D"},
      {"abcd==", "i\xB7\x1D"},
      {"abcd===", "i\xB7\x1D"},
      {"abcd==============", "i\xB7\x1D"},
      {"=", ""},

      // Whitespace should not be allowed if kNoPaddingValidation is set.
      {" a bcd", nullptr},
      {"ab\t\tc=", nullptr},
      {"ab c\ndefgh ", nullptr},

      // Failures that should apply in all decoding modes.
      {"abc&", nullptr},
      {"abcde", nullptr},
      {"a", nullptr},

      // Empty string should yield an empty result.
      {"", ""},
  };

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(::testing::Message() << test.in);
    Vector<char> out;
    String in = String(test.in);
    bool expected_success = test.expected_out != nullptr;
    Vector<char> expected_out;
    if (expected_success) {
      expected_out.insert(0, test.expected_out, strlen(test.expected_out));
    }

    bool success_8bit = Base64Decode(in, out);
    EXPECT_EQ(expected_success, success_8bit);
    if (expected_success) {
      EXPECT_EQ(expected_out, out);
    }
    out.clear();
    in.Ensure16Bit();
    bool success_16bit = Base64Decode(in, out);
    EXPECT_EQ(expected_success, success_16bit);
    if (expected_success) {
      EXPECT_EQ(expected_out, out);
    }
  }
}

TEST(Base64Test, ForgivingBase64Decode) {
  struct {
    const char* in;
    const char* expected_out;
  } kTestCases[] = {
      // Inputs that are multiples of 4 always succeed.
      {"abcd", "i\xB7\x1D"},
      {"abc=", "i\xB7"},
      {"abcdefgh", "i\xB7\x1Dy\xF8!"},

      // Lack of proper padding (to a multiple of 4) always succeeds if
      // len % 4 != 1.
      {"abcdef", "i\xB7\x1Dy"},
      {"abc", "i\xB7"},
      {"ab", "i"},

      // Invalid padding causes failure if kForgiving is set.
      {"abcd=", nullptr},
      {"abcd==", nullptr},
      {"abcd===", nullptr},
      {"abcd==============", nullptr},
      {"=", nullptr},

      // Whitespace should be allow if kForgiving is set.
      {" a bcd", "i\xB7\x1D"},
      {"ab\t\tc=", "i\xB7"},
      {"ab c\ndefgh", "i\xB7\x1Dy\xF8!"},

      // Failures that should apply in all decoding modes.
      {"abc&", nullptr},
      {"abcde", nullptr},
      {"a", nullptr},

      // Empty string should yield an empty result.
      {"", ""},
  };

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(::testing::Message() << test.in);
    Vector<char> out;
    String in = String(test.in);
    bool expected_success = test.expected_out != nullptr;
    Vector<char> expected_out;
    if (expected_success) {
      expected_out.insert(0, test.expected_out, strlen(test.expected_out));
    }

    bool success_8bit = Base64Decode(in, out, Base64DecodePolicy::kForgiving);
    EXPECT_EQ(expected_success, success_8bit);
    if (expected_success) {
      EXPECT_EQ(expected_out, out);
    }
    out.clear();
    in.Ensure16Bit();
    bool success_16bit = Base64Decode(in, out, Base64DecodePolicy::kForgiving);
    EXPECT_EQ(expected_success, success_16bit);
    if (expected_success) {
      EXPECT_EQ(expected_out, out);
    }
  }
}

}  // namespace WTF
