// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "storage/browser/quota/storage_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

class StorageDirectoryTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());
    storage_directory_ =
        std::make_unique<StorageDirectory>(temp_directory_.GetPath());
  }
  void TearDown() override { ASSERT_TRUE(temp_directory_.Delete()); }

 protected:
  base::ScopedTempDir temp_directory_;
  std::unique_ptr<StorageDirectory> storage_directory_;
};

TEST_F(StorageDirectoryTest, CreateDirectory) {
  base::FilePath storage_path = storage_directory_->path();

  EXPECT_FALSE(base::PathExists(storage_path));

  ASSERT_TRUE(storage_directory_->Create());
  EXPECT_TRUE(base::PathExists(storage_path));

  // Should still return true if it already exists.
  ASSERT_TRUE(storage_directory_->Create());
  EXPECT_TRUE(base::PathExists(storage_path));
}

TEST_F(StorageDirectoryTest, DoomAndClearStorage) {
  base::FilePath storage_path = storage_directory_->path();
  ASSERT_TRUE(storage_directory_->Create());
  EXPECT_TRUE(base::PathExists(storage_path));

  // Write data into directory.
  base::WriteFile(storage_path.AppendASCII("FakeStorage"), "dummy_content");

  ASSERT_TRUE(storage_directory_->Doom());
  EXPECT_FALSE(base::PathExists(storage_path));

  std::set<base::FilePath> directories =
      storage_directory_->EnumerateDoomedDirectoriesForTesting();
  EXPECT_EQ(directories.size(), 1u);

  storage_directory_->ClearDoomed();
  directories = storage_directory_->EnumerateDoomedDirectoriesForTesting();
  EXPECT_EQ(directories.size(), 0u);
}

TEST_F(StorageDirectoryTest, ClearDoomedMultiple) {
  base::FilePath storage_path = storage_directory_->path();

  // Create and doom storage directory multiple times.
  for (unsigned int i = 0; i < 5; i++) {
    ASSERT_TRUE(storage_directory_->Create());
    ASSERT_TRUE(storage_directory_->Doom());
  }

  std::set<base::FilePath> directories =
      storage_directory_->EnumerateDoomedDirectoriesForTesting();
  EXPECT_EQ(directories.size(), 5u);

  storage_directory_->ClearDoomed();
  directories = storage_directory_->EnumerateDoomedDirectoriesForTesting();
  EXPECT_EQ(directories.size(), 0u);
}

}  // namespace storage
