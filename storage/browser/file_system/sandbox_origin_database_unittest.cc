// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "storage/browser/file_system/sandbox_origin_database.h"
#include "storage/browser/test/sandbox_database_test_helper.h"
#include "storage/common/file_system/file_system_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/db/filename.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

using storage::SandboxOriginDatabase;

namespace content {

namespace {
const base::FilePath::CharType kFileSystemDirName[] =
    FILE_PATH_LITERAL("File System");
const base::FilePath::CharType kOriginDatabaseName[] =
    FILE_PATH_LITERAL("Origins");
}  // namespace

TEST(SandboxOriginDatabaseTest, BasicTest) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  const base::FilePath kFSDir = dir.GetPath().Append(kFileSystemDirName);
  EXPECT_FALSE(base::PathExists(kFSDir));
  EXPECT_TRUE(base::CreateDirectory(kFSDir));

  SandboxOriginDatabase database(kFSDir, nullptr);
  std::string origin("origin");

  EXPECT_FALSE(database.HasOriginPath(origin));
  // Double-check to make sure that had no side effects.
  EXPECT_FALSE(database.HasOriginPath(origin));

  base::FilePath path0;
  base::FilePath path1;

  // Empty strings aren't valid origins.
  EXPECT_FALSE(database.GetPathForOrigin(std::string(), &path0));

  EXPECT_TRUE(database.GetPathForOrigin(origin, &path0));
  EXPECT_TRUE(database.HasOriginPath(origin));
  EXPECT_TRUE(database.GetPathForOrigin(origin, &path1));
  EXPECT_FALSE(path0.empty());
  EXPECT_FALSE(path1.empty());
  EXPECT_EQ(path0, path1);

  EXPECT_TRUE(base::PathExists(kFSDir.Append(kOriginDatabaseName)));
}

TEST(SandboxOriginDatabaseTest, TwoPathTest) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  const base::FilePath kFSDir = dir.GetPath().Append(kFileSystemDirName);
  EXPECT_FALSE(base::PathExists(kFSDir));
  EXPECT_TRUE(base::CreateDirectory(kFSDir));

  SandboxOriginDatabase database(kFSDir, nullptr);
  std::string origin0("origin0");
  std::string origin1("origin1");

  EXPECT_FALSE(database.HasOriginPath(origin0));
  EXPECT_FALSE(database.HasOriginPath(origin1));

  base::FilePath path0;
  base::FilePath path1;
  EXPECT_TRUE(database.GetPathForOrigin(origin0, &path0));
  EXPECT_TRUE(database.HasOriginPath(origin0));
  EXPECT_FALSE(database.HasOriginPath(origin1));
  EXPECT_TRUE(database.GetPathForOrigin(origin1, &path1));
  EXPECT_TRUE(database.HasOriginPath(origin1));
  EXPECT_FALSE(path0.empty());
  EXPECT_FALSE(path1.empty());
  EXPECT_NE(path0, path1);

  EXPECT_TRUE(base::PathExists(kFSDir.Append(kOriginDatabaseName)));
}

TEST(SandboxOriginDatabaseTest, DropDatabaseTest) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  const base::FilePath kFSDir = dir.GetPath().Append(kFileSystemDirName);
  EXPECT_FALSE(base::PathExists(kFSDir));
  EXPECT_TRUE(base::CreateDirectory(kFSDir));

  SandboxOriginDatabase database(kFSDir, nullptr);
  std::string origin("origin");

  EXPECT_FALSE(database.HasOriginPath(origin));

  base::FilePath path0;
  EXPECT_TRUE(database.GetPathForOrigin(origin, &path0));
  EXPECT_TRUE(database.HasOriginPath(origin));
  EXPECT_FALSE(path0.empty());

  EXPECT_TRUE(base::PathExists(kFSDir.Append(kOriginDatabaseName)));

  database.DropDatabase();

  base::FilePath path1;
  EXPECT_TRUE(database.HasOriginPath(origin));
  EXPECT_TRUE(database.GetPathForOrigin(origin, &path1));
  EXPECT_FALSE(path1.empty());
  EXPECT_EQ(path0, path1);
}

