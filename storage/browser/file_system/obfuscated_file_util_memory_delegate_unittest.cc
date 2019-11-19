// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/obfuscated_file_util_memory_delegate.h"

#include <memory>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "net/base/io_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

class ObfuscatedFileUtilMemoryDelegateTest : public testing::Test {
 public:
  ObfuscatedFileUtilMemoryDelegateTest() = default;

  void SetUp() override {
    ASSERT_TRUE(file_system_directory_.CreateUniqueTempDir());
    file_util_ = std::make_unique<ObfuscatedFileUtilMemoryDelegate>(
        file_system_directory_.GetPath());
  }

  void TearDown() override {
    // In memory operations should not have any residue in file system
    // directory.
    EXPECT_TRUE(base::IsDirectoryEmpty(file_system_directory_.GetPath()));
  }

  ObfuscatedFileUtilMemoryDelegate* file_util() { return file_util_.get(); }

 protected:
  base::FilePath Path() { return file_system_directory_.GetPath(); }

  base::FilePath Path(const char* file_name) {
    return file_system_directory_.GetPath().AppendASCII(file_name);
  }

  bool FileExists(const base::FilePath& path) {
    return file_util()->PathExists(path) && !file_util()->DirectoryExists(path);
  }

  int64_t GetSize(const base::FilePath& path) {
    base::File::Info info;
    file_util()->GetFileInfo(path, &info);
    return info.size;
  }

 private:
  base::ScopedTempDir file_system_directory_;
  std::unique_ptr<ObfuscatedFileUtilMemoryDelegate> file_util_;

  DISALLOW_COPY_AND_ASSIGN(ObfuscatedFileUtilMemoryDelegateTest);
};

TEST_F(ObfuscatedFileUtilMemoryDelegateTest, CreateOrOpenFile) {
  base::FilePath file_name = Path("test_file");

  base::File file =
      file_util()->CreateOrOpen(file_name, base::File::FLAG_CREATE);
  ASSERT_EQ(base::File::FILE_ERROR_INVALID_OPERATION, file.error_details());
}

TEST_F(ObfuscatedFileUtilMemoryDelegateTest, CreateAndDeleteFile) {
  base::FilePath file_name = Path("test_file");

  bool created = false;
  base::File::Error result = file_util()->EnsureFileExists(file_name, &created);
  ASSERT_EQ(base::File::FILE_OK, result);
  ASSERT_TRUE(created);
  EXPECT_FALSE(base::PathExists(file_name));
  EXPECT_TRUE(file_util()->PathExists(file_name));
  EXPECT_EQ(0, GetSize(file_name));

  result = file_util()->EnsureFileExists(file_name, &created);
  ASSERT_EQ(base::File::FILE_OK, result);
  ASSERT_FALSE(created);

  result = file_util()->DeleteFile(file_name);
  ASSERT_EQ(base::File::FILE_OK, result);
  EXPECT_FALSE(base::PathExists(file_name));
  EXPECT_FALSE(file_util()->PathExists(file_name));
}

TEST_F(ObfuscatedFileUtilMemoryDelegateTest, PathNormalization) {
  base::FilePath file_name1 = Path("foo");
  std::vector<const char*> components = {"bar", "..", "baz", ".",  "qux",
                                         "..",  ".",  "..",  "foo"};
  base::FilePath file_name2 = Path(components[0]);

  for (size_t i = 1; i < components.size(); i++)
    file_name2 = file_name2.AppendASCII(components[i]);

  bool created = false;
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->EnsureFileExists(file_name1, &created));

  EXPECT_TRUE(FileExists(file_name1));
  EXPECT_TRUE(FileExists(file_name2));
}

TEST_F(ObfuscatedFileUtilMemoryDelegateTest, PathBeyondRoot) {
  base::FilePath file_name = Path("..").AppendASCII("foo");

  bool created = false;
  ASSERT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            file_util()->EnsureFileExists(file_name, &created));
  ASSERT_FALSE(created);

  EXPECT_FALSE(FileExists(file_name));
}

