// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/code_cache/dedicated_task_runner_for_resource.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class DedicatedTaskRunnerForResourceTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(DedicatedTaskRunnerForResourceTest, Basic) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path = temp_dir.GetPath().AppendASCII("test_path");

  // Acquire a runner.
  auto handle1 = DedicatedTaskRunnerForResource::Acquire(
      {base::TaskPriority::USER_BLOCKING, base::MayBlock()}, path);
  EXPECT_FALSE(handle1.is_null());
  EXPECT_TRUE(handle1.task_runner());

  // Acquire again with same path -> should be same runner.
  auto handle2 = DedicatedTaskRunnerForResource::Acquire(
      {base::TaskPriority::USER_BLOCKING, base::MayBlock()}, path);
  EXPECT_EQ(handle1.task_runner(), handle2.task_runner());

  // Acquire with different path -> should be different runner.
  base::FilePath path2 = temp_dir.GetPath().AppendASCII("test_path2");
  auto handle3 = DedicatedTaskRunnerForResource::Acquire(
      {base::TaskPriority::USER_BLOCKING, base::MayBlock()}, path2);
  EXPECT_NE(handle1.task_runner(), handle3.task_runner());
}

TEST_F(DedicatedTaskRunnerForResourceTest, Lifetime) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path = temp_dir.GetPath().AppendASCII("test_path");

  scoped_refptr<base::SingleThreadTaskRunner> raw_runner;
  {
    auto handle = DedicatedTaskRunnerForResource::Acquire(
        {base::TaskPriority::USER_BLOCKING, base::MayBlock()}, path);
    raw_runner = handle.task_runner();
    EXPECT_TRUE(raw_runner);
  }  // handle goes out of scope, releases the runner.

  // Acquire again -> should create a new runner (different pointer).
  auto handle2 = DedicatedTaskRunnerForResource::Acquire(
      {base::TaskPriority::USER_BLOCKING, base::MayBlock()}, path);
  EXPECT_NE(raw_runner, handle2.task_runner());
}

TEST_F(DedicatedTaskRunnerForResourceTest, MoveSemantics) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path = temp_dir.GetPath().AppendASCII("test_path");

  auto handle1 = DedicatedTaskRunnerForResource::Acquire(
      {base::TaskPriority::USER_BLOCKING, base::MayBlock()}, path);
  auto runner1 = handle1.task_runner();

  // Move constructor.
  DedicatedTaskRunnerForResource handle2(std::move(handle1));
  EXPECT_TRUE(handle1.is_null());
  EXPECT_EQ(runner1, handle2.task_runner());

  // Move assignment.
  DedicatedTaskRunnerForResource handle3;
  handle3 = std::move(handle2);
  EXPECT_TRUE(handle2.is_null());
  EXPECT_EQ(runner1, handle3.task_runner());
}

}  // namespace content
