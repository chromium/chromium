// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/bigint.h"

#include <array>
#include <limits>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "v8/include/v8.h"

namespace blink {

TEST(BigIntTest, ToUInt64_Zero) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::Local<v8::BigInt> v8_bigint = v8::BigInt::New(scope.GetIsolate(), 0);
  NonThrowableExceptionState exception_state;
  blink::BigInt bigint =
      ToBigInt(scope.GetIsolate(), v8_bigint, exception_state);
  std::optional<uint64_t> result = bigint.ToUInt64();
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(*result, 0u);
}

TEST(BigIntTest, ToUInt64_Negative) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::Local<v8::BigInt> v8_bigint = v8::BigInt::New(scope.GetIsolate(), -1);
  NonThrowableExceptionState exception_state;
  blink::BigInt bigint =
      ToBigInt(scope.GetIsolate(), v8_bigint, exception_state);
  std::optional<uint64_t> result = bigint.ToUInt64();
  EXPECT_FALSE(result.has_value());
}

TEST(BigIntTest, ToUInt64_Max) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::Local<v8::BigInt> v8_bigint = v8::BigInt::NewFromUnsigned(
      scope.GetIsolate(), std::numeric_limits<uint64_t>::max());
  NonThrowableExceptionState exception_state;
  blink::BigInt bigint =
      ToBigInt(scope.GetIsolate(), v8_bigint, exception_state);
  std::optional<uint64_t> result = bigint.ToUInt64();
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(*result, std::numeric_limits<uint64_t>::max());
}

TEST(BigIntTest, ToUint64_LargerThanMax) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  NonThrowableExceptionState exception_state;
  // This value represents std::numeric_limits<uint64_t>::max() + 1;
  std::array<uint64_t, 2> words = {0, 1};
  v8::Local<v8::BigInt> v8_bigint =
      v8::BigInt::NewFromWords(scope.GetContext(), /*sign_bit=*/0, words.size(),
                               words.data())
          .ToLocalChecked();
  blink::BigInt bigint =
      ToBigInt(scope.GetIsolate(), v8_bigint, exception_state);
  std::optional<uint64_t> result = bigint.ToUInt64();
  EXPECT_FALSE(result.has_value());
}

TEST(BigIntTest, ToInt64_Zero) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::Local<v8::BigInt> v8_bigint = v8::BigInt::New(scope.GetIsolate(), 0);
  NonThrowableExceptionState exception_state;
  blink::BigInt bigint =
      ToBigInt(scope.GetIsolate(), v8_bigint, exception_state);
  std::optional<int64_t> result = bigint.ToInt64();
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(*result, 0u);
}

TEST(BigIntTest, ToInt64_Max) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::Local<v8::BigInt> v8_bigint =
      v8::BigInt::New(scope.GetIsolate(), std::numeric_limits<int64_t>::max());
  NonThrowableExceptionState exception_state;
  blink::BigInt bigint =
      ToBigInt(scope.GetIsolate(), v8_bigint, exception_state);
  std::optional<int64_t> result = bigint.ToInt64();
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(*result, std::numeric_limits<int64_t>::max());
}

TEST(BigIntTest, ToInt64_LargerThanMax) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  // This value represents std::numeric_limits<int64_t>::max() + 1;
  std::array<uint64_t, 1> words = {0x8000000000000000ull};
  v8::Local<v8::BigInt> v8_bigint =
      v8::BigInt::NewFromWords(scope.GetContext(), /*sign_bit=*/0, words.size(),
                               words.data())
          .ToLocalChecked();
  NonThrowableExceptionState exception_state;
  blink::BigInt bigint =
      ToBigInt(scope.GetIsolate(), v8_bigint, exception_state);
  std::optional<int64_t> result = bigint.ToInt64();
  EXPECT_FALSE(result.has_value());
}

TEST(BigIntTest, ToInt64_Min) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::Local<v8::BigInt> v8_bigint =
      v8::BigInt::New(scope.GetIsolate(), std::numeric_limits<int64_t>::min());
  NonThrowableExceptionState exception_state;
  blink::BigInt bigint =
      ToBigInt(scope.GetIsolate(), v8_bigint, exception_state);
  std::optional<int64_t> result = bigint.ToInt64();
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(*result, std::numeric_limits<int64_t>::min());
}

TEST(BigIntTest, ToInt64_LessThanMin) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  // Together with `sign_bit`, this value represents
  // std::numeric_limits<int64_t>::min() - 1;
  std::array<uint64_t, 1> words = {0x8000000000000001ull};
  v8::Local<v8::BigInt> v8_bigint =
      v8::BigInt::NewFromWords(scope.GetContext(), /*sign_bit=*/1, words.size(),
                               words.data())
          .ToLocalChecked();
  NonThrowableExceptionState exception_state;
  blink::BigInt bigint =
      ToBigInt(scope.GetIsolate(), v8_bigint, exception_state);
  std::optional<int64_t> result = bigint.ToInt64();
  EXPECT_FALSE(result.has_value());
}

}  // namespace blink