TEST_F(ObfuscatedFileUtilMemoryDelegateTest, EnsureFileExists) {
  base::FilePath file_name = Path("foobar");
  bool created = false;
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->EnsureFileExists(file_name, &created));
  ASSERT_TRUE(created);

  EXPECT_TRUE(FileExists(file_name));
  EXPECT_FALSE(base::PathExists(file_name));
  EXPECT_EQ(0, GetSize(file_name));

  ASSERT_EQ(base::File::FILE_OK,
            file_util()->EnsureFileExists(file_name, &created));
  EXPECT_FALSE(created);
}

TEST_F(ObfuscatedFileUtilMemoryDelegateTest, CreateAndDeleteDirectory) {
  base::FilePath dir_name = Path("test_dir");

  ASSERT_EQ(base::File::FILE_OK,
            file_util()->CreateDirectory(dir_name, false /* exclusive */,
                                         false /* recursive */));
  EXPECT_TRUE(file_util()->DirectoryExists(dir_name));
  EXPECT_FALSE(base::DirectoryExists(dir_name));

  ASSERT_EQ(base::File::FILE_ERROR_EXISTS,
            file_util()->CreateDirectory(dir_name, true /* exclusive */,
                                         false /* recursive */));

  EXPECT_TRUE(
      file_util()->DeleteFileOrDirectory(dir_name, false /* recursive */));
  EXPECT_FALSE(file_util()->DirectoryExists(dir_name));
}

TEST_F(ObfuscatedFileUtilMemoryDelegateTest,
       CreateAndDeleteDirectoryRecursive) {
  base::FilePath dir_name = Path("test_dir");
  base::FilePath child_name = dir_name.AppendASCII("child_dir");
  base::FilePath grandchild_name = child_name.AppendASCII("grandchild_dir");

  ASSERT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            file_util()->CreateDirectory(grandchild_name, false /* exclusive */,
                                         false /* recursive */));

  ASSERT_EQ(base::File::FILE_OK,
            file_util()->CreateDirectory(grandchild_name, false /* exclusive */,
                                         true /* recursive */));

  EXPECT_TRUE(file_util()->DirectoryExists(dir_name));
  EXPECT_TRUE(file_util()->DirectoryExists(child_name));
  EXPECT_TRUE(file_util()->DirectoryExists(grandchild_name));

  EXPECT_FALSE(
      file_util()->DeleteFileOrDirectory(dir_name, false /* recursive */));

  EXPECT_TRUE(file_util()->DirectoryExists(dir_name));
  EXPECT_TRUE(file_util()->DirectoryExists(child_name));
  EXPECT_TRUE(file_util()->DirectoryExists(grandchild_name));

  EXPECT_TRUE(
      file_util()->DeleteFileOrDirectory(dir_name, true /* recursive */));

  EXPECT_FALSE(file_util()->DirectoryExists(dir_name));
  EXPECT_FALSE(file_util()->DirectoryExists(child_name));
  EXPECT_FALSE(file_util()->DirectoryExists(grandchild_name));
}

TEST_F(ObfuscatedFileUtilMemoryDelegateTest, DeleteNoneEmptyDirectory) {
  base::FilePath dir_name = Path("test_dir");
  base::FilePath child_dir_name = dir_name.AppendASCII("child_dir");
  base::FilePath file_name = child_dir_name.AppendASCII("child_file");

  bool created;
  base::File::Error result = file_util()->EnsureFileExists(file_name, &created);
  ASSERT_EQ(base::File::FILE_ERROR_NOT_FOUND, result);
  EXPECT_FALSE(file_util()->PathExists(file_name));

  ASSERT_EQ(base::File::FILE_OK,
            file_util()->CreateDirectory(child_dir_name, false /* exclusive*/,
                                         true /* recursive */));
  result = file_util()->EnsureFileExists(file_name, &created);
  ASSERT_EQ(base::File::FILE_OK, result);
  EXPECT_TRUE(file_util()->PathExists(file_name));

  EXPECT_FALSE(
      file_util()->DeleteFileOrDirectory(dir_name, false /* recursive */));
  EXPECT_FALSE(file_util()->DeleteFileOrDirectory(child_dir_name,
                                                  false /* recursive */));
  EXPECT_TRUE(
      file_util()->DeleteFileOrDirectory(dir_name, true /* recursive */));
  EXPECT_FALSE(file_util()->PathExists(child_dir_name));
  EXPECT_FALSE(file_util()->PathExists(file_name));
}

