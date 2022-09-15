// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "storage/browser/blob/scoped_file.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/browser/file_system/transient_file_util.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

namespace storage {

class TransientFileUtilTest : public testing::Test {
 public:
  TransientFileUtilTest() = default;

  TransientFileUtilTest(const TransientFileUtilTest&) = delete;
  TransientFileUtilTest& operator=(const TransientFileUtilTest&) = delete;

  ~TransientFileUtilTest() override = default;

  void SetUp() override {
    file_system_context_ = CreateFileSystemContextForTesting(
        /*quota_manager_proxy=*/nullptr,
        base::FilePath(FILE_PATH_LITERAL("dummy")));
    transient_file_util_ = std::make_unique<TransientFileUtil>();

    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
  }

  void TearDown() override {
    file_system_context_ = nullptr;
    base::RunLoop().RunUntilIdle();
  }

  void CreateAndRegisterTemporaryFile(
      FileSystemURL* file_url,
      base::FilePath* file_path,
      IsolatedContext::ScopedFSHandle* filesystem) {
    EXPECT_TRUE(base::CreateTemporaryFileInDir(data_dir_.GetPath(), file_path));
    IsolatedContext* isolated_context = IsolatedContext::GetInstance();
    std::string name = "tmp";
    *filesystem = isolated_context->RegisterFileSystemForPath(
        kFileSystemTypeForTransientFile, std::string(), *file_path, &name);
    ASSERT_TRUE(filesystem->is_valid());
    base::FilePath virtual_path =
        isolated_context->CreateVirtualRootPath(filesystem->id())
            .AppendASCII(name);
    *file_url = file_system_context_->CreateCrackedFileSystemURL(
        blink::StorageKey::CreateFromStringForTesting("http://foo"),
        kFileSystemTypeIsolated, virtual_path);
  }

  std::unique_ptr<FileSystemOperationContext> NewOperationContext() {
    return std::make_unique<FileSystemOperationContext>(
        file_system_context_.get());
  }

  FileSystemFileUtil* file_util() { return transient_file_util_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir data_dir_;
  scoped_refptr<FileSystemContext> file_system_context_;
  std::unique_ptr<TransientFileUtil> transient_file_util_;
};

TEST_F(TransientFileUtilTest, TransientFile) {
  FileSystemURL temp_url;
  base::FilePath temp_path;
  IsolatedContext::ScopedFSHandle filesystem;

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
    ScopedFile scoped_file = file_util()->CreateSnapshotFile(
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

}  // namespace storage