TEST(SandboxOriginDatabaseTest, DeleteOriginTest) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  const base::FilePath kFSDir = dir.GetPath().Append(kFileSystemDirName);
  EXPECT_FALSE(base::PathExists(kFSDir));
  EXPECT_TRUE(base::CreateDirectory(kFSDir));

  SandboxOriginDatabase database(kFSDir, nullptr);
  std::string origin("origin");

  EXPECT_FALSE(database.HasOriginPath(origin));
  EXPECT_TRUE(database.RemovePathForOrigin(origin));

  base::FilePath path0;
  EXPECT_TRUE(database.GetPathForOrigin(origin, &path0));
  EXPECT_TRUE(database.HasOriginPath(origin));
  EXPECT_FALSE(path0.empty());

  EXPECT_TRUE(database.RemovePathForOrigin(origin));
  EXPECT_FALSE(database.HasOriginPath(origin));

  base::FilePath path1;
  EXPECT_TRUE(database.GetPathForOrigin(origin, &path1));
  EXPECT_FALSE(path1.empty());
  EXPECT_NE(path0, path1);
}

TEST(SandboxOriginDatabaseTest, ListOriginsTest) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  const base::FilePath kFSDir = dir.GetPath().Append(kFileSystemDirName);
  EXPECT_FALSE(base::PathExists(kFSDir));
  EXPECT_TRUE(base::CreateDirectory(kFSDir));

  std::vector<SandboxOriginDatabase::OriginRecord> origins;

  SandboxOriginDatabase database(kFSDir, nullptr);
  EXPECT_TRUE(database.ListAllOrigins(&origins));
  EXPECT_TRUE(origins.empty());
  origins.clear();

  std::string origin0("origin0");
  std::string origin1("origin1");

  EXPECT_FALSE(database.HasOriginPath(origin0));
  EXPECT_FALSE(database.HasOriginPath(origin1));

  base::FilePath path0;
  base::FilePath path1;
  EXPECT_TRUE(database.GetPathForOrigin(origin0, &path0));
  EXPECT_TRUE(database.ListAllOrigins(&origins));
  EXPECT_EQ(origins.size(), 1UL);
  EXPECT_EQ(origins[0].origin, origin0);
  EXPECT_EQ(origins[0].path, path0);
  origins.clear();
  EXPECT_TRUE(database.GetPathForOrigin(origin1, &path1));
  EXPECT_TRUE(database.ListAllOrigins(&origins));
  EXPECT_EQ(origins.size(), 2UL);
  if (origins[0].origin == origin0) {
    EXPECT_EQ(origins[0].path, path0);
    EXPECT_EQ(origins[1].origin, origin1);
    EXPECT_EQ(origins[1].path, path1);
  } else {
    EXPECT_EQ(origins[0].origin, origin1);
    EXPECT_EQ(origins[0].path, path1);
    EXPECT_EQ(origins[1].origin, origin0);
    EXPECT_EQ(origins[1].path, path0);
  }
}

