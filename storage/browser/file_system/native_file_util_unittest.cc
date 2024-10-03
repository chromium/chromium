// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <set>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "build/build_config.h"
#include "storage/browser/file_system/native_file_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/test/android/content_uri_test_utils.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "windows.h"
#endif  // BUILDFLAG(IS_WIN)

namespace storage {
namespace {

using CopyOrMoveOption = FileSystemOperation::CopyOrMoveOption;
using CopyOrMoveOptionSet = FileSystemOperation::CopyOrMoveOptionSet;

}  // namespace

class NativeFileUtilTest : public testing::Test {
 public:
  NativeFileUtilTest() = default;

  NativeFileUtilTest(const NativeFileUtilTest&) = delete;
  NativeFileUtilTest& operator=(const NativeFileUtilTest&) = delete;

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

#if BUILDFLAG(IS_POSIX)
  void ExpectFileHasPermissionsPosix(base::FilePath file, int expected_mode) {
    base::File::Info file_info;
    int mode;
    ASSERT_TRUE(FileExists(file));

    EXPECT_TRUE(base::GetPosixFilePermissions(file, &mode));
    EXPECT_EQ(mode, expected_mode);
  }
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_WIN)
  void ExpectFileHasPermissionsWin(base::FilePath file,
                                   DWORD expected_attributes) {
    base::File::Info file_info;
    DWORD attributes;
    ASSERT_TRUE(FileExists(file));

    attributes = ::GetFileAttributes(file.value().c_str());
    EXPECT_NE(attributes, INVALID_FILE_ATTRIBUTES);
    EXPECT_EQ(attributes, expected_attributes);
  }
#endif  // BUILDFLAG(IS_WIN)

 private:
  base::ScopedTempDir data_dir_;
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

#if BUILDFLAG(IS_ANDROID)
  // Delete file and recreate using content-URI rather than path.
  ASSERT_TRUE(base::DeleteFile(file_name));

  base::FilePath content_uri =
      *base::test::android::GetContentUriFromCacheDirFilePath(file_name);

  EXPECT_EQ(base::File::FILE_OK,
            NativeFileUtil::EnsureFileExists(content_uri, &created));
  EXPECT_TRUE(created);

  EXPECT_TRUE(FileExists(file_name));
  EXPECT_EQ(0, GetSize(file_name));

  EXPECT_EQ(base::File::FILE_OK,
            NativeFileUtil::EnsureFileExists(file_name, &created));
  EXPECT_FALSE(created);
#endif
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

// TODO(crbug.com/40511450): Remove this test once last_access_time has
// been removed after PPAPI has been deprecated. Fuchsia does not support touch,
// which breaks this test that relies on it. Since PPAPI is being deprecated,
// this test is excluded from the Fuchsia build.
// See https://crbug.com/1077456 for details.
#if !BUILDFLAG(IS_FUCHSIA)
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

  const base::Time new_accessed = info.last_accessed + base::Hours(10);
  const base::Time new_modified = info.last_modified + base::Hours(5);

  EXPECT_EQ(base::File::FILE_OK,
            NativeFileUtil::Touch(file_name, new_accessed, new_modified));

  ASSERT_TRUE(base::GetFileInfo(file_name, &info));
  EXPECT_EQ(new_accessed, info.last_accessed);
  EXPECT_EQ(new_modified, info.last_modified);
}
#endif  // !BUILDFLAG(IS_FUCHSIA)

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

#if BUILDFLAG(IS_ANDROID)
  base::FilePath content_uri =
      *base::test::android::GetContentUriFromCacheDirFilePath(file_name);

  // Content-URIs only support truncate to zero.
  base::WriteFile(file_name, "foobar");
  EXPECT_EQ(base::File::FILE_ERROR_FAILED,
            NativeFileUtil::Truncate(content_uri, 1020));
  EXPECT_TRUE(FileExists(file_name));
  EXPECT_EQ(6, GetSize(file_name));

  EXPECT_EQ(base::File::FILE_OK, NativeFileUtil::Truncate(content_uri, 0));
  EXPECT_TRUE(FileExists(file_name));
  EXPECT_EQ(0, GetSize(file_name));
