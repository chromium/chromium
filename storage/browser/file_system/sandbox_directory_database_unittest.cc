// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/sandbox_directory_database.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "storage/browser/test/sandbox_database_test_helper.h"
#include "storage/common/file_system/file_system_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

#define FPL(x) FILE_PATH_LITERAL(x)

namespace storage {

namespace {
const base::FilePath::CharType kDirectoryDatabaseName[] = FPL("Paths");
}

class SandboxDirectoryDatabaseTest : public testing::Test {
 public:
  using FileId = SandboxDirectoryDatabase::FileId;
  using FileInfo = SandboxDirectoryDatabase::FileInfo;

  SandboxDirectoryDatabaseTest() {
    EXPECT_TRUE(base_.CreateUniqueTempDir());
    InitDatabase();
  }

  SandboxDirectoryDatabaseTest(const SandboxDirectoryDatabaseTest&) = delete;
  SandboxDirectoryDatabaseTest& operator=(const SandboxDirectoryDatabaseTest&) =
      delete;

  SandboxDirectoryDatabase* db() { return db_.get(); }

  void InitDatabase() {
    // Call CloseDatabase() to avoid having multiple database instances for
    // single directory at once.
    CloseDatabase();
    db_ = std::make_unique<SandboxDirectoryDatabase>(path(), nullptr);
  }

  void CloseDatabase() { db_.reset(); }

  base::File::Error AddFileInfo(FileId parent_id,
                                const base::FilePath::StringType& name) {
    FileId file_id;
    FileInfo info;
    info.parent_id = parent_id;
    info.name = name;
    return db_->AddFileInfo(info, &file_id);
  }

  void CreateDirectory(FileId parent_id,
                       const base::FilePath::StringType& name,
                       FileId* file_id_out) {
    FileInfo info;
    info.parent_id = parent_id;
    info.name = name;
    ASSERT_EQ(base::File::FILE_OK, db_->AddFileInfo(info, file_id_out));
  }

