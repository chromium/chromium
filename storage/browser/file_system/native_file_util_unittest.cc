// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <set>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "storage/browser/file_system/native_file_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using storage::FileSystemFileUtil;
using storage::FileSystemOperation;
using storage::NativeFileUtil;

namespace content {

class NativeFileUtilTest : public testing::Test {
 public:
  NativeFileUtilTest() = default;

  void SetUp() override { ASSERT_TRUE(data_dir_.CreateUniqueTempDir()); }

 protected:
  base::FilePath Path() { return data_dir_.GetPath(); }

  base::FilePath Path(const char* file_name) {
    return data_dir_.GetPath().AppendASCII(file_name);
  }

  bool FileExists(const base::FilePath& path) {
    return base::PathExists(path) && !base::DirectoryExists(path);
  }

  int64_t GetSize(const base::FilePath& path) {
    base::File::Info info;
    base::GetFileInfo(path, &info);
    return info.size;
  }

 private:
  base::ScopedTempDir data_dir_;

  DISALLOW_COPY_AND_ASSIGN(NativeFileUtilTest);
};

TEST_F(NativeFileUtilTest, CreateCloseAndDeleteFile) {
  base::FilePath file_name = Path("test_file");
  int flags = base::File::FLAG_WRITE | base::File::FLAG_ASYNC;
  base::File file =
      NativeFileUtil::CreateOrOpen(file_name, base::File::FLAG_CREATE | flags);
  ASSERT_TRUE(file.IsValid());
  ASSERT_TRUE(file.created());

  EXPECT_TRUE(base::PathExists(file_name));
  EXPECT_TRUE(NativeFileUtil::PathExists(file_name));
  EXPECT_EQ(0, GetSize(file_name));
  file.Close();

  file = NativeFileUtil::CreateOrOpen(file_name, base::File::FLAG_OPEN | flags);
  ASSERT_TRUE(file.IsValid());
  ASSERT_FALSE(file.created());
  file.Close();

  ASSERT_EQ(base::File::FILE_OK, NativeFileUtil::DeleteFile(file_name));
  EXPECT_FALSE(base::PathExists(file_name));
  EXPECT_FALSE(NativeFileUtil::PathExists(file_name));
}

TEST_F(NativeFileUtilTest, EnsureFileExists) {
  base::FilePath file_name = Path("foobar");
  bool created = false;
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::EnsureFileExists(file_name, &created));
  ASSERT_TRUE(created);

  EXPECT_TRUE(FileExists(file_name));
  EXPECT_EQ(0, GetSize(file_name));

  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::EnsureFileExists(file_name, &created));
  EXPECT_FALSE(created);
}

TEST_F(NativeFileUtilTest, CreateAndDeleteDirectory) {
  base::FilePath dir_name = Path("test_dir");
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::CreateDirectory(dir_name, false /* exclusive */,
                                            false /* recursive */));

  EXPECT_TRUE(NativeFileUtil::DirectoryExists(dir_name));
  EXPECT_TRUE(base::DirectoryExists(dir_name));

  ASSERT_EQ(base::File::FILE_ERROR_EXISTS,
            NativeFileUtil::CreateDirectory(dir_name, true /* exclusive */,
                                            false /* recursive */));

  ASSERT_EQ(base::File::FILE_OK, NativeFileUtil::DeleteDirectory(dir_name));
  EXPECT_FALSE(base::DirectoryExists(dir_name));
  EXPECT_FALSE(NativeFileUtil::DirectoryExists(dir_name));
}

TEST_F(NativeFileUtilTest, TouchFileAndGetFileInfo) {
  base::FilePath file_name = Path("test_file");
  base::File::Info native_info;
  ASSERT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            NativeFileUtil::GetFileInfo(file_name, &native_info));

  bool created = false;
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::EnsureFileExists(file_name, &created));
  ASSERT_TRUE(created);

  base::File::Info info;
  ASSERT_TRUE(base::GetFileInfo(file_name, &info));
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::GetFileInfo(file_name, &native_info));
  ASSERT_EQ(info.size, native_info.size);
  ASSERT_EQ(info.is_directory, native_info.is_directory);
  ASSERT_EQ(info.is_symbolic_link, native_info.is_symbolic_link);
  ASSERT_EQ(info.last_modified, native_info.last_modified);
  ASSERT_EQ(info.last_accessed, native_info.last_accessed);
  ASSERT_EQ(info.creation_time, native_info.creation_time);

  const base::Time new_accessed =
      info.last_accessed + base::TimeDelta::FromHours(10);
  const base::Time new_modified =
      info.last_modified + base::TimeDelta::FromHours(5);

  EXPECT_EQ(base::File::FILE_OK,
            NativeFileUtil::Touch(file_name, new_accessed, new_modified));

  ASSERT_TRUE(base::GetFileInfo(file_name, &info));
  EXPECT_EQ(new_accessed, info.last_accessed);
  EXPECT_EQ(new_modified, info.last_modified);
}