#endif
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
                from_file, to_file1, FileSystemOperation::CopyOrMoveOptionSet(),
                nosync));

  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::CopyOrMoveFile(
                from_file, to_file2, FileSystemOperation::CopyOrMoveOptionSet(),
                sync));

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
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::CopyOrMoveFile(
                from_file, to_dir_file,
                FileSystemOperation::CopyOrMoveOptionSet(), nosync));
  EXPECT_TRUE(FileExists(to_dir_file));
  EXPECT_EQ(1020, GetSize(to_dir_file));

  // Following tests are error checking.
  // Source doesn't exist.
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            NativeFileUtil::CopyOrMoveFile(
                Path("nonexists"), Path("file"),
                FileSystemOperation::CopyOrMoveOptionSet(), nosync));

  // Source is not a file.
  EXPECT_EQ(base::File::FILE_ERROR_NOT_A_FILE,
            NativeFileUtil::CopyOrMoveFile(
                dir, Path("file"), FileSystemOperation::CopyOrMoveOptionSet(),
                nosync));
  // Destination is not a file.
  EXPECT_EQ(
      base::File::FILE_ERROR_INVALID_OPERATION,
      NativeFileUtil::CopyOrMoveFile(
          from_file, dir, FileSystemOperation::CopyOrMoveOptionSet(), nosync));
  // Destination's parent doesn't exist.
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            NativeFileUtil::CopyOrMoveFile(
                from_file, Path("nodir").AppendASCII("file"),
                FileSystemOperation::CopyOrMoveOptionSet(), nosync));
  // Destination's parent is a file.
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            NativeFileUtil::CopyOrMoveFile(
                from_file, Path("tofile1").AppendASCII("file"),
                FileSystemOperation::CopyOrMoveOptionSet(), nosync));
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
                from_file, to_file, FileSystemOperation::CopyOrMoveOptionSet(),
                move));

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
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::CopyOrMoveFile(
                from_file, to_dir_file,
                FileSystemOperation::CopyOrMoveOptionSet(), move));
  EXPECT_FALSE(FileExists(from_file));
  EXPECT_TRUE(FileExists(to_dir_file));
  EXPECT_EQ(1020, GetSize(to_dir_file));

  // Following is error checking.
  // Source doesn't exist.
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            NativeFileUtil::CopyOrMoveFile(
                Path("nonexists"), Path("file"),
                FileSystemOperation::CopyOrMoveOptionSet(), move));

  base::FilePath dir2 = Path("dir2");
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::CreateDirectory(dir2, false, false));
  ASSERT_TRUE(base::DirectoryExists(dir2));
  // Source is a directory, destination is a file.
  EXPECT_EQ(
      base::File::FILE_ERROR_INVALID_OPERATION,
      NativeFileUtil::CopyOrMoveFile(
          dir, to_file, FileSystemOperation::CopyOrMoveOptionSet(), move));

#if BUILDFLAG(IS_WIN)
  // Source is a directory, destination is a directory.
  EXPECT_EQ(base::File::FILE_ERROR_NOT_A_FILE,
            NativeFileUtil::CopyOrMoveFile(
                dir, dir2, FileSystemOperation::CopyOrMoveOptionSet(), move));
#endif

  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::EnsureFileExists(from_file, &created));
  ASSERT_TRUE(FileExists(from_file));
  // Destination is not a file.
  EXPECT_EQ(
      base::File::FILE_ERROR_INVALID_OPERATION,
      NativeFileUtil::CopyOrMoveFile(
          from_file, dir, FileSystemOperation::CopyOrMoveOptionSet(), move));

  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::EnsureFileExists(from_file, &created));
  ASSERT_TRUE(FileExists(from_file));
  // Destination's parent doesn't exist.
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            NativeFileUtil::CopyOrMoveFile(
                from_file, Path("nodir").AppendASCII("file"),
                FileSystemOperation::CopyOrMoveOptionSet(), move));
  // Destination's parent is a file.
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            NativeFileUtil::CopyOrMoveFile(
                from_file, Path("tofile1").AppendASCII("file"),
                FileSystemOperation::CopyOrMoveOptionSet(), move));
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

  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::CopyOrMoveFile(
                from_directory, to_directory,
                FileSystemOperation::CopyOrMoveOptionSet(), move));

  EXPECT_FALSE(base::DirectoryExists(from_directory));
  EXPECT_FALSE(FileExists(from_file));
  EXPECT_TRUE(base::DirectoryExists(to_directory));
  EXPECT_TRUE(FileExists(to_file));
  EXPECT_EQ(1020, GetSize(to_file));
}

