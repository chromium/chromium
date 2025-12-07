// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/storage_access_status_cache.h"

#include "base/test/gtest_util.h"
#include "net/cookies/cookie_util.h"
#include "testing/gtest/include/gtest/gtest-death-test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(StorageAccessStatusCacheTest, DefaultConstructor) {
  StorageAccessStatusCache status;
  EXPECT_FALSE(status.IsSet());
}

TEST(StorageAccessStatusCacheTest, ConstructorWithValue) {
  StorageAccessStatusCache status(
      net::cookie_util::StorageAccessStatus::kActive);
  ASSERT_TRUE(status.IsSet());
  EXPECT_EQ(status.GetStatusForThirdPartyContext(),
            std::make_optional(net::cookie_util::StorageAccessStatus::kActive));
}

TEST(StorageAccessStatusCacheTest, ConstructorWithNullopt) {
  StorageAccessStatusCache status(std::nullopt);
  ASSERT_TRUE(status.IsSet());
  EXPECT_EQ(status.GetStatusForThirdPartyContext(), std::nullopt);
}

TEST(StorageAccessStatusCacheTest, EqualityOperatorWithValue) {
  StorageAccessStatusCache status(cookie_util::StorageAccessStatus::kActive);
  EXPECT_EQ(status, cookie_util::StorageAccessStatus::kActive);
  EXPECT_NE(status, cookie_util::StorageAccessStatus::kNone);
}

#ifdef GTEST_HAS_DEATH_TEST
TEST(StorageAccessStatusCacheTest, EqualityOperatorWithoutValue) {
  StorageAccessStatusCache status;
  EXPECT_CHECK_DEATH(
      EXPECT_NE(status, cookie_util::StorageAccessStatus::kNone));
}
#endif  // GTEST_HAS_DEATH_TEST

#ifdef GTEST_HAS_DEATH_TEST
TEST(StorageAccessStatusCacheTest, GetStatusForThirdPartyContext_Unset) {
  StorageAccessStatusCache status;
  EXPECT_CHECK_DEATH(status.GetStatusForThirdPartyContext());
}
#endif  // GTEST_HAS_DEATH_TEST

TEST(StorageAccessStatusCacheTest, Reset) {
  StorageAccessStatusCache status(
      net::cookie_util::StorageAccessStatus::kActive);
  EXPECT_TRUE(status.IsSet());
  status.Reset();
  EXPECT_FALSE(status.IsSet());
}

#ifdef GTEST_HAS_DEATH_TEST
TEST(StorageAccessStatusCacheTest, GetStatusForThirdPartyContext_AfterReset) {
  StorageAccessStatusCache status(
      net::cookie_util::StorageAccessStatus::kActive);
  status.Reset();
  EXPECT_CHECK_DEATH(status.GetStatusForThirdPartyContext());
}
#endif  // GTEST_HAS_DEATH_TEST

}  // namespace

}  // namespace net
