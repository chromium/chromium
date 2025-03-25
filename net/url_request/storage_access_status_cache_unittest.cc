// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/storage_access_status_cache.h"

#include "net/cookies/cookie_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(StorageAccessStatusTest, DefaultConstructor) {
  StorageAccessStatusCache status;
  EXPECT_FALSE(status.IsSet());
}

TEST(StorageAccessStatusTest, ConstructorWithValue) {
  StorageAccessStatusCache status(
      net::cookie_util::StorageAccessStatus::kActive);
  ASSERT_TRUE(status.IsSet());
  EXPECT_EQ(status.GetStatusForThirdPartyContext(),
            std::make_optional(net::cookie_util::StorageAccessStatus::kActive));
}

TEST(StorageAccessStatusTest, ConstructorWithNullopt) {
  StorageAccessStatusCache status(std::nullopt);
  ASSERT_TRUE(status.IsSet());
  EXPECT_EQ(status.GetStatusForThirdPartyContext(), std::nullopt);
}

TEST(StorageAccessStatusTest, EqualityOperatorWithValue) {
  StorageAccessStatusCache status(cookie_util::StorageAccessStatus::kActive);
  EXPECT_EQ(status, cookie_util::StorageAccessStatus::kActive);
  EXPECT_NE(status, cookie_util::StorageAccessStatus::kNone);
}

TEST(StorageAccessStatusTest, EqualityOperatorWithoutValue) {
  StorageAccessStatusCache status;
  EXPECT_DEATH_IF_SUPPORTED(
      EXPECT_NE(status, cookie_util::StorageAccessStatus::kNone),
      "Check failed");
}

TEST(StorageAccessStatusTest, GetStatusForThirdPartyContext_Unset) {
  StorageAccessStatusCache status;
  EXPECT_DEATH_IF_SUPPORTED(status.GetStatusForThirdPartyContext(),
                            "Check failed");
}

}  // namespace

}  // namespace net