TEST_F(NativeFileUtilTest, CreateFileEnumerator) {
  base::FilePath path_1 = Path("dir1");
  base::FilePath path_2 = Path("file1");
  base::FilePath path_11 = Path("dir1").AppendASCII("file11");
  base::FilePath path_12 = Path("dir1").AppendASCII("dir12");
  base::FilePath path_121 =
      Path("dir1").AppendASCII("dir12").AppendASCII("file121");
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::CreateDirectory(path_1, false, false));
  bool created = false;
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::EnsureFileExists(path_2, &created));
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::EnsureFileExists(path_11, &created));
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::CreateDirectory(path_12, false, false));
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::EnsureFileExists(path_121, &created));

  {
    std::unique_ptr<FileSystemFileUtil::AbstractFileEnumerator> enumerator =
        NativeFileUtil::CreateFileEnumerator(Path(), false);
    std::set<base::FilePath> set;
    set.insert(path_1);
    set.insert(path_2);
    for (base::FilePath path = enumerator->Next(); !path.empty();
         path = enumerator->Next())
      EXPECT_EQ(1U, set.erase(path));
    EXPECT_TRUE(set.empty());
  }

  {
    std::unique_ptr<FileSystemFileUtil::AbstractFileEnumerator> enumerator =
        NativeFileUtil::CreateFileEnumerator(Path(), true);
    std::set<base::FilePath> set;
    set.insert(path_1);
    set.insert(path_2);
    set.insert(path_11);
    set.insert(path_12);
    set.insert(path_121);
    for (base::FilePath path = enumerator->Next(); !path.empty();
         path = enumerator->Next())
      EXPECT_EQ(1U, set.erase(path));
    EXPECT_TRUE(set.empty());
  }
}

TEST_F(NativeFileUtilTest, Truncate) {
  base::FilePath file_name = Path("truncated");
  bool created = false;
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::EnsureFileExists(file_name, &created));
  ASSERT_TRUE(created);

  ASSERT_EQ(base::File::FILE_OK, NativeFileUtil::Truncate(file_name, 1020));

  EXPECT_TRUE(FileExists(file_name));
  EXPECT_EQ(1020, GetSize(file_name));
}

TEST_F(NativeFileUtilTest, CopyFile) {
  base::FilePath from_file = Path("fromfile");
  base::FilePath to_file1 = Path("tofile1");
  base::FilePath to_file2 = Path("tofile2");
  const NativeFileUtil::CopyOrMoveMode nosync = NativeFileUtil::COPY_NOSYNC;
  const NativeFileUtil::CopyOrMoveMode sync = NativeFileUtil::COPY_SYNC;
  bool created = false;
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);

  ASSERT_EQ(base::File::FILE_OK, NativeFileUtil::Truncate(from_file, 1020));

  EXPECT_TRUE(FileExists(from_file));
  EXPECT_EQ(1020, GetSize(from_file));

  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::CopyOrMoveFile(
                from_file, to_file1, FileSystemOperation::OPTION_NONE, nosync));

  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::CopyOrMoveFile(
                from_file, to_file2, FileSystemOperation::OPTION_NONE, sync));

  EXPECT_TRUE(FileExists(from_file));
  EXPECT_EQ(1020, GetSize(from_file));
  EXPECT_TRUE(FileExists(to_file1));
  EXPECT_EQ(1020, GetSize(to_file1));
  EXPECT_TRUE(FileExists(to_file2));
  EXPECT_EQ(1020, GetSize(to_file2));

  base::FilePath dir = Path("dir");
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::CreateDirectory(dir, false, false));
  ASSERT_TRUE(base::DirectoryExists(dir));
  base::FilePath to_dir_file = dir.AppendASCII("file");
  ASSERT_EQ(base::File::FILE_OK, NativeFileUtil::CopyOrMoveFile(
                                     from_file, to_dir_file,
                                     FileSystemOperation::OPTION_NONE, nosync));
  EXPECT_TRUE(FileExists(to_dir_file));
  EXPECT_EQ(1020, GetSize(to_dir_file));

  // Following tests are error checking.
  // Source doesn't exist.
  EXPECT_EQ(
      base::File::FILE_ERROR_NOT_FOUND,
      NativeFileUtil::CopyOrMoveFile(Path("nonexists"), Path("file"),
                                     FileSystemOperation::OPTION_NONE, nosync));

  // Source is not a file.
  EXPECT_EQ(base::File::FILE_ERROR_NOT_A_FILE,
            NativeFileUtil::CopyOrMoveFile(
                dir, Path("file"), FileSystemOperation::OPTION_NONE, nosync));
  // Destination is not a file.
  EXPECT_EQ(base::File::FILE_ERROR_INVALID_OPERATION,
            NativeFileUtil::CopyOrMoveFile(
                from_file, dir, FileSystemOperation::OPTION_NONE, nosync));
  // Destination's parent doesn't exist.
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            NativeFileUtil::CopyOrMoveFile(
                from_file, Path("nodir").AppendASCII("file"),
                FileSystemOperation::OPTION_NONE, nosync));
  // Destination's parent is a file.
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            NativeFileUtil::CopyOrMoveFile(
                from_file, Path("tofile1").AppendASCII("file"),
                FileSystemOperation::OPTION_NONE, nosync));
}

