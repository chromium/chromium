// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "storage/browser/file_system/async_file_util_adapter.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_file_util.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/local_file_util.h"
#include "storage/browser/file_system/native_file_util.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::AsyncFileTestHelper;
using storage::AsyncFileUtilAdapter;
using storage::FileSystemContext;
using storage::FileSystemOperationContext;
using storage::FileSystemURL;
using storage::LocalFileUtil;

namespace content {

namespace {

const GURL kOrigin("http://foo/");
const storage::FileSystemType kFileSystemType = storage::kFileSystemTypeTest;

}  // namespace

class LocalFileUtilTest : public testing::Test {
 public:
  LocalFileUtilTest() = default;

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    file_system_context_ =
        CreateFileSystemContextForTesting(nullptr, data_dir_.GetPath());
  }

  void TearDown() override {
    file_system_context_ = nullptr;
    base::RunLoop().RunUntilIdle();
  }

 protected:
  FileSystemOperationContext* NewContext() {
    FileSystemOperationContext* context =
        new FileSystemOperationContext(file_system_context_.get());
    context->set_update_observers(
        *file_system_context_->GetUpdateObservers(kFileSystemType));
    return context;
  }

  LocalFileUtil* file_util() {
    AsyncFileUtilAdapter* adapter = static_cast<AsyncFileUtilAdapter*>(
        file_system_context_->GetAsyncFileUtil(kFileSystemType));
    return static_cast<LocalFileUtil*>(adapter->sync_file_util());
  }

  FileSystemURL CreateURL(const std::string& file_name) {
    return file_system_context_->CreateCrackedFileSystemURL(
        kOrigin, kFileSystemType, base::FilePath().FromUTF8Unsafe(file_name));
  }

  base::FilePath LocalPath(const char* file_name) {
    base::FilePath path;
    std::unique_ptr<FileSystemOperationContext> context(NewContext());
    file_util()->GetLocalFilePath(context.get(), CreateURL(file_name), &path);
    return path;
  }

  bool FileExists(const char* file_name) {
    return base::PathExists(LocalPath(file_name)) &&
           !base::DirectoryExists(LocalPath(file_name));
  }

  bool DirectoryExists(const char* file_name) {
    return base::DirectoryExists(LocalPath(file_name));
  }

  int64_t GetSize(const char* file_name) {
    base::File::Info info;
    base::GetFileInfo(LocalPath(file_name), &info);
    return info.size;
  }

  base::File CreateFile(const char* file_name) {
    int file_flags = base::File::FLAG_CREATE | base::File::FLAG_WRITE |
                     base::File::FLAG_ASYNC;

    std::unique_ptr<FileSystemOperationContext> context(NewContext());
    return file_util()->CreateOrOpen(context.get(), CreateURL(file_name),
                                     file_flags);
  }

  base::File::Error EnsureFileExists(const char* file_name, bool* created) {
    std::unique_ptr<FileSystemOperationContext> context(NewContext());
    return file_util()->EnsureFileExists(context.get(), CreateURL(file_name),
                                         created);
  }

  FileSystemContext* file_system_context() {
    return file_system_context_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<FileSystemContext> file_system_context_;
  base::ScopedTempDir data_dir_;

  DISALLOW_COPY_AND_ASSIGN(LocalFileUtilTest);
};

TEST_F(LocalFileUtilTest, CreateAndClose) {
  const char* file_name = "test_file";
  base::File file = CreateFile(file_name);
  ASSERT_TRUE(file.IsValid());
  ASSERT_TRUE(file.created());

  EXPECT_TRUE(FileExists(file_name));
  EXPECT_EQ(0, GetSize(file_name));

  std::unique_ptr<FileSystemOperationContext> context(NewContext());
}

// base::CreateSymbolicLink is supported on most POSIX, but not on Fuchsia.
#if defined(OS_POSIX)
TEST_F(LocalFileUtilTest, CreateFailForSymlink) {
  // Create symlink target file.
  const char* target_name = "symlink_target";
  base::File target_file = CreateFile(target_name);
  ASSERT_TRUE(target_file.IsValid());
  ASSERT_TRUE(target_file.created());
  base::FilePath target_path = LocalPath(target_name);

  // Create symlink where target must be real file.
  const char* symlink_name = "symlink_file";
  base::FilePath symlink_path = LocalPath(symlink_name);
  ASSERT_TRUE(base::CreateSymbolicLink(target_path, symlink_path));
  ASSERT_TRUE(FileExists(symlink_name));

  // Try to open the symlink file which should fail.
  std::unique_ptr<FileSystemOperationContext> context(NewContext());
  FileSystemURL url = CreateURL(symlink_name);
  int file_flags = base::File::FLAG_OPEN | base::File::FLAG_READ;
  base::File file = file_util()->CreateOrOpen(context.get(), url, file_flags);
  ASSERT_FALSE(file.IsValid());
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, file.error_details());
}
#endif

