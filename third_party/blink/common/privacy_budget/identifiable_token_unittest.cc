// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"

#include <string_view>

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

// The set of candidate conversion templates depend on whether the conversion is
// explicit or implicit. This class is used to exercise implicit conversion of
// IdIdentifiableApiSample.
struct ImplicitConverter {
  // NOLINTNEXTLINE(google-explicit-constructor)
  ImplicitConverter(IdentifiableToken sample) : sample(sample) {}

  IdentifiableToken sample;
};

}  // namespace

TEST(IdentifiableTokenTest, SampleBool) {
  bool source_value = false;
  auto expected_value = INT64_C(0);
  EXPECT_EQ(IdentifiableToken(expected_value), IdentifiableToken(source_value));
  EXPECT_EQ(IdentifiableToken(expected_value),
            ImplicitConverter(source_value).sample);
}

TEST(IdentifiableTokenTest, SampleSignedChar) {
  auto source_value = static_cast<signed char>(-65);
  auto expected_value = INT64_C(-65);
  EXPECT_EQ(IdentifiableToken(expected_value), IdentifiableToken(source_value));
  EXPECT_EQ(IdentifiableToken(expected_value),
            ImplicitConverter(source_value).sample);
}

TEST(IdentifiableTokenTest, SampleChar) {
  auto source_value = 'A';
  auto expected_value = INT64_C(65);
  EXPECT_EQ(IdentifiableToken(expected_value), IdentifiableToken(source_value));
  EXPECT_EQ(IdentifiableToken(expected_value),
            ImplicitConverter(source_value).sample);
}

TEST(IdentifiableTokenTest, SampleInt) {
  auto source_value = 123;
  auto expected_value = INT64_C(123);
  EXPECT_EQ(IdentifiableToken(expected_value), IdentifiableToken(source_value));
  EXPECT_EQ(IdentifiableToken(expected_value),
            ImplicitConverter(source_value).sample);
}

TEST(IdentifiableTokenTest, SampleNegativeInt) {
  auto source_value = -123;
  auto expected_value = INT64_C(-123);
  EXPECT_EQ(IdentifiableToken(expected_value), IdentifiableToken(source_value));
  EXPECT_EQ(IdentifiableToken(expected_value),
            ImplicitConverter(source_value).sample);
}

TEST(IdentifiableTokenTest, SampleUnsigned) {
  auto source_value = UINT64_C(123);
  auto expected_value = INT64_C(123);
  EXPECT_EQ(IdentifiableToken(expected_value), IdentifiableToken(source_value));
  EXPECT_EQ(IdentifiableToken(expected_value),
            ImplicitConverter(source_value).sample);
}

TEST(IdentifiableTokenTest, SampleBigUnsignedThatFits) {
  auto source_value =
      static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 1;
  auto expected_value = std::numeric_limits<int64_t>::min();
  EXPECT_EQ(IdentifiableToken(expected_value), IdentifiableToken(source_value));
  EXPECT_EQ(IdentifiableToken(expected_value),
            ImplicitConverter(source_value).sample);
}

TEST(IdentifiableTokenTest, SampleFloat) {
  auto source_value = 5.1f;
  auto expected_value = INT64_C(0x4014666660000000);
  EXPECT_EQ(IdentifiableToken(expected_value), IdentifiableToken(source_value));
  EXPECT_EQ(IdentifiableToken(expected_value),
            ImplicitConverter(source_value).sample);
}

TEST(IdentifiableTokenTest, SampleConstCharArray) {
  EXPECT_EQ(IdentifiableToken(INT64_C(0xf75a3b8a1499428d)),
            IdentifiableToken("abcd"));
  // No implicit converter for const char[].
}

TEST(IdentifiableTokenTest, SampleStdString) {
  EXPECT_EQ(IdentifiableToken(INT64_C(0xf75a3b8a1499428d)),
            IdentifiableToken(std::string("abcd")));
  // No implicit converter for std::string.
}

TEST(IdentifiableTokenTest, SampleStringPiece) {
  auto source_value = std::string_view("abcd");
  auto expected_value = INT64_C(0xf75a3b8a1499428d);
  EXPECT_EQ(IdentifiableToken(expected_value), IdentifiableToken(source_value));
  // No implicit converter for StringPiece.
}

TEST(IdentifiableTokenTest, SampleCharSpan) {
  auto source_value = base::make_span("abcd", 4u);
  auto expected_value = INT64_C(0xf75a3b8a1499428d);
  EXPECT_EQ(IdentifiableToken(expected_value), IdentifiableToken(source_value));
  EXPECT_EQ(IdentifiableToken(expected_value),
            ImplicitConverter(source_value).sample);
}

TEST(IdentifiableTokenTest, SampleStringSpan) {
  std::string strings[] = {"baby", "shark", "du duu du duu du du"};
  auto source_value = base::make_span(strings);
  auto expected_value = INT64_C(0xd37aad882e58faa5);
  EXPECT_EQ(IdentifiableToken(expected_value), IdentifiableToken(source_value));
  EXPECT_EQ(IdentifiableToken(expected_value),
            ImplicitConverter(source_value).sample);
}

TEST(IdentifiableTokenTest, SampleTuple) {
  EXPECT_EQ(IdentifiableToken(INT64_C(0x5848123245be627a)),
            IdentifiableToken(1, 2, 3, 4, 5));
  // No implicit converter for tuples.
}

TEST(IdentifiableTokenTest, SampleHeterogenousTuple) {
  EXPECT_EQ(IdentifiableToken(INT64_C(0x672cf4c107b5b22)),
            IdentifiableToken(1, 2, 3.0, 4, 'a'));
  // No implicit converter for tuples.
}

}  // namespace blink