TEST(SandboxOriginDatabaseTest, DatabaseRecoveryTest) {
  // Checks if SandboxOriginDatabase properly handles database corruption.
  // In this test, we'll register some origins to the origin database, then
  // corrupt database and its log file.
  // After repairing, the origin database should be consistent even when some
  // entries lost.

  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  const base::FilePath kFSDir = dir.GetPath().Append(kFileSystemDirName);
  const base::FilePath kDBDir = kFSDir.Append(kOriginDatabaseName);
  EXPECT_FALSE(base::PathExists(kFSDir));
  EXPECT_TRUE(base::CreateDirectory(kFSDir));

  const std::string kOrigins[] = {
      "foo.example.com",  "bar.example.com",  "baz.example.com",
      "hoge.example.com", "fuga.example.com",
  };

  std::unique_ptr<SandboxOriginDatabase> database(
      new SandboxOriginDatabase(kFSDir, nullptr));
  for (size_t i = 0; i < base::size(kOrigins); ++i) {
    base::FilePath path;
    EXPECT_FALSE(database->HasOriginPath(kOrigins[i]));
    EXPECT_TRUE(database->GetPathForOrigin(kOrigins[i], &path));
    EXPECT_FALSE(path.empty());
    EXPECT_TRUE(database->GetPathForOrigin(kOrigins[i], &path));

    if (i != 1)
      EXPECT_TRUE(base::CreateDirectory(kFSDir.Append(path)));
  }
  database.reset();

  const base::FilePath kGarbageDir = kFSDir.AppendASCII("foo");
  const base::FilePath kGarbageFile = kGarbageDir.AppendASCII("bar");
  EXPECT_TRUE(base::CreateDirectory(kGarbageDir));
  base::File file(kGarbageFile,
                  base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  EXPECT_TRUE(file.IsValid());
  file.Close();

  // Corrupt database itself and last log entry to drop last 1 database
  // operation.  The database should detect the corruption and should recover
  // its consistency after recovery.
  CorruptDatabase(kDBDir, leveldb::kDescriptorFile, 0,
                  std::numeric_limits<size_t>::max());
  CorruptDatabase(kDBDir, leveldb::kLogFile, -1, 1);

  base::FilePath path;
  database.reset(new SandboxOriginDatabase(kFSDir, nullptr));
  std::vector<SandboxOriginDatabase::OriginRecord> origins_in_db;
  EXPECT_TRUE(database->ListAllOrigins(&origins_in_db));

  // Expect all but last added origin will be repaired back, and kOrigins[1]
  // should be dropped due to absence of backing directory.
  EXPECT_EQ(base::size(kOrigins) - 2, origins_in_db.size());

  const std::string kOrigin("piyo.example.org");
  EXPECT_FALSE(database->HasOriginPath(kOrigin));
  EXPECT_TRUE(database->GetPathForOrigin(kOrigin, &path));
  EXPECT_FALSE(path.empty());
  EXPECT_TRUE(database->HasOriginPath(kOrigin));

  EXPECT_FALSE(base::PathExists(kGarbageFile));
  EXPECT_FALSE(base::PathExists(kGarbageDir));
}

TEST(SandboxOriginDatabaseTest, DatabaseRecoveryForMissingDBFileTest) {
  const leveldb::FileType kLevelDBFileTypes[] = {
      leveldb::kLogFile,        leveldb::kDBLockFile,  leveldb::kTableFile,
      leveldb::kDescriptorFile, leveldb::kCurrentFile, leveldb::kTempFile,
      leveldb::kInfoLogFile,
  };

  for (const auto& file_type : kLevelDBFileTypes) {
    base::ScopedTempDir dir;
    ASSERT_TRUE(dir.CreateUniqueTempDir());
    const base::FilePath kFSDir = dir.GetPath().Append(kFileSystemDirName);
    const base::FilePath kDBDir = kFSDir.Append(kOriginDatabaseName);
    EXPECT_FALSE(base::PathExists(kFSDir));
    EXPECT_TRUE(base::CreateDirectory(kFSDir));

    const std::string kOrigin = "foo.example.com";
    base::FilePath path;

    std::unique_ptr<SandboxOriginDatabase> database(
        new SandboxOriginDatabase(kFSDir, nullptr));
    EXPECT_FALSE(database->HasOriginPath(kOrigin));
    EXPECT_TRUE(database->GetPathForOrigin(kOrigin, &path));
    EXPECT_FALSE(path.empty());
    EXPECT_TRUE(database->GetPathForOrigin(kOrigin, &path));
    EXPECT_TRUE(base::CreateDirectory(kFSDir.Append(path)));
    database.reset();

    DeleteDatabaseFile(kDBDir, file_type);

    database.reset(new SandboxOriginDatabase(kFSDir, nullptr));
    std::vector<SandboxOriginDatabase::OriginRecord> origins_in_db;
    EXPECT_TRUE(database->ListAllOrigins(&origins_in_db));

    const std::string kOrigin2("piyo.example.org");
    EXPECT_FALSE(database->HasOriginPath(kOrigin2));
    EXPECT_TRUE(database->GetPathForOrigin(kOrigin2, &path));
    EXPECT_FALSE(path.empty());
    EXPECT_TRUE(database->HasOriginPath(kOrigin2));
  }
}

}  // namespace content