#if !BUILDFLAG(IS_WIN)
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

  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::CopyOrMoveFile(
                from_directory, to_directory,
                FileSystemOperation::CopyOrMoveOptionSet(), move));

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
                from_file, to_file1, {CopyOrMoveOption::kPreserveLastModified},
                NativeFileUtil::COPY_NOSYNC));

  base::File::Info file_info2;
  ASSERT_TRUE(FileExists(to_file1));
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::GetFileInfo(to_file1, &file_info2));
  EXPECT_EQ(file_info1.last_modified, file_info2.last_modified);

  // Test for copy (sync).
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::CopyOrMoveFile(
                from_file, to_file2, {CopyOrMoveOption::kPreserveLastModified},
                NativeFileUtil::COPY_SYNC));

  ASSERT_TRUE(FileExists(to_file2));
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::GetFileInfo(to_file1, &file_info2));
  EXPECT_EQ(file_info1.last_modified, file_info2.last_modified);

  // Test for move.
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::CopyOrMoveFile(
                from_file, to_file3, {CopyOrMoveOption::kPreserveLastModified},
                NativeFileUtil::MOVE));

  ASSERT_TRUE(FileExists(to_file3));
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::GetFileInfo(to_file2, &file_info2));
  EXPECT_EQ(file_info1.last_modified, file_info2.last_modified);
}

// This test is disabled on Fuchsia because file permissions are not supported.
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_WIN)
TEST_F(NativeFileUtilTest, PreserveDestinationPermissions) {
  // Ensure both the src and dest files exist.
  base::FilePath to_file = Path("to-file");
  base::FilePath from_file = Path("from-file1");
  bool created = false;
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::EnsureFileExists(to_file, &created));
  ASSERT_TRUE(created);
  EXPECT_TRUE(FileExists(to_file));

  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);
  EXPECT_TRUE(FileExists(from_file));

#if BUILDFLAG(IS_POSIX)
  int dest_initial_mode;
  ASSERT_TRUE(base::GetPosixFilePermissions(to_file, &dest_initial_mode));
#elif BUILDFLAG(IS_WIN)
  DWORD dest_initial_attributes = ::GetFileAttributes(to_file.value().c_str());
  ASSERT_NE(dest_initial_attributes, INVALID_FILE_ATTRIBUTES);
#endif  // BUILDFLAG(IS_POSIX)

  // Give dest file some distinct permissions it didn't have before.
#if BUILDFLAG(IS_POSIX)
  int old_dest_mode = dest_initial_mode | S_IRGRP | S_IXOTH;
  EXPECT_NE(old_dest_mode, dest_initial_mode);
  EXPECT_TRUE(base::SetPosixFilePermissions(to_file, old_dest_mode));
#elif BUILDFLAG(IS_WIN)
  DWORD old_dest_attributes = FILE_ATTRIBUTE_NORMAL;
  EXPECT_NE(old_dest_attributes, dest_initial_attributes);
  EXPECT_TRUE(
      ::SetFileAttributes(to_file.value().c_str(), old_dest_attributes));
#endif  // BUILDFLAG(IS_POSIX)

  // Test for copy (nosync).
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::CopyOrMoveFile(
                from_file, to_file,
                {CopyOrMoveOption::kPreserveDestinationPermissions},
                NativeFileUtil::COPY_NOSYNC));
#if BUILDFLAG(IS_POSIX)
  ExpectFileHasPermissionsPosix(to_file, old_dest_mode);
#elif BUILDFLAG(IS_WIN)
  ExpectFileHasPermissionsWin(to_file, old_dest_attributes);
#endif  // BUILDFLAG(IS_POSIX)

  // Test for copy (sync).
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::CopyOrMoveFile(
                from_file, to_file,
                {CopyOrMoveOption::kPreserveDestinationPermissions},
                NativeFileUtil::COPY_SYNC));
#if BUILDFLAG(IS_POSIX)
  ExpectFileHasPermissionsPosix(to_file, old_dest_mode);
#elif BUILDFLAG(IS_WIN)
  ExpectFileHasPermissionsWin(to_file, old_dest_attributes);
#endif  // BUILDFLAG(IS_POSIX)

  // Test for move.
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::CopyOrMoveFile(
                from_file, to_file,
                {CopyOrMoveOption::kPreserveDestinationPermissions},
                NativeFileUtil::MOVE));
#if BUILDFLAG(IS_POSIX)
  ExpectFileHasPermissionsPosix(to_file, old_dest_mode);
#elif BUILDFLAG(IS_WIN)
  ExpectFileHasPermissionsWin(to_file, old_dest_attributes);
#endif  // BUILDFLAG(IS_POSIX)
}
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_WIN)

// This test is disabled on Fuchsia because file permissions are not supported.
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_WIN)
TEST_F(NativeFileUtilTest, PreserveLastModifiedAndDestinationPermissions) {
  base::FilePath from_file = Path("fromfile");
  base::FilePath to_file1 = Path("tofile1");
  base::FilePath to_file2 = Path("tofile2");
  base::FilePath to_file3 = Path("tofile3");
  bool created = false;
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);
  EXPECT_TRUE(FileExists(from_file));

  base::File::Info from_file_info;
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::GetFileInfo(from_file, &from_file_info));

  // Create destination files.
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::EnsureFileExists(to_file1, &created));
  ASSERT_TRUE(created);
  EXPECT_TRUE(FileExists(to_file1));

  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::EnsureFileExists(to_file2, &created));
  ASSERT_TRUE(created);
  EXPECT_TRUE(FileExists(to_file2));

  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::EnsureFileExists(to_file3, &created));
  ASSERT_TRUE(created);
  EXPECT_TRUE(FileExists(to_file3));

  // Get initial permissions of the dest files. We can assume that the 3
  // destination files have the same permissions.