TEST_F(ObfuscatedFileUtilMemoryDelegateTest, TouchFileAndGetFileInfo) {
  base::FilePath file_name = Path("test_file");
  base::File::Info info;
  ASSERT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            file_util()->GetFileInfo(file_name, &info));

  bool created = false;
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->EnsureFileExists(file_name, &created));
  ASSERT_TRUE(created);

  ASSERT_FALSE(base::GetFileInfo(file_name, &info));
  ASSERT_EQ(base::File::FILE_OK, file_util()->GetFileInfo(file_name, &info));
  ASSERT_EQ(0, info.size);
  ASSERT_EQ(false, info.is_directory);
  ASSERT_EQ(false, info.is_symbolic_link);

  const base::Time new_accessed =
      info.last_accessed + base::TimeDelta::FromHours(10);
  const base::Time new_modified =
      info.last_modified + base::TimeDelta::FromHours(5);

  EXPECT_EQ(base::File::FILE_OK,
            file_util()->Touch(file_name, new_accessed, new_modified));

  ASSERT_EQ(base::File::FILE_OK, file_util()->GetFileInfo(file_name, &info));
  EXPECT_EQ(new_accessed, info.last_accessed);
  EXPECT_EQ(new_modified, info.last_modified);
}

TEST_F(ObfuscatedFileUtilMemoryDelegateTest, Truncate) {
  base::FilePath file_name = Path("truncated");

  bool created = false;
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->EnsureFileExists(file_name, &created));
  ASSERT_TRUE(created);

  ASSERT_EQ(base::File::FILE_OK, file_util()->Truncate(file_name, 1020));

  EXPECT_TRUE(FileExists(file_name));
  EXPECT_EQ(1020, GetSize(file_name));
}

TEST_F(ObfuscatedFileUtilMemoryDelegateTest, CopyFile) {
  base::FilePath from_file = Path("fromfile");
  base::FilePath to_file1 = Path("tofile1");
  base::FilePath to_file2 = Path("tofile2");
  const storage::NativeFileUtil::CopyOrMoveMode nosync =
      storage::NativeFileUtil::COPY_NOSYNC;
  const storage::NativeFileUtil::CopyOrMoveMode sync =
      storage::NativeFileUtil::COPY_SYNC;

  bool created = false;
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);

  ASSERT_EQ(base::File::FILE_OK, file_util()->Truncate(from_file, 1020));

  EXPECT_TRUE(FileExists(from_file));
  EXPECT_EQ(1020, GetSize(from_file));

  ASSERT_EQ(base::File::FILE_OK,
            file_util()->CopyOrMoveFile(
                from_file, to_file1, FileSystemOperation::OPTION_NONE, sync));

  ASSERT_EQ(base::File::FILE_OK,
            file_util()->CopyOrMoveFile(
                from_file, to_file2, FileSystemOperation::OPTION_NONE, nosync));

  EXPECT_TRUE(FileExists(from_file));
  EXPECT_EQ(1020, GetSize(from_file));
  EXPECT_TRUE(FileExists(to_file1));
  EXPECT_EQ(1020, GetSize(to_file1));
  EXPECT_TRUE(FileExists(to_file2));
  EXPECT_EQ(1020, GetSize(to_file2));

  base::FilePath dir = Path("dir");
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->CreateDirectory(dir, false, false));
  ASSERT_TRUE(file_util()->DirectoryExists(dir));
  base::FilePath to_dir_file = dir.AppendASCII("file");
  ASSERT_EQ(base::File::FILE_OK, file_util()->CopyOrMoveFile(
                                     from_file, to_dir_file,
                                     FileSystemOperation::OPTION_NONE, nosync));
  EXPECT_TRUE(FileExists(to_dir_file));
  EXPECT_EQ(1020, GetSize(to_dir_file));
}

