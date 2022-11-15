// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/base64.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace WTF {

TEST(Base64Test, DecodeNoPaddingValidation) {
  struct {
    const char* in;
    Vector<char> expected_out;
  } kTestCases[] = {
      // Inputs that are multiples of 4 always succeed.
      {"abcd", {'i', '\xB7', '\x1D'}},
      {"abc=", {'i', '\xB7'}},
      {"abcdefgh", {'i', '\xB7', '\x1D', 'y', '\xF8', '!'}},

      // Lack of proper padding (to a multiple of 4) always succeeds if
      // len % 4 != 1.
      {"abcdef", {'i', '\xB7', '\x1D', 'y'}},
      {"abc", {'i', '\xB7'}},
      {"ab", {'i'}},

      // Invalid padding is ignored if kNoPaddingValidation is set.
      {"abcd=", {'i', '\xB7', '\x1D'}},
      {"abcd==", {'i', '\xB7', '\x1D'}},
      {"abcd===", {'i', '\xB7', '\x1D'}},
      {"abcd==============", {'i', '\xB7', '\x1D'}},

      // Whitespace should not be allow if kNoPaddingValidation is set.
      {" a bcd", {}},
      {"ab\t\tc=", {}},
      {"ab c\ndefgh ", {}},

      // Failures that should apply in all decoding modes.
      {"abc&", {}},
      {"abcde", {}},
      {"a", {}},

      // Empty string should yield an empty result.
      {"", {}},
  };

  for (const auto& test : kTestCases) {
    Vector<char> out;
    bool success = Base64Decode(String(test.in), out);
    if (success) {
      EXPECT_EQ(test.expected_out, out) << test.in;
    } else {
      EXPECT_TRUE(test.expected_out.empty()) << test.in;
    }
  }
}

TEST(Base64Test, ForgivingBase64Decode) {
  struct {
    const char* in;
    Vector<char> expected_out;
  } kTestCases[] = {
      // Inputs that are multiples of 4 always succeed.
      {"abcd", {'i', '\xB7', '\x1D'}},
      {"abc=", {'i', '\xB7'}},
      {"abcdefgh", {'i', '\xB7', '\x1D', 'y', '\xF8', '!'}},

      // Lack of proper padding (to a multiple of 4) always succeeds if
      // len % 4 != 1.
      {"abcdef", {'i', '\xB7', '\x1D', 'y'}},
      {"abc", {'i', '\xB7'}},
      {"ab", {'i'}},

      // Invalid padding causes failure if kForgiving is set.
      {"abcd=", {}},
      {"abcd==", {}},
      {"abcd===", {}},
      {"abcd==============", {}},

      // Whitespace should be allow if kForgiving is set.
      {" a bcd", {'i', '\xB7', '\x1D'}},
      {"ab\t\tc=", {'i', '\xB7'}},
      {"ab c\ndefgh ", {'i', '\xB7', '\x1D', 'y', '\xF8', '!'}},

      // Failures that should apply in all decoding modes.
      {"abc&", {}},
      {"abcde", {}},
      {"a", {}},

      // Empty string should yield an empty result.
      {"", {}},
  };

  for (const auto& test : kTestCases) {
    Vector<char> out;
    bool success =
        Base64Decode(String(test.in), out, Base64DecodePolicy::kForgiving);
    if (success) {
      EXPECT_EQ(test.expected_out, out) << test.in;
    } else {
      EXPECT_TRUE(test.expected_out.empty()) << test.in;
    }
  }
}

}  // namespace WTF
