// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/sandbox_prioritized_origin_database.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "storage/browser/file_system/sandbox_origin_database.h"
#include "testing/gtest/include/gtest/gtest.h"

using storage::SandboxOriginDatabase;
using storage::SandboxOriginDatabaseInterface;
using storage::SandboxPrioritizedOriginDatabase;

namespace content {

TEST(SandboxPrioritizedOriginDatabaseTest, BasicTest) {
  base::ScopedTempDir dir;
  base::FilePath path;
  ASSERT_TRUE(dir.CreateUniqueTempDir());

  const std::string kOrigin1("origin1");
  const std::string kOrigin2("origin2");

  SandboxPrioritizedOriginDatabase database(dir.GetPath(), nullptr);

  // Set the kOrigin1 as a parimary origin.
  EXPECT_TRUE(database.InitializePrimaryOrigin(kOrigin1));

  // Add two origins.
  EXPECT_TRUE(database.GetPathForOrigin(kOrigin1, &path));
  EXPECT_TRUE(database.GetPathForOrigin(kOrigin2, &path));

  // Verify them.
  EXPECT_TRUE(database.HasOriginPath(kOrigin1));
  EXPECT_TRUE(database.GetPathForOrigin(kOrigin1, &path));
  EXPECT_FALSE(path.empty());
  EXPECT_TRUE(database.HasOriginPath(kOrigin2));
  EXPECT_TRUE(database.GetPathForOrigin(kOrigin2, &path));
  EXPECT_FALSE(path.empty());

  std::vector<SandboxOriginDatabaseInterface::OriginRecord> origins;
  database.ListAllOrigins(&origins);
  ASSERT_EQ(2U, origins.size());
  EXPECT_TRUE(origins[0].origin == kOrigin1 || origins[1].origin == kOrigin1);
  EXPECT_TRUE(origins[0].origin == kOrigin2 || origins[1].origin == kOrigin2);
  EXPECT_NE(origins[0].path, origins[1].path);

  // Try remove path for kOrigin1.
  database.RemovePathForOrigin(kOrigin1);

  // Verify the removal.
  EXPECT_FALSE(database.HasOriginPath(kOrigin1));
  EXPECT_TRUE(database.HasOriginPath(kOrigin2));
  database.ListAllOrigins(&origins);
  ASSERT_EQ(1U, origins.size());
  EXPECT_EQ(kOrigin2, origins[0].origin);

  // Try remove path for kOrigin2.
  database.RemovePathForOrigin(kOrigin2);

  // Verify the removal.
  EXPECT_FALSE(database.HasOriginPath(kOrigin1));
  EXPECT_FALSE(database.HasOriginPath(kOrigin2));
  database.ListAllOrigins(&origins);
  EXPECT_TRUE(origins.empty());
}

TEST(SandboxPrioritizedOriginDatabaseTest, SetPrimaryLaterTest) {
  base::ScopedTempDir dir;
  base::FilePath path;
  ASSERT_TRUE(dir.CreateUniqueTempDir());

  const std::string kOrigin1("origin1");
  const std::string kOrigin2("origin2");

  SandboxPrioritizedOriginDatabase database(dir.GetPath(), nullptr);

  EXPECT_TRUE(database.GetPrimaryOrigin().empty());

  EXPECT_TRUE(database.GetPathForOrigin(kOrigin1, &path));
  EXPECT_TRUE(database.GetPathForOrigin(kOrigin2, &path));

  // Set the kOrigin1 as a parimary origin.
  EXPECT_TRUE(database.InitializePrimaryOrigin(kOrigin1));
  EXPECT_EQ(kOrigin1, database.GetPrimaryOrigin());

  // Regardless of whether it is initialized as primary or not
  // they should just work.
  EXPECT_TRUE(database.HasOriginPath(kOrigin1));
  EXPECT_TRUE(database.GetPathForOrigin(kOrigin1, &path));
  EXPECT_FALSE(path.empty());
  EXPECT_TRUE(database.HasOriginPath(kOrigin2));
  EXPECT_TRUE(database.GetPathForOrigin(kOrigin2, &path));
  EXPECT_FALSE(path.empty());
}

TEST(SandboxPrioritizedOriginDatabaseTest, LostPrimaryOriginFileTest) {
  base::ScopedTempDir dir;
  base::FilePath path;
  ASSERT_TRUE(dir.CreateUniqueTempDir());

  const std::string kOrigin1("origin1");
  const std::string kData("foo");

  SandboxPrioritizedOriginDatabase database(dir.GetPath(), nullptr);

  EXPECT_TRUE(database.GetPrimaryOrigin().empty());

  // Set the kOrigin1 as a parimary origin.
  EXPECT_TRUE(database.InitializePrimaryOrigin(kOrigin1));
  EXPECT_EQ(kOrigin1, database.GetPrimaryOrigin());

  // Make sure it works.
  EXPECT_TRUE(database.HasOriginPath(kOrigin1));
  EXPECT_TRUE(database.GetPathForOrigin(kOrigin1, &path));

  // Reset the database.
  database.DropDatabase();

  // kOrigin1 should still be marked as primary.
  EXPECT_TRUE(database.HasOriginPath(kOrigin1));
  EXPECT_TRUE(database.GetPathForOrigin(kOrigin1, &path));

  // Corrupt the primary origin file.
  base::WriteFile(database.primary_origin_file(), kData.data(), kData.size());

  // Reset the database.
  database.DropDatabase();

  // kOrigin1 is no longer marked as primary, and unfortunately we fail
  // to find the data for the origin.
  EXPECT_FALSE(database.HasOriginPath(kOrigin1));
}

TEST(SandboxPrioritizedOriginDatabaseTest, MigrationTest) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());

  const std::string kOrigin1("origin1");
  const std::string kOrigin2("origin2");
  const std::string kFakeDirectoryData1("0123456789");
  const std::string kFakeDirectoryData2("abcde");
  base::FilePath old_dir_db_path1, old_dir_db_path2;
  base::FilePath path1, path2;

  // Initialize the directory with two origins using the regular
  // SandboxOriginDatabase.
  {
    SandboxOriginDatabase database_old(dir.GetPath(), nullptr);
    base::FilePath old_db_path = database_old.GetDatabasePath();
    EXPECT_FALSE(base::PathExists(old_db_path));

    // Initialize paths for kOrigin1 and kOrigin2.
    EXPECT_TRUE(database_old.GetPathForOrigin(kOrigin1, &path1));
    EXPECT_FALSE(path1.empty());
    EXPECT_TRUE(database_old.GetPathForOrigin(kOrigin2, &path2));
    EXPECT_FALSE(path2.empty());

    EXPECT_TRUE(base::DirectoryExists(old_db_path));

    // Populate the origin directory with some fake data.
    old_dir_db_path1 = dir.GetPath().Append(path1);
    ASSERT_TRUE(base::CreateDirectory(old_dir_db_path1));
    EXPECT_EQ(static_cast<int>(kFakeDirectoryData1.size()),
              base::WriteFile(old_dir_db_path1.AppendASCII("dummy"),
                              kFakeDirectoryData1.data(),
                              kFakeDirectoryData1.size()));
    old_dir_db_path2 = dir.GetPath().Append(path2);
    ASSERT_TRUE(base::CreateDirectory(old_dir_db_path2));
    EXPECT_EQ(static_cast<int>(kFakeDirectoryData2.size()),
              base::WriteFile(old_dir_db_path2.AppendASCII("dummy"),
                              kFakeDirectoryData2.data(),
                              kFakeDirectoryData2.size()));
  }

  // Re-open the directory using sandboxPrioritizedOriginDatabase.
  SandboxPrioritizedOriginDatabase database(dir.GetPath(), nullptr);

  // Set the kOrigin1 as a parimary origin.
  // (Trying to initialize another origin should fail).
  EXPECT_TRUE(database.InitializePrimaryOrigin(kOrigin1));
  EXPECT_FALSE(database.InitializePrimaryOrigin(kOrigin2));

  EXPECT_EQ(kOrigin1, database.GetPrimaryOrigin());

  // Regardless of whether the origin is registered as primary or not
  // it should just work.
  EXPECT_TRUE(database.HasOriginPath(kOrigin1));
  EXPECT_TRUE(database.GetPathForOrigin(kOrigin1, &path1));
  EXPECT_TRUE(database.HasOriginPath(kOrigin2));
  EXPECT_TRUE(database.GetPathForOrigin(kOrigin2, &path2));

  // The directory content must be kept (or migrated if necessary) as well.
  std::string origin_db_data;
  base::FilePath dir_db_path = dir.GetPath().Append(path1);
  EXPECT_TRUE(base::PathExists(dir_db_path.AppendASCII("dummy")));
  EXPECT_TRUE(base::ReadFileToString(dir_db_path.AppendASCII("dummy"),
                                     &origin_db_data));
  EXPECT_EQ(kFakeDirectoryData1, origin_db_data);

  origin_db_data.clear();
  dir_db_path = dir.GetPath().Append(path2);
  EXPECT_TRUE(base::PathExists(dir_db_path.AppendASCII("dummy")));
  EXPECT_TRUE(base::ReadFileToString(dir_db_path.AppendASCII("dummy"),
                                     &origin_db_data));
  EXPECT_EQ(kFakeDirectoryData2, origin_db_data);

  // After the migration the kOrigin1 directory database path must be gone.
  EXPECT_FALSE(base::PathExists(old_dir_db_path1));
  EXPECT_TRUE(base::PathExists(old_dir_db_path2));
}

}  // namespace content
