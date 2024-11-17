// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/process_bound_string.h"

#include <algorithm>
#include <string>

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

  auto str2 = std::move(str);

  EXPECT_EQ(str2.value(), "hello");
}

class ProcessBoundFeatureTest : public ::testing::Test,
                                public ::testing::WithParamInterface<
                                    /*kProcessBoundStringEncryption*/ bool> {
 public:
  ProcessBoundFeatureTest() {
    scoped_feature_list_.InitWithFeatureState(
        crypto::features::kProcessBoundStringEncryption, GetParam());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Only Windows supports real encryption at the moment. On other platforms, the
// underlying decrypted buffer is returned and since it was never decrypted,
// Short String Optimization means that the custom allocator is never used for
// the test string, meaning it never gets cleared. Which is fine, since it was
// never encrypted anyway.
#if BUILDFLAG(IS_WIN)
// Reading into freed memory upsets sanitizers.
#if !defined(ADDRESS_SANITIZER) && !defined(THREAD_SANITIZER) && \
    !defined(MEMORY_SANITIZER)
TEST_P(ProcessBoundFeatureTest, Encryption) {
  [[maybe_unused]] const char* data;
  {
    crypto::ProcessBound<std::string> process_bound(std::string("hello"));
    crypto::SecureString secure = process_bound.secure_value();
    constexpr std::array<uint8_t, 5> kPlainText = {'h', 'e', 'l', 'l', 'o'};
    if (GetParam()) {
      EXPECT_THAT(process_bound.maybe_encrypted_data_,
                  ::testing::Not(ContainsSubsequence(kPlainText)));
    } else {
      EXPECT_THAT(process_bound.maybe_encrypted_data_,
                  ContainsSubsequence(kPlainText));
    }
    EXPECT_STREQ(secure.c_str(), "hello");
    data = secure.data();
  }
// In debug builds, frees are poisoned after the SecureString allocator has
// zeroed it, so this check can only take place for release builds.
#if defined(NDEBUG)
  if (GetParam()) {
    EXPECT_EQ(data[0], '\x00');
  }
#endif  // defined(NDEBUG)
}
#endif  // !defined(ADDRESS_SANITIZER) && !defined(THREAD_SANITIZER) &&
        // !defined(MEMORY_SANITIZER)
#endif  // #if BUILDFLAG(IS_WIN)

// On all other platforms, make sure the basic functionality works when
// encryption is both enabled and disabled.
TEST_P(ProcessBoundFeatureTest, Basic) {
  crypto::ProcessBound<std::string> process_bound(std::string("hello"));
  EXPECT_STREQ("hello", process_bound.value().c_str());
}

INSTANTIATE_TEST_SUITE_P(,
                         ProcessBoundFeatureTest,
                         testing::Bool(),
                         [](auto& info) {
                           return info.param ? "Enabled" : "Disabled";
                         });

}  // namespace crypto
