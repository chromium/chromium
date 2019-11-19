// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "storage/browser/blob/scoped_file.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/browser/file_system/transient_file_util.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"

using storage::FileSystemURL;

namespace content {

class TransientFileUtilTest : public testing::Test {
 public:
  TransientFileUtilTest() = default;
  ~TransientFileUtilTest() override = default;

  void SetUp() override {
    file_system_context_ = CreateFileSystemContextForTesting(
        nullptr, base::FilePath(FILE_PATH_LITERAL("dummy")));
    transient_file_util_.reset(new storage::TransientFileUtil);

    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
  }

  void TearDown() override {
    file_system_context_ = nullptr;
    base::RunLoop().RunUntilIdle();
  }

  void CreateAndRegisterTemporaryFile(
      FileSystemURL* file_url,
      base::FilePath* file_path,
      storage::IsolatedContext::ScopedFSHandle* filesystem) {
    EXPECT_TRUE(base::CreateTemporaryFileInDir(data_dir_.GetPath(), file_path));
    storage::IsolatedContext* isolated_context =
        storage::IsolatedContext::GetInstance();
    std::string name = "tmp";
    *filesystem = isolated_context->RegisterFileSystemForPath(
        storage::kFileSystemTypeForTransientFile, std::string(), *file_path,
        &name);
    ASSERT_TRUE(filesystem->is_valid());
    base::FilePath virtual_path =
        isolated_context->CreateVirtualRootPath(filesystem->id())
            .AppendASCII(name);
    *file_url = file_system_context_->CreateCrackedFileSystemURL(
        GURL("http://foo"), storage::kFileSystemTypeIsolated, virtual_path);
  }

  std::unique_ptr<storage::FileSystemOperationContext> NewOperationContext() {
    return std::make_unique<storage::FileSystemOperationContext>(
        file_system_context_.get());
  }

  storage::FileSystemFileUtil* file_util() {
    return transient_file_util_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir data_dir_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  std::unique_ptr<storage::TransientFileUtil> transient_file_util_;

  DISALLOW_COPY_AND_ASSIGN(TransientFileUtilTest);
};

TEST_F(TransientFileUtilTest, TransientFile) {
  FileSystemURL temp_url;
  base::FilePath temp_path;
  storage::IsolatedContext::ScopedFSHandle filesystem;

  CreateAndRegisterTemporaryFile(&temp_url, &temp_path, &filesystem);

  base::File::Error error;
  base::File::Info file_info;
  base::FilePath path;

  // Make sure the file is there.
  ASSERT_TRUE(temp_url.is_valid());
  ASSERT_TRUE(base::PathExists(temp_path));
  ASSERT_FALSE(base::DirectoryExists(temp_path));

  // Create a snapshot file.
  {
    storage::ScopedFile scoped_file = file_util()->CreateSnapshotFile(
        NewOperationContext().get(), temp_url, &error, &file_info, &path);
    ASSERT_EQ(base::File::FILE_OK, error);
    ASSERT_EQ(temp_path, path);
    ASSERT_FALSE(file_info.is_directory);

    // The file should be still there.
    ASSERT_TRUE(base::PathExists(temp_path));
    ASSERT_EQ(base::File::FILE_OK,
              file_util()->GetFileInfo(NewOperationContext().get(), temp_url,
                                       &file_info, &path));
    ASSERT_EQ(temp_path, path);
    ASSERT_FALSE(file_info.is_directory);
  }

  // The file's now scoped out.
  base::RunLoop().RunUntilIdle();

  // Now the temporary file and the transient filesystem must be gone too.
  ASSERT_FALSE(base::PathExists(temp_path));
  ASSERT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            file_util()->GetFileInfo(NewOperationContext().get(), temp_url,
                                     &file_info, &path));
}

}  // namespace content
