// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/buckets/bucket_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(BucketUtilsTest, IsValidBucketName) {
  struct {
    std::string bucket_name;
    bool expected_validity;
  } test_cases[] = {
      {"", false},
      {"StringThatIsLongerThan64Characters1234567890123456789012345678901",
       false},
      {"-ShouldNotStartWithDash", false},
      {"_ShouldNotStartWithUnderscore", false},
      {"ShouldNotContainNonAsciiCharacters!", false},
      {"ShouldNotContainUpperCases", false},
      {"should-be-valid-123", true},
  };

  for (auto& testCase : test_cases) {
    EXPECT_EQ(IsValidBucketName(testCase.bucket_name),
              testCase.expected_validity);
  }
}
}  // namespace content