TEST_F(LocalFileUtilTest, EnsureFileExists) {
  const char* file_name = "foobar";
  bool created;
  ASSERT_EQ(base::File::FILE_OK, EnsureFileExists(file_name, &created));
  ASSERT_TRUE(created);

  EXPECT_TRUE(FileExists(file_name));
  EXPECT_EQ(0, GetSize(file_name));

  ASSERT_EQ(base::File::FILE_OK, EnsureFileExists(file_name, &created));
  EXPECT_FALSE(created);
}

TEST_F(LocalFileUtilTest, TouchFile) {
  const char* file_name = "test_file";
  base::File file = CreateFile(file_name);
  ASSERT_TRUE(file.IsValid());
  ASSERT_TRUE(file.created());

  std::unique_ptr<FileSystemOperationContext> context(NewContext());

  base::File::Info info;
  ASSERT_TRUE(base::GetFileInfo(LocalPath(file_name), &info));
  const base::Time new_accessed =
      info.last_accessed + base::TimeDelta::FromHours(10);
  const base::Time new_modified =
      info.last_modified + base::TimeDelta::FromHours(5);

  EXPECT_EQ(base::File::FILE_OK,
            file_util()->Touch(context.get(), CreateURL(file_name),
                               new_accessed, new_modified));

  ASSERT_TRUE(base::GetFileInfo(LocalPath(file_name), &info));
  EXPECT_EQ(new_accessed, info.last_accessed);
  EXPECT_EQ(new_modified, info.last_modified);
}

TEST_F(LocalFileUtilTest, TouchDirectory) {
  const char* dir_name = "test_dir";
  std::unique_ptr<FileSystemOperationContext> context(NewContext());
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->CreateDirectory(context.get(), CreateURL(dir_name),
                                         false /* exclusive */,
                                         false /* recursive */));

  base::File::Info info;
  ASSERT_TRUE(base::GetFileInfo(LocalPath(dir_name), &info));
  const base::Time new_accessed =
      info.last_accessed + base::TimeDelta::FromHours(10);
  const base::Time new_modified =
      info.last_modified + base::TimeDelta::FromHours(5);

  EXPECT_EQ(base::File::FILE_OK,
            file_util()->Touch(context.get(), CreateURL(dir_name), new_accessed,
                               new_modified));

  ASSERT_TRUE(base::GetFileInfo(LocalPath(dir_name), &info));
  EXPECT_EQ(new_accessed, info.last_accessed);
  EXPECT_EQ(new_modified, info.last_modified);
}

TEST_F(LocalFileUtilTest, Truncate) {
  const char* file_name = "truncated";
  bool created;
  ASSERT_EQ(base::File::FILE_OK, EnsureFileExists(file_name, &created));
  ASSERT_TRUE(created);

  std::unique_ptr<FileSystemOperationContext> context;

  context.reset(NewContext());
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->Truncate(context.get(), CreateURL(file_name), 1020));

  EXPECT_TRUE(FileExists(file_name));
  EXPECT_EQ(1020, GetSize(file_name));
}

