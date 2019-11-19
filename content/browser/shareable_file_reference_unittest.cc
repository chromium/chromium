// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/shareable_file_reference.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

using storage::ShareableFileReference;

namespace content {

TEST(ShareableFileReferenceTest, TestReferences) {
  base::test::SingleThreadTaskEnvironment task_environment;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::ThreadTaskRunnerHandle::Get();
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Create a file.
  base::FilePath file;
  base::CreateTemporaryFileInDir(temp_dir.GetPath(), &file);
  EXPECT_TRUE(base::PathExists(file));

  // Create a first reference to that file.
  scoped_refptr<ShareableFileReference> reference1;
  reference1 = ShareableFileReference::Get(file);
  EXPECT_FALSE(reference1.get());
  reference1 = ShareableFileReference::GetOrCreate(
      file, ShareableFileReference::DELETE_ON_FINAL_RELEASE, task_runner.get());
  EXPECT_TRUE(reference1.get());
  EXPECT_TRUE(file == reference1->path());

  // Get a second reference to that file.
  scoped_refptr<ShareableFileReference> reference2;
  reference2 = ShareableFileReference::Get(file);
  EXPECT_EQ(reference1.get(), reference2.get());
  reference2 = ShareableFileReference::GetOrCreate(
      file, ShareableFileReference::DELETE_ON_FINAL_RELEASE, task_runner.get());
  EXPECT_EQ(reference1.get(), reference2.get());

  // Drop the first reference, the file and reference should still be there.
  reference1 = nullptr;
  EXPECT_TRUE(ShareableFileReference::Get(file).get());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(base::PathExists(file));

  // Drop the second reference, the file and reference should get deleted.
  reference2 = nullptr;
  EXPECT_FALSE(ShareableFileReference::Get(file).get());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(base::PathExists(file));

  // TODO(michaeln): add a test for files that aren't deletable behavior.
}

}  // namespace content
