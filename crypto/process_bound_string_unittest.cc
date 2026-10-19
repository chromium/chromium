// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/process_bound_string.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crypto {

namespace {

MATCHER_P(ContainsSubsequence, subsequence, "contains subsequence") {
  return std::search(arg.begin(), arg.end(), subsequence.begin(),
                     subsequence.end()) != arg.end();
}

}  // namespace

// Test fixture template
template <typename T>
class ProcessBoundTest : public ::testing::Test {};

// Define the types you want to test
typedef ::testing::Types<std::string, std::wstring, std::u16string> TestTypes;

// Register the test fixture for these types
TYPED_TEST_SUITE(ProcessBoundTest, TestTypes);

TYPED_TEST(ProcessBoundTest, DefaultConstructed) {
  crypto::ProcessBound<TypeParam> str;
  EXPECT_TRUE(str.empty());
  EXPECT_EQ(0u, str.size());
  EXPECT_EQ(TypeParam(), str.value());
  EXPECT_TRUE(str.secure_value().empty());
}

TYPED_TEST(ProcessBoundTest, TestCases) {
  const size_t test_cases[] = {0, 1, 15, 16, 17};

  for (const auto test_length : test_cases) {
    TypeParam test_value;
    for (size_t i = 0; i < test_length; i++) {
      test_value.append(
          1, static_cast<typename TypeParam::value_type>('0' + (i % 10)));
    }
    crypto::ProcessBound<TypeParam> str(test_value);
    EXPECT_EQ(test_value, str.value());
  }
}

TEST(ProcessBound, Copy) {
  crypto::ProcessBound<std::string> str(std::string("hello"));

  auto str2 = str;

  EXPECT_EQ(str2.value(), str.value());
  EXPECT_EQ(str.value(), "hello");
}

TEST(ProcessBound, Move) {
  crypto::ProcessBound<std::string> str(std::string("hello"));

  // Move constructor
  auto str2 = std::move(str);

  EXPECT_EQ(str2.value(), "hello");
  EXPECT_EQ(str2.size(), 5u);
  EXPECT_FALSE(str2.empty());

  // Intentional use-after-move to validate moved-from state matches default
  // constructor.
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_TRUE(str.empty());
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_EQ(str.size(), 0u);
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_EQ(str.value(), "");
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_TRUE(str.secure_value().empty());

  // Move assignment
  crypto::ProcessBound<std::string> str3(std::string("world"));
  str3 = std::move(str2);

  EXPECT_EQ(str3.value(), "hello");
  EXPECT_EQ(str3.size(), 5u);
  EXPECT_FALSE(str3.empty());

  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_TRUE(str2.empty());
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_EQ(str2.size(), 0u);
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_EQ(str2.value(), "");
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_TRUE(str2.secure_value().empty());

  // Indirect through a pointer so this is not optimized away as a self-move.
  crypto::ProcessBound<std::string>* alias = &str3;
  str3 = std::move(*alias);

  EXPECT_EQ(str3.value(), "hello");
  EXPECT_EQ(str3.size(), 5u);
  EXPECT_FALSE(str3.empty());
}

using ProcessBoundEncryptionTest = ::testing::Test;

// Only Windows supports real encryption at the moment. On other platforms, the
// underlying decrypted buffer is returned and since it was never decrypted,
// Short String Optimization means that the custom allocator is never used for
// the test string, meaning it never gets cleared. Which is fine, since it was
// never encrypted anyway.
#if BUILDFLAG(IS_WIN)

// Reading into freed memory upsets sanitizers.
#if !defined(ADDRESS_SANITIZER) && !defined(THREAD_SANITIZER) && \
    !defined(MEMORY_SANITIZER)
TEST_F(ProcessBoundEncryptionTest, Encryption) {
  [[maybe_unused]] const char* data;
  {
    crypto::ProcessBound<std::string> process_bound(std::string("hello"));
    crypto::SecureString secure = process_bound.secure_value();
    constexpr std::array<uint8_t, 5> kPlainText = {'h', 'e', 'l', 'l', 'o'};
    EXPECT_THAT(process_bound.maybe_encrypted_data_,
                ::testing::Not(ContainsSubsequence(kPlainText)));
    EXPECT_STREQ(secure.c_str(), "hello");
    data = secure.data();
    ASSERT_NE(data, nullptr);
  }
// In debug builds, frees are poisoned after the SecureString allocator has
// zeroed it, so this check can only take place for release builds.
#if defined(NDEBUG)
  EXPECT_EQ(data[0], '\x00');
#endif  // defined(NDEBUG)
}
#endif  // !defined(ADDRESS_SANITIZER) && !defined(THREAD_SANITIZER) &&
        // !defined(MEMORY_SANITIZER)

TEST_F(ProcessBoundEncryptionTest, EmptyString) {
  crypto::ProcessBound<std::string> process_bound("");
  crypto::SecureString secure = process_bound.secure_value();
  EXPECT_TRUE(process_bound.maybe_encrypted_data_.empty());
  EXPECT_TRUE(secure.empty());
  const char* data = secure.data();
  // Empty string is "\0", not nullptr.
  ASSERT_NE(data, nullptr);
  // Due to the Small String Optimization (SSO) empty strings may be allocated
  // on the stack, and not go through SecureAllocator::deallocate. That's ok
  // because the entire content was already zero.
  EXPECT_EQ(data[0], '\x00');
}

#endif  // #if BUILDFLAG(IS_WIN)

}  // namespace crypto
