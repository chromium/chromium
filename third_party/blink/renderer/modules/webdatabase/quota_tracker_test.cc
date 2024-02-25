// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webdatabase/quota_tracker.h"

#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {
namespace {

TEST(QuotaTrackerTest, UpdateAndGetSizeAndSpaceAvailable) {
  test::TaskEnvironment task_environment;
  QuotaTracker& tracker = QuotaTracker::Instance();
  scoped_refptr<const SecurityOrigin> origin =
      SecurityOrigin::CreateFromString("file:///a/b/c");

  const String database_name = "db";
  const uint64_t kDatabaseSize = 1234ULL;
  tracker.UpdateDatabaseSize(origin.get(), database_name, kDatabaseSize);

  uint64_t used = 0;
  uint64_t available = 0;
  tracker.GetDatabaseSizeAndSpaceAvailableToOrigin(origin.get(), database_name,
                                                   &used, &available);

  EXPECT_EQ(used, kDatabaseSize);
  EXPECT_EQ(available, 0UL);
}

TEST(QuotaTrackerTest, LocalAccessBlocked) {
  test::TaskEnvironment task_environment;
  QuotaTracker& tracker = QuotaTracker::Instance();
  scoped_refptr<SecurityOrigin> origin =
      SecurityOrigin::CreateFromString("file:///a/b/c");

  const String database_name = "db";
  const uint64_t kDatabaseSize = 1234ULL;
  tracker.UpdateDatabaseSize(origin.get(), database_name, kDatabaseSize);

  // QuotaTracker should not care about policy, just identity.
  origin->BlockLocalAccessFromLocalOrigin();

  uint64_t used = 0;
  uint64_t available = 0;
  tracker.GetDatabaseSizeAndSpaceAvailableToOrigin(origin.get(), database_name,
                                                   &used, &available);

  EXPECT_EQ(used, kDatabaseSize);
  EXPECT_EQ(available, 0UL);
}

}  // namespace
}  // namespace blink