TEST_F(LocalFileUtilTest, CopyFile) {
  const char* from_file = "fromfile";
  const char* to_file1 = "tofile1";
  const char* to_file2 = "tofile2";
  bool created;
  ASSERT_EQ(base::File::FILE_OK, EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);

  std::unique_ptr<FileSystemOperationContext> context;
  context.reset(NewContext());
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->Truncate(context.get(), CreateURL(from_file), 1020));

  EXPECT_TRUE(FileExists(from_file));
  EXPECT_EQ(1020, GetSize(from_file));

  ASSERT_EQ(
      base::File::FILE_OK,
      AsyncFileTestHelper::Copy(file_system_context(), CreateURL(from_file),
                                CreateURL(to_file1)));

  context.reset(NewContext());
  ASSERT_EQ(
      base::File::FILE_OK,
      AsyncFileTestHelper::Copy(file_system_context(), CreateURL(from_file),
                                CreateURL(to_file2)));

  EXPECT_TRUE(FileExists(from_file));
  EXPECT_EQ(1020, GetSize(from_file));
  EXPECT_TRUE(FileExists(to_file1));
  EXPECT_EQ(1020, GetSize(to_file1));
  EXPECT_TRUE(FileExists(to_file2));
  EXPECT_EQ(1020, GetSize(to_file2));
}

TEST_F(LocalFileUtilTest, CopyDirectory) {
  const char* from_dir = "fromdir";
  const char* from_file = "fromdir/fromfile";
  const char* to_dir = "todir";
  const char* to_file = "todir/fromfile";
  bool created;
  std::unique_ptr<FileSystemOperationContext> context;

  context.reset(NewContext());
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->CreateDirectory(context.get(), CreateURL(from_dir),
                                         false, false));
  ASSERT_EQ(base::File::FILE_OK, EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);

  context.reset(NewContext());
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->Truncate(context.get(), CreateURL(from_file), 1020));

  EXPECT_TRUE(DirectoryExists(from_dir));
  EXPECT_TRUE(FileExists(from_file));
  EXPECT_EQ(1020, GetSize(from_file));
  EXPECT_FALSE(DirectoryExists(to_dir));

  context.reset(NewContext());
  ASSERT_EQ(base::File::FILE_OK,
            AsyncFileTestHelper::Copy(file_system_context(),
                                      CreateURL(from_dir), CreateURL(to_dir)));

  EXPECT_TRUE(DirectoryExists(from_dir));
  EXPECT_TRUE(FileExists(from_file));
  EXPECT_EQ(1020, GetSize(from_file));
  EXPECT_TRUE(DirectoryExists(to_dir));
  EXPECT_TRUE(FileExists(to_file));
  EXPECT_EQ(1020, GetSize(to_file));
}

TEST_F(LocalFileUtilTest, MoveFile) {
  const char* from_file = "fromfile";
  const char* to_file = "tofile";
  bool created;
  ASSERT_EQ(base::File::FILE_OK, EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);
  std::unique_ptr<FileSystemOperationContext> context;

  context.reset(NewContext());
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->Truncate(context.get(), CreateURL(from_file), 1020));

  EXPECT_TRUE(FileExists(from_file));
  EXPECT_EQ(1020, GetSize(from_file));

  context.reset(NewContext());
  ASSERT_EQ(base::File::FILE_OK, AsyncFileTestHelper::Move(
                                     file_system_context(),
                                     CreateURL(from_file), CreateURL(to_file)));

  EXPECT_FALSE(FileExists(from_file));
  EXPECT_TRUE(FileExists(to_file));
  EXPECT_EQ(1020, GetSize(to_file));
}

TEST_F(LocalFileUtilTest, MoveDirectory) {
  const char* from_dir = "fromdir";
  const char* from_file = "fromdir/fromfile";
  const char* to_dir = "todir";
  const char* to_file = "todir/fromfile";
  bool created;
  std::unique_ptr<FileSystemOperationContext> context;

  context.reset(NewContext());
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->CreateDirectory(context.get(), CreateURL(from_dir),
                                         false, false));
  ASSERT_EQ(base::File::FILE_OK, EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);

  context.reset(NewContext());
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->Truncate(context.get(), CreateURL(from_file), 1020));

  EXPECT_TRUE(DirectoryExists(from_dir));
  EXPECT_TRUE(FileExists(from_file));
  EXPECT_EQ(1020, GetSize(from_file));
  EXPECT_FALSE(DirectoryExists(to_dir));

  context.reset(NewContext());
  ASSERT_EQ(base::File::FILE_OK,
            AsyncFileTestHelper::Move(file_system_context(),
                                      CreateURL(from_dir), CreateURL(to_dir)));

  EXPECT_FALSE(DirectoryExists(from_dir));
  EXPECT_TRUE(DirectoryExists(to_dir));
  EXPECT_TRUE(FileExists(to_file));
  EXPECT_EQ(1020, GetSize(to_file));
}

}  // namespace content
