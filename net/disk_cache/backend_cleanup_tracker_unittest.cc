// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/backend_cleanup_tracker.h"

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace disk_cache {
namespace {

using testing::UnorderedElementsAre;
using testing::IsEmpty;

class BackendCleanupTrackerTest : public net::TestWithTaskEnvironment {
 protected:
  BackendCleanupTrackerTest() = default;

  void SetUp() override {
    testing::Test::SetUp();

    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());
    // Create two unique paths.
    path1_ = tmp_dir_.GetPath().Append(FILE_PATH_LITERAL("a"));
    path2_ = tmp_dir_.GetPath().Append(FILE_PATH_LITERAL("b"));
  }

  void RecordCall(int val) { called_.push_back(val); }

  base::OnceClosure RecordCallClosure(int val) {
    return base::BindOnce(&BackendCleanupTrackerTest::RecordCall,
                          base::Unretained(this), val);
  }

  base::ScopedTempDir tmp_dir_;
  base::FilePath path1_;
  base::FilePath path2_;
  std::vector<int> called_;
};

TEST_F(BackendCleanupTrackerTest, DistinctPath) {
  scoped_refptr<BackendCleanupTracker> t1 =
      BackendCleanupTracker::TryCreate(path1_, RecordCallClosure(1));
  scoped_refptr<BackendCleanupTracker> t2 =
      BackendCleanupTracker::TryCreate(path2_, RecordCallClosure(2));
  // Both should be created immediately (since the paths are distinct), none of
  // the callbacks should be invoked.
  ASSERT_TRUE(t1 != nullptr);
  ASSERT_TRUE(t2 != nullptr);
  RunUntilIdle();
  EXPECT_TRUE(called_.empty());

  t1->AddPostCleanupCallback(RecordCallClosure(3));
  t2->AddPostCleanupCallback(RecordCallClosure(4));
  t2->AddPostCleanupCallback(RecordCallClosure(5));

  // Just adding callbacks doesn't run them, nor just an event loop.
  EXPECT_TRUE(called_.empty());
  RunUntilIdle();
  EXPECT_TRUE(called_.empty());

  t1 = nullptr;
  // Callbacks are not invoked immediately.
  EXPECT_TRUE(called_.empty());

  // ... but via the event loop.
  RunUntilIdle();
  EXPECT_THAT(called_, UnorderedElementsAre(3));

  // Now cleanup t2.
  t2 = nullptr;
  EXPECT_THAT(called_, UnorderedElementsAre(3));
  RunUntilIdle();
  EXPECT_THAT(called_, UnorderedElementsAre(3, 4, 5));
}

TEST_F(BackendCleanupTrackerTest, SamePath) {
  scoped_refptr<BackendCleanupTracker> t1 =
      BackendCleanupTracker::TryCreate(path1_, RecordCallClosure(1));
  scoped_refptr<BackendCleanupTracker> t2 =
      BackendCleanupTracker::TryCreate(path1_, RecordCallClosure(2));
  // Since path is the same, only first call succeeds. No callback yet,
  // since t1 controls the path.
  ASSERT_TRUE(t1 != nullptr);
  EXPECT_TRUE(t2 == nullptr);
  RunUntilIdle();
  EXPECT_TRUE(called_.empty());

  t1->AddPostCleanupCallback(RecordCallClosure(3));
  t1->AddPostCleanupCallback(RecordCallClosure(4));

  // Create an alias denoting work in progress.
  scoped_refptr<BackendCleanupTracker> alias = t1;
  t1 = nullptr;

  EXPECT_TRUE(called_.empty());
  RunUntilIdle();
  EXPECT_TRUE(called_.empty());

  alias = nullptr;
  EXPECT_TRUE(called_.empty());
  RunUntilIdle();
  // Both the callback passed to the TryCreate that failed and ones passed to
  // AddPostCleanupCallback are called.
  EXPECT_THAT(called_, UnorderedElementsAre(2, 3, 4));
}

}  // namespace
}  // namespace disk_cache