#if BUILDFLAG(IS_POSIX)
  int dest_initial_mode;
  ASSERT_TRUE(base::GetPosixFilePermissions(to_file1, &dest_initial_mode));
#elif BUILDFLAG(IS_WIN)
  DWORD dest_initial_attributes = ::GetFileAttributes(to_file1.value().c_str());
  ASSERT_NE(dest_initial_attributes, INVALID_FILE_ATTRIBUTES);
#endif  // BUILDFLAG(IS_POSIX)

  // Give dest files some distinct permissions they didn't have before.
#if BUILDFLAG(IS_POSIX)
  int old_dest_mode = dest_initial_mode | S_IRGRP | S_IXOTH;
  EXPECT_NE(old_dest_mode, dest_initial_mode);
  EXPECT_TRUE(base::SetPosixFilePermissions(to_file1, old_dest_mode));
  EXPECT_TRUE(base::SetPosixFilePermissions(to_file2, old_dest_mode));
  EXPECT_TRUE(base::SetPosixFilePermissions(to_file3, old_dest_mode));
#elif BUILDFLAG(IS_WIN)
  DWORD old_dest_attributes = FILE_ATTRIBUTE_NORMAL;
  EXPECT_NE(old_dest_attributes, dest_initial_attributes);
  EXPECT_TRUE(
      ::SetFileAttributes(to_file1.value().c_str(), old_dest_attributes));
  EXPECT_TRUE(
      ::SetFileAttributes(to_file2.value().c_str(), old_dest_attributes));
  EXPECT_TRUE(
      ::SetFileAttributes(to_file3.value().c_str(), old_dest_attributes));
#endif  // BUILDFLAG(IS_POSIX)

  // Test for copy (nosync).
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::CopyOrMoveFile(
                from_file, to_file1,
                {CopyOrMoveOption::kPreserveLastModified,
                 CopyOrMoveOption::kPreserveDestinationPermissions},
                NativeFileUtil::COPY_NOSYNC));
  base::File::Info to_file_info;
  ASSERT_TRUE(FileExists(to_file1));
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::GetFileInfo(to_file1, &to_file_info));
  EXPECT_EQ(from_file_info.last_modified, to_file_info.last_modified);

#if BUILDFLAG(IS_POSIX)
  ExpectFileHasPermissionsPosix(to_file1, old_dest_mode);
#elif BUILDFLAG(IS_WIN)
  ExpectFileHasPermissionsWin(to_file1, old_dest_attributes);
#endif  // BUILDFLAG(IS_POSIX)

  // Test for copy (sync).
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::CopyOrMoveFile(
                from_file, to_file2,
                {CopyOrMoveOption::kPreserveLastModified,
                 CopyOrMoveOption::kPreserveDestinationPermissions},
                NativeFileUtil::COPY_SYNC));
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::GetFileInfo(to_file2, &to_file_info));
  EXPECT_EQ(from_file_info.last_modified, to_file_info.last_modified);

#if BUILDFLAG(IS_POSIX)
  ExpectFileHasPermissionsPosix(to_file2, old_dest_mode);
#elif BUILDFLAG(IS_WIN)
  ExpectFileHasPermissionsWin(to_file2, old_dest_attributes);
#endif  // BUILDFLAG(IS_POSIX)

  // Test for move.
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::CopyOrMoveFile(
                from_file, to_file3,
                {CopyOrMoveOption::kPreserveLastModified,
                 CopyOrMoveOption::kPreserveDestinationPermissions},
                NativeFileUtil::MOVE));
  ASSERT_EQ(base::File::FILE_OK,
            NativeFileUtil::GetFileInfo(to_file3, &to_file_info));
  EXPECT_EQ(from_file_info.last_modified, to_file_info.last_modified);

#if BUILDFLAG(IS_POSIX)
  ExpectFileHasPermissionsPosix(to_file3, old_dest_mode);
#elif BUILDFLAG(IS_WIN)
  ExpectFileHasPermissionsWin(to_file3, old_dest_attributes);
#endif  // BUILDFLAG(IS_POSIX)
}
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_WIN)

}  // namespace storage