TEST_F(ObfuscatedFileUtilMemoryDelegateTest, CopyForeignFile) {
  base::ScopedTempDir source_dir;
  ASSERT_TRUE(source_dir.CreateUniqueTempDir());
  base::FilePath from_file = source_dir.GetPath().AppendASCII("from_file");

  base::FilePath valid_to_file = Path("to_file");
  base::FilePath invalid_to_file = Path("dir").AppendASCII("to_file");

  char test_data[] = "0123456789";
  const int test_data_len = strlen(test_data);

  const storage::NativeFileUtil::CopyOrMoveMode sync =
      storage::NativeFileUtil::COPY_SYNC;

  // Test copying nonexistent file.
  EXPECT_EQ(
      base::File::FILE_ERROR_NOT_FOUND,
      file_util()->CopyInForeignFile(from_file, valid_to_file,
                                     FileSystemOperation::OPTION_NONE, sync));

  // Create source file.
  EXPECT_EQ(test_data_len,
            base::WriteFile(from_file, test_data, test_data_len));

  // Test copying to a nonexistent directory.
  EXPECT_EQ(
      base::File::FILE_ERROR_NOT_FOUND,
      file_util()->CopyInForeignFile(from_file, invalid_to_file,
                                     FileSystemOperation::OPTION_NONE, sync));
  EXPECT_FALSE(FileExists(invalid_to_file));

  // Test copying to a valid path.
  EXPECT_EQ(base::File::FILE_OK, file_util()->CopyInForeignFile(
                                     from_file, valid_to_file,
                                     FileSystemOperation::OPTION_NONE, sync));
  EXPECT_TRUE(FileExists(valid_to_file));
  EXPECT_EQ(test_data_len, GetSize(valid_to_file));
  scoped_refptr<net::IOBuffer> content =
      base::MakeRefCounted<net::IOBuffer>(static_cast<size_t>(test_data_len));
  EXPECT_EQ(test_data_len, file_util()->ReadFile(valid_to_file, 0,
                                                 content.get(), test_data_len));
  EXPECT_EQ(std::string(test_data),
            std::string(content->data(), test_data_len));
}

TEST_F(ObfuscatedFileUtilMemoryDelegateTest, CopyFileNonExistingFile) {
  const storage::NativeFileUtil::CopyOrMoveMode nosync =
      storage::NativeFileUtil::COPY_NOSYNC;

  EXPECT_EQ(
      base::File::FILE_ERROR_NOT_FOUND,
      file_util()->CopyOrMoveFile(Path("nonexists"), Path("file"),
                                  FileSystemOperation::OPTION_NONE, nosync));
}

TEST_F(ObfuscatedFileUtilMemoryDelegateTest, CopyDirectoryOverFile) {
  const storage::NativeFileUtil::CopyOrMoveMode nosync =
      storage::NativeFileUtil::COPY_NOSYNC;

  base::FilePath dir = Path("dir");
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->CreateDirectory(dir, false, false));

  EXPECT_EQ(base::File::FILE_ERROR_NOT_A_FILE,
            file_util()->CopyOrMoveFile(
                dir, Path("file"), FileSystemOperation::OPTION_NONE, nosync));
}

TEST_F(ObfuscatedFileUtilMemoryDelegateTest, CopyFileOverDirectory) {
  base::FilePath file_name = Path("fromfile");
  base::FilePath dir = Path("dir");
  const storage::NativeFileUtil::CopyOrMoveMode nosync =
      storage::NativeFileUtil::COPY_NOSYNC;

  bool created = false;
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->EnsureFileExists(file_name, &created));
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->CreateDirectory(dir, false, false));

  EXPECT_EQ(base::File::FILE_ERROR_INVALID_OPERATION,
            file_util()->CopyOrMoveFile(
                file_name, dir, FileSystemOperation::OPTION_NONE, nosync));
}

TEST_F(ObfuscatedFileUtilMemoryDelegateTest, CopyFileToNonExistingDirectory) {
  base::FilePath file_name = Path("fromfile");
  const storage::NativeFileUtil::CopyOrMoveMode nosync =
      storage::NativeFileUtil::COPY_NOSYNC;

  bool created = false;
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->EnsureFileExists(file_name, &created));

  EXPECT_EQ(
      base::File::FILE_ERROR_NOT_FOUND,
      file_util()->CopyOrMoveFile(file_name, Path("nodir").AppendASCII("file"),
                                  FileSystemOperation::OPTION_NONE, nosync));
}