TEST_F(NativeFileUtilTest, MoveFile) {
  base::FilePath from_file = Path("fromfile");
  base::FilePath to_file = Path("tofile");
  const NativeFileUtil::CopyOrMoveMode move = NativeFileUtil::MOVE;
  bool created = false;
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);

  ASSERT_EQ(base::File::FILE_OK, NativeFileUtil::Truncate(from_file, 1020));

  EXPECT_TRUE(FileExists(from_file));
  EXPECT_EQ(1020, GetSize(from_file));

  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::CopyOrMoveFile(
                from_file, to_file, FileSystemOperation::OPTION_NONE, move));

  EXPECT_FALSE(FileExists(from_file));
  EXPECT_TRUE(FileExists(to_file));
  EXPECT_EQ(1020, GetSize(to_file));

  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::EnsureFileExists(from_file, &created));
  ASSERT_TRUE(FileExists(from_file));
  ASSERT_EQ(base::File::FILE_OK, NativeFileUtil::Truncate(from_file, 1020));

  base::FilePath dir = Path("dir");
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::CreateDirectory(dir, false, false));
  ASSERT_TRUE(base::DirectoryExists(dir));
  base::FilePath to_dir_file = dir.AppendASCII("file");
  ASSERT_EQ(base::File::FILE_OK, NativeFileUtil::CopyOrMoveFile(
                                     from_file, to_dir_file,
                                     FileSystemOperation::OPTION_NONE, move));
  EXPECT_FALSE(FileExists(from_file));
  EXPECT_TRUE(FileExists(to_dir_file));
  EXPECT_EQ(1020, GetSize(to_dir_file));

  // Following is error checking.
  // Source doesn't exist.
  EXPECT_EQ(
      base::File::FILE_ERROR_NOT_FOUND,
      NativeFileUtil::CopyOrMoveFile(Path("nonexists"), Path("file"),
                                     FileSystemOperation::OPTION_NONE, move));

  base::FilePath dir2 = Path("dir2");
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::CreateDirectory(dir2, false, false));
  ASSERT_TRUE(base::DirectoryExists(dir2));
  // Source is a directory, destination is a file.
  EXPECT_EQ(base::File::FILE_ERROR_INVALID_OPERATION,
            NativeFileUtil::CopyOrMoveFile(
                dir, to_file, FileSystemOperation::OPTION_NONE, move));

#if defined(OS_WIN)
  // Source is a directory, destination is a directory.
  EXPECT_EQ(base::File::FILE_ERROR_NOT_A_FILE,
            NativeFileUtil::CopyOrMoveFile(
                dir, dir2, FileSystemOperation::OPTION_NONE, move));
#endif

  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::EnsureFileExists(from_file, &created));
  ASSERT_TRUE(FileExists(from_file));
  // Destination is not a file.
  EXPECT_EQ(base::File::FILE_ERROR_INVALID_OPERATION,
            NativeFileUtil::CopyOrMoveFile(
                from_file, dir, FileSystemOperation::OPTION_NONE, move));

  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::EnsureFileExists(from_file, &created));
  ASSERT_TRUE(FileExists(from_file));
  // Destination's parent doesn't exist.
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            NativeFileUtil::CopyOrMoveFile(
                from_file, Path("nodir").AppendASCII("file"),
                FileSystemOperation::OPTION_NONE, move));
  // Destination's parent is a file.
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            NativeFileUtil::CopyOrMoveFile(
                from_file, Path("tofile1").AppendASCII("file"),
                FileSystemOperation::OPTION_NONE, move));
}