  void CreateFile(FileId parent_id,
                  const base::FilePath::StringType& name,
                  const base::FilePath::StringType& data_path,
                  FileId* file_id_out) {
    FileId file_id;

    FileInfo info;
    info.parent_id = parent_id;
    info.name = name;
    info.data_path = base::FilePath(data_path).NormalizePathSeparators();
    ASSERT_EQ(base::File::FILE_OK, db_->AddFileInfo(info, &file_id));

    base::FilePath local_path = path().Append(data_path);
    if (!base::DirectoryExists(local_path.DirName()))
      ASSERT_TRUE(base::CreateDirectory(local_path.DirName()));

    base::File file(local_path,
                    base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(file.IsValid());
    ASSERT_TRUE(file.created());

    if (file_id_out)
      *file_id_out = file_id;
  }

  void ClearDatabaseAndDirectory() {
    db_.reset();
    ASSERT_TRUE(base::DeletePathRecursively(path()));
    ASSERT_TRUE(base::CreateDirectory(path()));
    db_ = std::make_unique<SandboxDirectoryDatabase>(path(), nullptr);
  }

  bool RepairDatabase() {
    return db()->RepairDatabase(
        FilePathToString(path().Append(kDirectoryDatabaseName)));
  }

  const base::FilePath& path() { return base_.GetPath(); }

  // Makes link from |parent_id| to |child_id| with |name|.
  void MakeHierarchyLink(FileId parent_id,
                         FileId child_id,
                         const base::FilePath::StringType& name) {
    ASSERT_TRUE(db()->db_
                    ->Put(leveldb::WriteOptions(),
                          "CHILD_OF:" + base::NumberToString(parent_id) + ":" +
                              FilePathToString(base::FilePath(name)),
                          base::NumberToString(child_id))
                    .ok());
  }

  // Deletes link from parent of |file_id| to |file_id|.
  void DeleteHierarchyLink(FileId file_id) {
    FileInfo file_info;
    ASSERT_TRUE(db()->GetFileInfo(file_id, &file_info));
    ASSERT_TRUE(
        db()->db_
            ->Delete(leveldb::WriteOptions(),
                     "CHILD_OF:" + base::NumberToString(file_info.parent_id) +
                         ":" + FilePathToString(base::FilePath(file_info.name)))
            .ok());
  }

 protected:
  // Common temp base for nondestructive uses.
  base::ScopedTempDir base_;
  std::unique_ptr<SandboxDirectoryDatabase> db_;
};

TEST_F(SandboxDirectoryDatabaseTest, TestMissingFileGetInfo) {
  FileId file_id = 888;
  FileInfo info;
  EXPECT_FALSE(db()->GetFileInfo(file_id, &info));
}

TEST_F(SandboxDirectoryDatabaseTest, TestGetRootFileInfoBeforeCreate) {
  FileId file_id = 0;
  FileInfo info;
  EXPECT_TRUE(db()->GetFileInfo(file_id, &info));
  EXPECT_EQ(0, info.parent_id);
  EXPECT_TRUE(info.name.empty());
  EXPECT_TRUE(info.data_path.empty());
}

TEST_F(SandboxDirectoryDatabaseTest, TestMissingParentAddFileInfo) {
  FileId parent_id = 7;
  EXPECT_EQ(base::File::FILE_ERROR_NOT_A_DIRECTORY,
            AddFileInfo(parent_id, FILE_PATH_LITERAL("foo")));
}

TEST_F(SandboxDirectoryDatabaseTest, TestAddNameClash) {
  FileInfo info;
  FileId file_id;
  info.parent_id = 0;
  info.name = FILE_PATH_LITERAL("dir 0");
  EXPECT_EQ(base::File::FILE_OK, db()->AddFileInfo(info, &file_id));

  // Check for name clash in the root directory.
  base::FilePath::StringType name = info.name;
  EXPECT_EQ(base::File::FILE_ERROR_EXISTS, AddFileInfo(0, name));
  name = FILE_PATH_LITERAL("dir 1");
  EXPECT_EQ(base::File::FILE_OK, AddFileInfo(0, name));

  name = FILE_PATH_LITERAL("subdir 0");
  EXPECT_EQ(base::File::FILE_OK, AddFileInfo(file_id, name));

  // Check for name clash in a subdirectory.
  EXPECT_EQ(base::File::FILE_ERROR_EXISTS, AddFileInfo(file_id, name));
  name = FILE_PATH_LITERAL("subdir 1");
  EXPECT_EQ(base::File::FILE_OK, AddFileInfo(file_id, name));
}

TEST_F(SandboxDirectoryDatabaseTest, TestRenameNoMoveNameClash) {
  FileInfo info;
  FileId file_id0;
  base::FilePath::StringType name0 = FILE_PATH_LITERAL("foo");
  base::FilePath::StringType name1 = FILE_PATH_LITERAL("bar");
  base::FilePath::StringType name2 = FILE_PATH_LITERAL("bas");
  info.parent_id = 0;
  info.name = name0;
  EXPECT_EQ(base::File::FILE_OK, db()->AddFileInfo(info, &file_id0));
  EXPECT_EQ(base::File::FILE_OK, AddFileInfo(0, name1));
  info.name = name1;
  EXPECT_FALSE(db()->UpdateFileInfo(file_id0, info));
  info.name = name2;
  EXPECT_TRUE(db()->UpdateFileInfo(file_id0, info));
}

TEST_F(SandboxDirectoryDatabaseTest, TestMoveSameNameNameClash) {
  FileInfo info;
  FileId file_id0;
  FileId file_id1;
  base::FilePath::StringType name0 = FILE_PATH_LITERAL("foo");
  base::FilePath::StringType name1 = FILE_PATH_LITERAL("bar");
  info.parent_id = 0;
  info.name = name0;
  EXPECT_EQ(base::File::FILE_OK, db()->AddFileInfo(info, &file_id0));
  info.parent_id = file_id0;
  EXPECT_EQ(base::File::FILE_OK, db()->AddFileInfo(info, &file_id1));
  info.parent_id = 0;
  EXPECT_FALSE(db()->UpdateFileInfo(file_id1, info));
  info.name = name1;
  EXPECT_TRUE(db()->UpdateFileInfo(file_id1, info));
}

TEST_F(SandboxDirectoryDatabaseTest, TestMoveRenameNameClash) {
  FileInfo info;
  FileId file_id0;
  FileId file_id1;
  base::FilePath::StringType name0 = FILE_PATH_LITERAL("foo");
  base::FilePath::StringType name1 = FILE_PATH_LITERAL("bar");
  base::FilePath::StringType name2 = FILE_PATH_LITERAL("bas");
  info.parent_id = 0;
  info.name = name0;
  EXPECT_EQ(base::File::FILE_OK, db()->AddFileInfo(info, &file_id0));
  info.parent_id = file_id0;
  info.name = name1;
  EXPECT_EQ(base::File::FILE_OK, db()->AddFileInfo(info, &file_id1));
  info.parent_id = 0;
  info.name = name0;
  EXPECT_FALSE(db()->UpdateFileInfo(file_id1, info));
  info.name = name1;
  EXPECT_TRUE(db()->UpdateFileInfo(file_id1, info));
  // Also test a successful move+rename.
  info.parent_id = file_id0;
  info.name = name2;
  EXPECT_TRUE(db()->UpdateFileInfo(file_id1, info));
}

TEST_F(SandboxDirectoryDatabaseTest, TestRemoveWithChildren) {
  FileInfo info;
  FileId file_id0;
  FileId file_id1;
  info.parent_id = 0;
  info.name = FILE_PATH_LITERAL("foo");
  EXPECT_EQ(base::File::FILE_OK, db()->AddFileInfo(info, &file_id0));
  info.parent_id = file_id0;
  EXPECT_EQ(base::File::FILE_OK, db()->AddFileInfo(info, &file_id1));
  EXPECT_FALSE(db()->RemoveFileInfo(file_id0));
  EXPECT_TRUE(db()->RemoveFileInfo(file_id1));
  EXPECT_TRUE(db()->RemoveFileInfo(file_id0));
}

TEST_F(SandboxDirectoryDatabaseTest, TestGetChildWithName) {
  FileInfo info;
  FileId file_id0;
  FileId file_id1;
  base::FilePath::StringType name0 = FILE_PATH_LITERAL("foo");
  base::FilePath::StringType name1 = FILE_PATH_LITERAL("bar");
  info.parent_id = 0;
  info.name = name0;
  EXPECT_EQ(base::File::FILE_OK, db()->AddFileInfo(info, &file_id0));
  info.parent_id = file_id0;
  info.name = name1;
  EXPECT_EQ(base::File::FILE_OK, db()->AddFileInfo(info, &file_id1));
  EXPECT_NE(file_id0, file_id1);

  FileId check_file_id;
  EXPECT_FALSE(db()->GetChildWithName(0, name1, &check_file_id));
  EXPECT_TRUE(db()->GetChildWithName(0, name0, &check_file_id));
  EXPECT_EQ(file_id0, check_file_id);
  EXPECT_FALSE(db()->GetChildWithName(file_id0, name0, &check_file_id));
  EXPECT_TRUE(db()->GetChildWithName(file_id0, name1, &check_file_id));
  EXPECT_EQ(file_id1, check_file_id);
}

TEST_F(SandboxDirectoryDatabaseTest, TestGetFileWithPath) {
  FileInfo info;
  FileId file_id0;
  FileId file_id1;
  FileId file_id2;
  base::FilePath::StringType name0 = FILE_PATH_LITERAL("foo");
  base::FilePath::StringType name1 = FILE_PATH_LITERAL("bar");
  base::FilePath::StringType name2 = FILE_PATH_LITERAL("dog");

  info.parent_id = 0;
  info.name = name0;
  EXPECT_EQ(base::File::FILE_OK, db()->AddFileInfo(info, &file_id0));
  info.parent_id = file_id0;
  info.name = name1;
  EXPECT_EQ(base::File::FILE_OK, db()->AddFileInfo(info, &file_id1));
  EXPECT_NE(file_id0, file_id1);
  info.parent_id = file_id1;
  info.name = name2;
  EXPECT_EQ(base::File::FILE_OK, db()->AddFileInfo(info, &file_id2));
  EXPECT_NE(file_id0, file_id2);
  EXPECT_NE(file_id1, file_id2);

  FileId check_file_id;
  base::FilePath path = base::FilePath(name0);
  EXPECT_TRUE(db()->GetFileWithPath(path, &check_file_id));
  EXPECT_EQ(file_id0, check_file_id);

  path = path.Append(name1);
  EXPECT_TRUE(db()->GetFileWithPath(path, &check_file_id));
  EXPECT_EQ(file_id1, check_file_id);

  path = path.Append(name2);
  EXPECT_TRUE(db()->GetFileWithPath(path, &check_file_id));
  EXPECT_EQ(file_id2, check_file_id);
}

TEST_F(SandboxDirectoryDatabaseTest, TestListChildren) {
  // No children in the root.
  std::vector<FileId> children;
  EXPECT_TRUE(db()->ListChildren(0, &children));
  EXPECT_TRUE(children.empty());

  // One child in the root.
  FileId file_id0;
  FileInfo info;
  info.parent_id = 0;
  info.name = FILE_PATH_LITERAL("foo");
  EXPECT_EQ(base::File::FILE_OK, db()->AddFileInfo(info, &file_id0));
  EXPECT_TRUE(db()->ListChildren(0, &children));
  EXPECT_EQ(children.size(), 1UL);
  EXPECT_EQ(children[0], file_id0);

  // Two children in the root.
  FileId file_id1;
  info.name = FILE_PATH_LITERAL("bar");
  EXPECT_EQ(base::File::FILE_OK, db()->AddFileInfo(info, &file_id1));
  EXPECT_TRUE(db()->ListChildren(0, &children));
  EXPECT_EQ(2UL, children.size());
  if (children[0] == file_id0) {
    EXPECT_EQ(children[1], file_id1);
  } else {
    EXPECT_EQ(children[1], file_id0);
    EXPECT_EQ(children[0], file_id1);
  }

  // No children in a subdirectory.
  EXPECT_TRUE(db()->ListChildren(file_id0, &children));
  EXPECT_TRUE(children.empty());

  // One child in a subdirectory.
  info.parent_id = file_id0;
  info.name = FILE_PATH_LITERAL("foo");
  FileId file_id2;
  FileId file_id3;
  EXPECT_EQ(base::File::FILE_OK, db()->AddFileInfo(info, &file_id2));
  EXPECT_TRUE(db()->ListChildren(file_id0, &children));
  EXPECT_EQ(1UL, children.size());
  EXPECT_EQ(children[0], file_id2);

  // Two children in a subdirectory.
  info.name = FILE_PATH_LITERAL("bar");
  EXPECT_EQ(base::File::FILE_OK, db()->AddFileInfo(info, &file_id3));
  EXPECT_TRUE(db()->ListChildren(file_id0, &children));
  EXPECT_EQ(2UL, children.size());
  if (children[0] == file_id2) {
    EXPECT_EQ(children[1], file_id3);
  } else {
    EXPECT_EQ(children[1], file_id2);
    EXPECT_EQ(children[0], file_id3);
  }
}

TEST_F(SandboxDirectoryDatabaseTest, TestUpdateModificationTime) {
  FileInfo info0;
  FileId file_id;
  info0.parent_id = 0;
  info0.name = FILE_PATH_LITERAL("name");
  info0.data_path = base::FilePath(FILE_PATH_LITERAL("fake path"));
  info0.modification_time = base::Time::Now();
  EXPECT_EQ(base::File::FILE_OK, db()->AddFileInfo(info0, &file_id));
  FileInfo info1;
  EXPECT_TRUE(db()->GetFileInfo(file_id, &info1));
  EXPECT_EQ(info0.name, info1.name);
  EXPECT_EQ(info0.parent_id, info1.parent_id);
  EXPECT_EQ(info0.data_path, info1.data_path);
  EXPECT_EQ(floor(info0.modification_time.InSecondsFSinceUnixEpoch()),
            info1.modification_time.InSecondsFSinceUnixEpoch());

  EXPECT_TRUE(db()->UpdateModificationTime(file_id, base::Time::UnixEpoch()));
  EXPECT_TRUE(db()->GetFileInfo(file_id, &info1));
  EXPECT_EQ(info0.name, info1.name);
  EXPECT_EQ(info0.parent_id, info1.parent_id);
  EXPECT_EQ(info0.data_path, info1.data_path);
  EXPECT_NE(info0.modification_time, info1.modification_time);
  EXPECT_EQ(info1.modification_time.InSecondsFSinceUnixEpoch(),
            floor(base::Time::UnixEpoch().InSecondsFSinceUnixEpoch()));

  EXPECT_FALSE(db()->UpdateModificationTime(999, base::Time::UnixEpoch()));
}

TEST_F(SandboxDirectoryDatabaseTest, TestSimpleFileOperations) {
  FileId file_id = 888;
  FileInfo info0;
  EXPECT_FALSE(db()->GetFileInfo(file_id, &info0));
  info0.parent_id = 0;
  info0.data_path = base::FilePath(FILE_PATH_LITERAL("foo"));
  info0.name = FILE_PATH_LITERAL("file name");
  info0.modification_time = base::Time::Now();
  EXPECT_EQ(base::File::FILE_OK, db()->AddFileInfo(info0, &file_id));
  FileInfo info1;
  EXPECT_TRUE(db()->GetFileInfo(file_id, &info1));
  EXPECT_EQ(info0.parent_id, info1.parent_id);
  EXPECT_EQ(info0.data_path, info1.data_path);
  EXPECT_EQ(info0.name, info1.name);
  EXPECT_EQ(floor(info0.modification_time.InSecondsFSinceUnixEpoch()),
            info1.modification_time.InSecondsFSinceUnixEpoch());
}

TEST_F(SandboxDirectoryDatabaseTest, TestOverwritingMoveFileSrcDirectory) {
  FileId directory_id;
  FileInfo info0;
  info0.parent_id = 0;
  info0.name = FILE_PATH_LITERAL("directory");
  info0.modification_time = base::Time::Now();
  EXPECT_EQ(base::File::FILE_OK, db()->AddFileInfo(info0, &directory_id));

  FileId file_id;
  FileInfo info1;
  info1.parent_id = 0;
  info1.data_path = base::FilePath(FILE_PATH_LITERAL("bar"));
  info1.name = FILE_PATH_LITERAL("file");
  info1.modification_time = base::Time::UnixEpoch();
  EXPECT_EQ(base::File::FILE_OK, db()->AddFileInfo(info1, &file_id));

  EXPECT_FALSE(db()->OverwritingMoveFile(directory_id, file_id));
}

TEST_F(SandboxDirectoryDatabaseTest, TestOverwritingMoveFileDestDirectory) {
  FileId file_id;
  FileInfo info0;
  info0.parent_id = 0;
  info0.name = FILE_PATH_LITERAL("file");
  info0.data_path = base::FilePath(FILE_PATH_LITERAL("bar"));
  info0.modification_time = base::Time::Now();
  EXPECT_EQ(base::File::FILE_OK, db()->AddFileInfo(info0, &file_id));

  FileId directory_id;
  FileInfo info1;
  info1.parent_id = 0;
  info1.name = FILE_PATH_LITERAL("directory");
  info1.modification_time = base::Time::UnixEpoch();
  EXPECT_EQ(base::File::FILE_OK, db()->AddFileInfo(info1, &directory_id));

  EXPECT_FALSE(db()->OverwritingMoveFile(file_id, directory_id));
}

TEST_F(SandboxDirectoryDatabaseTest, TestOverwritingMoveFileSuccess) {
  FileId file_id0;
  FileInfo info0;
  info0.parent_id = 0;
  info0.data_path = base::FilePath(FILE_PATH_LITERAL("foo"));
  info0.name = FILE_PATH_LITERAL("file name 0");
  info0.modification_time = base::Time::Now();
  EXPECT_EQ(base::File::FILE_OK, db()->AddFileInfo(info0, &file_id0));

  FileInfo dir_info;
  FileId dir_id;
  dir_info.parent_id = 0;
  dir_info.name = FILE_PATH_LITERAL("directory name");
  EXPECT_EQ(base::File::FILE_OK, db()->AddFileInfo(dir_info, &dir_id));

  FileId file_id1;
  FileInfo info1;
  info1.parent_id = dir_id;
  info1.data_path = base::FilePath(FILE_PATH_LITERAL("bar"));
  info1.name = FILE_PATH_LITERAL("file name 1");
  info1.modification_time = base::Time::UnixEpoch();
  EXPECT_EQ(base::File::FILE_OK, db()->AddFileInfo(info1, &file_id1));

  EXPECT_TRUE(db()->OverwritingMoveFile(file_id0, file_id1));

  FileInfo check_info;
  FileId check_id;

  EXPECT_FALSE(db()->GetFileWithPath(base::FilePath(info0.name), &check_id));
  EXPECT_TRUE(db()->GetFileWithPath(
      base::FilePath(dir_info.name).Append(info1.name), &check_id));
  EXPECT_TRUE(db()->GetFileInfo(check_id, &check_info));

  EXPECT_EQ(info0.data_path, check_info.data_path);
}

TEST_F(SandboxDirectoryDatabaseTest, TestGetNextInteger) {
  int64_t next = -1;
  EXPECT_TRUE(db()->GetNextInteger(&next));
  EXPECT_EQ(0, next);
  EXPECT_TRUE(db()->GetNextInteger(&next));
  EXPECT_EQ(1, next);
  InitDatabase();
  EXPECT_TRUE(db()->GetNextInteger(&next));
  EXPECT_EQ(2, next);
  EXPECT_TRUE(db()->GetNextInteger(&next));
  EXPECT_EQ(3, next);
  InitDatabase();
  EXPECT_TRUE(db()->GetNextInteger(&next));
  EXPECT_EQ(4, next);
}

TEST_F(SandboxDirectoryDatabaseTest, TestConsistencyCheck_Empty) {
  EXPECT_TRUE(db()->IsFileSystemConsistent());

  int64_t next = -1;
  EXPECT_TRUE(db()->GetNextInteger(&next));
  EXPECT_EQ(0, next);
  EXPECT_TRUE(db()->IsFileSystemConsistent());
}

TEST_F(SandboxDirectoryDatabaseTest, TestConsistencyCheck_Consistent) {
  FileId dir_id;
  CreateFile(0, FPL("foo"), FPL("hoge"), nullptr);
  CreateDirectory(0, FPL("bar"), &dir_id);
  CreateFile(dir_id, FPL("baz"), FPL("fuga"), nullptr);
  CreateFile(dir_id, FPL("fizz"), FPL("buzz"), nullptr);

  EXPECT_TRUE(db()->IsFileSystemConsistent());
}

TEST_F(SandboxDirectoryDatabaseTest, TestConsistencyCheck_BackingMultiEntry) {
  const base::FilePath::CharType kBackingFileName[] = FPL("the celeb");
  CreateFile(0, FPL("foo"), kBackingFileName, nullptr);

  EXPECT_TRUE(db()->IsFileSystemConsistent());
  ASSERT_TRUE(base::DeleteFile(path().Append(kBackingFileName)));
  CreateFile(0, FPL("bar"), kBackingFileName, nullptr);
  EXPECT_FALSE(db()->IsFileSystemConsistent());
}

TEST_F(SandboxDirectoryDatabaseTest, TestConsistencyCheck_FileLost) {
  const base::FilePath::CharType kBackingFileName[] = FPL("hoge");
  CreateFile(0, FPL("foo"), kBackingFileName, nullptr);

  EXPECT_TRUE(db()->IsFileSystemConsistent());
  ASSERT_TRUE(base::DeleteFile(path().Append(kBackingFileName)));
  EXPECT_TRUE(db()->IsFileSystemConsistent());
}

TEST_F(SandboxDirectoryDatabaseTest, TestConsistencyCheck_OrphanFile) {
  CreateFile(0, FPL("foo"), FPL("hoge"), nullptr);

  EXPECT_TRUE(db()->IsFileSystemConsistent());

  base::File file(path().Append(FPL("Orphan File")),
                  base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  ASSERT_TRUE(file.IsValid());
  ASSERT_TRUE(file.created());
  file.Close();

  EXPECT_TRUE(db()->IsFileSystemConsistent());
}

TEST_F(SandboxDirectoryDatabaseTest, TestConsistencyCheck_RootLoop) {
  EXPECT_TRUE(db()->IsFileSystemConsistent());
  MakeHierarchyLink(0, 0, base::FilePath::StringType());
  EXPECT_FALSE(db()->IsFileSystemConsistent());
}

TEST_F(SandboxDirectoryDatabaseTest, TestConsistencyCheck_DirectoryLoop) {
  FileId dir1_id;
  FileId dir2_id;
  base::FilePath::StringType dir1_name = FPL("foo");
  CreateDirectory(0, dir1_name, &dir1_id);
  CreateDirectory(dir1_id, FPL("bar"), &dir2_id);

  EXPECT_TRUE(db()->IsFileSystemConsistent());
  MakeHierarchyLink(dir2_id, dir1_id, dir1_name);
  EXPECT_FALSE(db()->IsFileSystemConsistent());
}

TEST_F(SandboxDirectoryDatabaseTest, TestConsistencyCheck_NameMismatch) {
  FileId dir_id;
  FileId file_id;
  CreateDirectory(0, FPL("foo"), &dir_id);
  CreateFile(dir_id, FPL("bar"), FPL("hoge/fuga/piyo"), &file_id);

  EXPECT_TRUE(db()->IsFileSystemConsistent());
  DeleteHierarchyLink(file_id);
  MakeHierarchyLink(dir_id, file_id, FPL("baz"));
  EXPECT_FALSE(db()->IsFileSystemConsistent());
}

TEST_F(SandboxDirectoryDatabaseTest, TestConsistencyCheck_WreckedEntries) {
  FileId dir1_id;
  FileId dir2_id;
  CreateDirectory(0, FPL("foo"), &dir1_id);
  CreateDirectory(dir1_id, FPL("bar"), &dir2_id);
  CreateFile(dir2_id, FPL("baz"), FPL("fizz/buzz"), nullptr);

  EXPECT_TRUE(db()->IsFileSystemConsistent());
  DeleteHierarchyLink(dir2_id);  // Delete link from |dir1_id| to |dir2_id|.
  EXPECT_FALSE(db()->IsFileSystemConsistent());
}

TEST_F(SandboxDirectoryDatabaseTest, TestRepairDatabase_Success) {
  base::FilePath::StringType kFileName = FPL("bar");

  FileId file_id_prev;
  CreateFile(0, FPL("foo"), FPL("hoge"), nullptr);
  CreateFile(0, kFileName, FPL("fuga"), &file_id_prev);

  const base::FilePath kDatabaseDirectory =
      path().Append(kDirectoryDatabaseName);
  CloseDatabase();
  CorruptDatabase(kDatabaseDirectory, leveldb::kDescriptorFile, 0,
                  std::numeric_limits<size_t>::max());
  InitDatabase();
  EXPECT_FALSE(db()->IsFileSystemConsistent());

  FileId file_id;
  EXPECT_TRUE(db()->GetChildWithName(0, kFileName, &file_id));
  EXPECT_EQ(file_id_prev, file_id);

  EXPECT_TRUE(db()->IsFileSystemConsistent());
}

TEST_F(SandboxDirectoryDatabaseTest, TestRepairDatabase_Failure) {
  base::FilePath::StringType kFileName = FPL("bar");

  CreateFile(0, FPL("foo"), FPL("hoge"), nullptr);
  CreateFile(0, kFileName, FPL("fuga"), nullptr);

  const base::FilePath kDatabaseDirectory =
      path().Append(kDirectoryDatabaseName);
  CloseDatabase();
  CorruptDatabase(kDatabaseDirectory, leveldb::kDescriptorFile, 0,
                  std::numeric_limits<size_t>::max());
  CorruptDatabase(kDatabaseDirectory, leveldb::kLogFile, -1, 1);
  InitDatabase();
  EXPECT_FALSE(db()->IsFileSystemConsistent());

  FileId file_id;
  EXPECT_FALSE(db()->GetChildWithName(0, kFileName, &file_id));
  EXPECT_TRUE(db()->IsFileSystemConsistent());
}

TEST_F(SandboxDirectoryDatabaseTest, TestRepairDatabase_MissingManifest) {
  base::FilePath::StringType kFileName = FPL("bar");

  FileId file_id_prev;
  CreateFile(0, FPL("foo"), FPL("hoge"), nullptr);
  CreateFile(0, kFileName, FPL("fuga"), &file_id_prev);

  const base::FilePath kDatabaseDirectory =
      path().Append(kDirectoryDatabaseName);
  CloseDatabase();

  DeleteDatabaseFile(kDatabaseDirectory, leveldb::kDescriptorFile);

  InitDatabase();
  EXPECT_FALSE(db()->IsFileSystemConsistent());

  FileId file_id;
  EXPECT_TRUE(db()->GetChildWithName(0, kFileName, &file_id));
  EXPECT_EQ(file_id_prev, file_id);

  EXPECT_TRUE(db()->IsFileSystemConsistent());
}

}  // namespace storage