TEST_F(ObfuscatedFileUtilMemoryDelegateTest, CopyFileAsChildOfOtherFile) {
  base::FilePath from_file = Path("fromfile");
  base::FilePath to_file = Path("tofile");
  const storage::NativeFileUtil::CopyOrMoveMode nosync =
      storage::NativeFileUtil::COPY_NOSYNC;

  bool created = false;
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->EnsureFileExists(from_file, &created));
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->EnsureFileExists(to_file, &created));

  EXPECT_EQ(
      base::File::FILE_ERROR_NOT_FOUND,
      file_util()->CopyOrMoveFile(from_file, to_file.AppendASCII("file"),
                                  FileSystemOperation::OPTION_NONE, nosync));
}

TEST_F(ObfuscatedFileUtilMemoryDelegateTest, MoveFile) {
  base::FilePath from_file = Path("fromfile");
  base::FilePath to_file = Path("tofile");

  const storage::NativeFileUtil::CopyOrMoveMode move =
      storage::NativeFileUtil::MOVE;

  bool created = false;
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);

  ASSERT_EQ(base::File::FILE_OK, file_util()->Truncate(from_file, 1020));

  EXPECT_TRUE(FileExists(from_file));
  EXPECT_EQ(1020, GetSize(from_file));

  ASSERT_EQ(base::File::FILE_OK,
            file_util()->CopyOrMoveFile(
                from_file, to_file, FileSystemOperation::OPTION_NONE, move));

  EXPECT_FALSE(FileExists(from_file));
  EXPECT_TRUE(FileExists(to_file));
  EXPECT_EQ(1020, GetSize(to_file));

  ASSERT_EQ(base::File::FILE_OK,
            file_util()->EnsureFileExists(from_file, &created));
  ASSERT_TRUE(FileExists(from_file));
  ASSERT_EQ(base::File::FILE_OK, file_util()->Truncate(from_file, 1020));

  base::FilePath dir = Path("dir");
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->CreateDirectory(dir, false, false));
  ASSERT_TRUE(file_util()->DirectoryExists(dir));
  base::FilePath to_dir_file = dir.AppendASCII("file");
  ASSERT_EQ(base::File::FILE_OK, file_util()->CopyOrMoveFile(
                                     from_file, to_dir_file,
                                     FileSystemOperation::OPTION_NONE, move));
  EXPECT_FALSE(FileExists(from_file));
  EXPECT_TRUE(FileExists(to_dir_file));
  EXPECT_EQ(1020, GetSize(to_dir_file));
}

TEST_F(ObfuscatedFileUtilMemoryDelegateTest, MoveNonExistingFile) {
  const storage::NativeFileUtil::CopyOrMoveMode move =
      storage::NativeFileUtil::MOVE;

  EXPECT_EQ(
      base::File::FILE_ERROR_NOT_FOUND,
      file_util()->CopyOrMoveFile(Path("nonexists"), Path("file"),
                                  FileSystemOperation::OPTION_NONE, move));
}

TEST_F(ObfuscatedFileUtilMemoryDelegateTest, MoveDirectoryOverDirectory) {
  base::FilePath dir = Path("dir");
  base::FilePath dir2 = Path("dir2");

  const storage::NativeFileUtil::CopyOrMoveMode move =
      storage::NativeFileUtil::MOVE;

  ASSERT_EQ(base::File::FILE_OK,
            file_util()->CreateDirectory(dir, false, false));
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->CreateDirectory(dir2, false, false));

  base::File::Error result = file_util()->CopyOrMoveFile(
      dir, dir2, FileSystemOperation::OPTION_NONE, move);
#if defined(OS_WIN)
  EXPECT_EQ(base::File::FILE_ERROR_NOT_A_FILE, result);
#else
  EXPECT_EQ(base::File::FILE_OK, result);
#endif
}

TEST_F(ObfuscatedFileUtilMemoryDelegateTest, MoveFileOverDirectory) {
  base::FilePath from_file = Path("fromfile");
  base::FilePath dir = Path("dir");

  const storage::NativeFileUtil::CopyOrMoveMode move =
      storage::NativeFileUtil::MOVE;

  bool created = false;
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->EnsureFileExists(from_file, &created));
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->CreateDirectory(dir, false, false));

  EXPECT_EQ(base::File::FILE_ERROR_INVALID_OPERATION,
            file_util()->CopyOrMoveFile(
                from_file, dir, FileSystemOperation::OPTION_NONE, move));
}