TEST_F(NativeFileUtilTest, MoveFile_Directory) {
  base::FilePath from_directory = Path("fromdirectory");
  base::FilePath to_directory = Path("todirectory");
  base::FilePath from_file = from_directory.AppendASCII("fromfile");
  base::FilePath to_file = to_directory.AppendASCII("fromfile");
  ASSERT_TRUE(base::CreateDirectory(from_directory));
  const NativeFileUtil::CopyOrMoveMode move = NativeFileUtil::MOVE;
  bool created = false;
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);

  ASSERT_EQ(base::File::FILE_OK, NativeFileUtil::Truncate(from_file, 1020));

  EXPECT_TRUE(FileExists(from_file));
  EXPECT_EQ(1020, GetSize(from_file));

  ASSERT_EQ(base::File::FILE_OK, NativeFileUtil::CopyOrMoveFile(
                                     from_directory, to_directory,
                                     FileSystemOperation::OPTION_NONE, move));

  EXPECT_FALSE(base::DirectoryExists(from_directory));
  EXPECT_FALSE(FileExists(from_file));
  EXPECT_TRUE(base::DirectoryExists(to_directory));
  EXPECT_TRUE(FileExists(to_file));
  EXPECT_EQ(1020, GetSize(to_file));
}

#if !defined(OS_WIN)
TEST_F(NativeFileUtilTest, MoveFile_OverwriteEmptyDirectory) {
  base::FilePath from_directory = Path("fromdirectory");
  base::FilePath to_directory = Path("todirectory");
  base::FilePath from_file = from_directory.AppendASCII("fromfile");
  base::FilePath to_file = to_directory.AppendASCII("fromfile");
  ASSERT_TRUE(base::CreateDirectory(from_directory));
  ASSERT_TRUE(base::CreateDirectory(to_directory));
  const NativeFileUtil::CopyOrMoveMode move = NativeFileUtil::MOVE;
  bool created = false;
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);

  ASSERT_EQ(base::File::FILE_OK, NativeFileUtil::Truncate(from_file, 1020));

  EXPECT_TRUE(FileExists(from_file));
  EXPECT_EQ(1020, GetSize(from_file));

  ASSERT_EQ(base::File::FILE_OK, NativeFileUtil::CopyOrMoveFile(
                                     from_directory, to_directory,
                                     FileSystemOperation::OPTION_NONE, move));

  EXPECT_FALSE(base::DirectoryExists(from_directory));
  EXPECT_FALSE(FileExists(from_file));
  EXPECT_TRUE(base::DirectoryExists(to_directory));
  EXPECT_TRUE(FileExists(to_file));
  EXPECT_EQ(1020, GetSize(to_file));
}
#endif

TEST_F(NativeFileUtilTest, PreserveLastModified) {
  base::FilePath from_file = Path("fromfile");
  base::FilePath to_file1 = Path("tofile1");
  base::FilePath to_file2 = Path("tofile2");
  base::FilePath to_file3 = Path("tofile3");
  bool created = false;
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);
  EXPECT_TRUE(FileExists(from_file));

  base::File::Info file_info1;
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::GetFileInfo(from_file, &file_info1));

  // Test for copy (nosync).
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::CopyOrMoveFile(
                from_file, to_file1,
                FileSystemOperation::OPTION_PRESERVE_LAST_MODIFIED,
                NativeFileUtil::COPY_NOSYNC));

  base::File::Info file_info2;
  ASSERT_TRUE(FileExists(to_file1));
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::GetFileInfo(to_file1, &file_info2));
  EXPECT_EQ(file_info1.last_modified, file_info2.last_modified);

  // Test for copy (sync).
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::CopyOrMoveFile(
                from_file, to_file2,
                FileSystemOperation::OPTION_PRESERVE_LAST_MODIFIED,
                NativeFileUtil::COPY_SYNC));

  ASSERT_TRUE(FileExists(to_file2));
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::GetFileInfo(to_file1, &file_info2));
  EXPECT_EQ(file_info1.last_modified, file_info2.last_modified);

  // Test for move.
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::CopyOrMoveFile(
                from_file, to_file3,
                FileSystemOperation::OPTION_PRESERVE_LAST_MODIFIED,
                NativeFileUtil::MOVE));

  ASSERT_TRUE(FileExists(to_file3));
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::GetFileInfo(to_file2, &file_info2));
  EXPECT_EQ(file_info1.last_modified, file_info2.last_modified);
}

}  // namespace content
