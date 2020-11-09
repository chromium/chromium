// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_database_owner.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

TEST(TrustTokenDatabaseOwner, Initializes) {
  base::test::TaskEnvironment env;
  std::unique_ptr<TrustTokenDatabaseOwner> owner;
  TrustTokenDatabaseOwner::Create(
      /*db_opener=*/base::BindOnce([](sql::Database* db) {
        CHECK(db->OpenInMemory());
        return true;
      }),
      base::ThreadTaskRunnerHandle::Get(),
      /*flush_delay_for_writes=*/base::TimeDelta(),
      /*on_done_initializing=*/
      base::BindLambdaForTesting(
          [&owner](std::unique_ptr<TrustTokenDatabaseOwner> created) {
            owner = std::move(created);
            base::RunLoop().Quit();
          }));
  env.RunUntilIdle();
  ASSERT_TRUE(owner);
  EXPECT_TRUE(owner->IssuerData());
  EXPECT_TRUE(owner->IssuerToplevelPairData());
  EXPECT_TRUE(owner->ToplevelData());

  owner.reset();
  // Wait until TrustTokenDatabaseOwner finishes closing its database
  // asynchronously, so as not to leak after the test concludes.
  env.RunUntilIdle();
}

TEST(TrustTokenDatabaseOwner, StillInitializesOnDbOpenFailure) {
  base::test::TaskEnvironment env;

  // The database opener callback returning failure should still lead to a
  // usable state (albeit not one that writes through to a database).
  std::unique_ptr<TrustTokenDatabaseOwner> owner;
  TrustTokenDatabaseOwner::Create(
      /*db_opener=*/base::BindOnce([](sql::Database* unused) { return false; }),
      base::ThreadTaskRunnerHandle::Get(),
      /*flush_delay_for_writes=*/base::TimeDelta(),
      /*on_done_initializing=*/
      base::BindLambdaForTesting(
          [&owner](std::unique_ptr<TrustTokenDatabaseOwner> created) {
            owner = std::move(created);
            base::RunLoop().Quit();
          }));
  env.RunUntilIdle();

  ASSERT_TRUE(owner);
  EXPECT_TRUE(owner->IssuerData());
  EXPECT_TRUE(owner->IssuerToplevelPairData());
  EXPECT_TRUE(owner->ToplevelData());

  owner.reset();
  // Wait until TrustTokenDatabaseOwner finishes closing its database
  // asynchronously, so as not to leak after the test concludes.
  env.RunUntilIdle();
}

}  // namespace network