TEST_F(ObfuscatedFileUtilMemoryDelegateTest, MoveFileToNonExistingDirectory) {
  base::FilePath from_file = Path("fromfile");

  const storage::NativeFileUtil::CopyOrMoveMode move =
      storage::NativeFileUtil::MOVE;

  bool created = false;
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->EnsureFileExists(from_file, &created));

  EXPECT_EQ(
      base::File::FILE_ERROR_NOT_FOUND,
      file_util()->CopyOrMoveFile(from_file, Path("nodir").AppendASCII("file"),
                                  FileSystemOperation::OPTION_NONE, move));
}

TEST_F(ObfuscatedFileUtilMemoryDelegateTest, MoveFileAsChildOfOtherFile) {
  base::FilePath from_file = Path("fromfile");
  base::FilePath to_file = Path("tofile");

  const storage::NativeFileUtil::CopyOrMoveMode move =
      storage::NativeFileUtil::MOVE;

  bool created = false;
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->EnsureFileExists(from_file, &created));
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->EnsureFileExists(to_file, &created));

  EXPECT_EQ(
      base::File::FILE_ERROR_NOT_FOUND,
      file_util()->CopyOrMoveFile(from_file, to_file.AppendASCII("file"),
                                  FileSystemOperation::OPTION_NONE, move));
}

TEST_F(ObfuscatedFileUtilMemoryDelegateTest, MoveFile_Directory) {
  base::FilePath from_directory = Path("fromdirectory");
  base::FilePath to_directory = Path("todirectory");
  base::FilePath from_file = from_directory.AppendASCII("fromfile");
  base::FilePath to_file = to_directory.AppendASCII("fromfile");

  ASSERT_EQ(base::File::FILE_OK,
            file_util()->CreateDirectory(from_directory, false /* exclusive */,
                                         false /* recursive */));
  const storage::NativeFileUtil::CopyOrMoveMode move =
      storage::NativeFileUtil::MOVE;
  bool created = false;
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);

  ASSERT_EQ(base::File::FILE_OK, file_util()->Truncate(from_file, 1020));

  EXPECT_TRUE(FileExists(from_file));
  EXPECT_EQ(1020, GetSize(from_file));

  ASSERT_EQ(base::File::FILE_OK, file_util()->CopyOrMoveFile(
                                     from_directory, to_directory,
                                     FileSystemOperation::OPTION_NONE, move));

  EXPECT_FALSE(file_util()->DirectoryExists(from_directory));
  EXPECT_FALSE(FileExists(from_file));
  EXPECT_TRUE(file_util()->DirectoryExists(to_directory));
  EXPECT_TRUE(FileExists(to_file));
  EXPECT_EQ(1020, GetSize(to_file));
}

#if !defined(OS_WIN)
TEST_F(ObfuscatedFileUtilMemoryDelegateTest, MoveFile_OverwriteEmptyDirectory) {
  base::FilePath from_directory = Path("fromdirectory");
  base::FilePath to_directory = Path("todirectory");
  base::FilePath from_file = from_directory.AppendASCII("fromfile");
  base::FilePath to_file = to_directory.AppendASCII("fromfile");
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->CreateDirectory(from_directory, false /* exclusive */,
                                         false /* recursive */));
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->CreateDirectory(to_directory, false /* exclusive */,
                                         false /* recursive */));
  const storage::NativeFileUtil::CopyOrMoveMode move =
      storage::NativeFileUtil::MOVE;
  bool created = false;
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);

  ASSERT_EQ(base::File::FILE_OK, file_util()->Truncate(from_file, 1020));

  EXPECT_TRUE(FileExists(from_file));
  EXPECT_EQ(1020, GetSize(from_file));
  ASSERT_EQ(base::File::FILE_OK, file_util()->CopyOrMoveFile(
                                     from_directory, to_directory,
                                     FileSystemOperation::OPTION_NONE, move));

  EXPECT_FALSE(file_util()->DirectoryExists(from_directory));
  EXPECT_FALSE(FileExists(from_file));
  EXPECT_TRUE(file_util()->DirectoryExists(to_directory));
  EXPECT_TRUE(FileExists(to_file));
  EXPECT_EQ(1020, GetSize(to_file));
}
#endif

TEST_F(ObfuscatedFileUtilMemoryDelegateTest, PreserveLastModified_NoSync) {
  base::FilePath from_file = Path("fromfile");
  base::FilePath to_file = Path("tofile");

  const storage::NativeFileUtil::CopyOrMoveMode nosync =
      storage::NativeFileUtil::COPY_NOSYNC;

  bool created = false;
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);
  EXPECT_TRUE(FileExists(from_file));

  base::File::Info file_info1;
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->GetFileInfo(from_file, &file_info1));

  ASSERT_EQ(base::File::FILE_OK,
            file_util()->CopyOrMoveFile(
                from_file, to_file,
                FileSystemOperation::OPTION_PRESERVE_LAST_MODIFIED, nosync));

  base::File::Info file_info2;
  ASSERT_TRUE(FileExists(to_file));
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->GetFileInfo(to_file, &file_info2));
  EXPECT_EQ(file_info1.last_modified, file_info2.last_modified);
}

TEST_F(ObfuscatedFileUtilMemoryDelegateTest, PreserveLastModified_Sync) {
  base::FilePath from_file = Path("fromfile");
  base::FilePath to_file = Path("tofile");

  const storage::NativeFileUtil::CopyOrMoveMode sync =
      storage::NativeFileUtil::COPY_SYNC;

  bool created = false;
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);
  EXPECT_TRUE(FileExists(from_file));

  base::File::Info file_info1;
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->GetFileInfo(from_file, &file_info1));

  ASSERT_EQ(base::File::FILE_OK,
            file_util()->CopyOrMoveFile(
                from_file, to_file,
                FileSystemOperation::OPTION_PRESERVE_LAST_MODIFIED, sync));
  ASSERT_TRUE(FileExists(to_file));

  base::File::Info file_info2;
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->GetFileInfo(to_file, &file_info2));
  EXPECT_EQ(file_info1.last_modified, file_info2.last_modified);
}

TEST_F(ObfuscatedFileUtilMemoryDelegateTest, PreserveLastModified_Move) {
  base::FilePath from_file = Path("fromfile");
  base::FilePath to_file = Path("tofile");

  const storage::NativeFileUtil::CopyOrMoveMode move =
      storage::NativeFileUtil::MOVE;

  bool created = false;
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);
  EXPECT_TRUE(FileExists(from_file));

  base::File::Info file_info1;
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->GetFileInfo(from_file, &file_info1));

  ASSERT_EQ(base::File::FILE_OK,
            file_util()->CopyOrMoveFile(
                from_file, to_file,
                FileSystemOperation::OPTION_PRESERVE_LAST_MODIFIED, move));
  ASSERT_TRUE(FileExists(to_file));

  base::File::Info file_info2;
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->GetFileInfo(to_file, &file_info2));
  EXPECT_EQ(file_info1.last_modified, file_info2.last_modified);
}

TEST_F(ObfuscatedFileUtilMemoryDelegateTest, ComputeDirectorySize) {
  base::FilePath file_name0 = Path("test_file0");
  base::FilePath dir_name1 = Path("dir1");
  base::FilePath file_name1 = dir_name1.AppendASCII("test_file1");
  base::FilePath dir_name2 = dir_name1.AppendASCII("dir2");
  base::FilePath file_name2 = dir_name2.AppendASCII("test_file2");
  char content[] = "01234567890123456789";

  ASSERT_EQ(base::File::FILE_OK,
            file_util()->CreateDirectory(dir_name2, false /* exclusive */,
                                         true /* recursive */));

  ASSERT_EQ(base::File::FILE_OK,
            file_util()->CreateFileForTesting(
                file_name0, base::span<const char>(content, 10)));
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->CreateFileForTesting(
                file_name1, base::span<const char>(content, 15)));
  ASSERT_EQ(base::File::FILE_OK,
            file_util()->CreateFileForTesting(
                file_name2, base::span<const char>(content, 20)));

  ASSERT_EQ(20u, file_util()->ComputeDirectorySize(dir_name2));
  ASSERT_EQ(35u, file_util()->ComputeDirectorySize(dir_name1));
  ASSERT_EQ(45u, file_util()->ComputeDirectorySize(Path()));
}

}  // namespace storage