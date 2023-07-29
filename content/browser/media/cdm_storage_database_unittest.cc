// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "content/browser/media/cdm_storage_database.h"
#include "media/cdm/cdm_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

namespace {

const media::CdmType kCdmType{1234, 5678};

const char kFileName[] = "file.txt";
const char kFileNameTwo[] = "file2.txt";

const std::vector<uint8_t> kPopulatedFileValue = {1, 2, 3};

}  // namespace

class CdmStorageDatabaseTest : public testing::Test {
 public:
  using CdmStorageHostOpenError = CdmStorageHost::CdmStorageHostOpenError;
  CdmStorageDatabaseTest() = default;
  ~CdmStorageDatabaseTest() override = default;

  void SetUp() override { ASSERT_TRUE(profile_path_.CreateUniqueTempDir()); }

 protected:
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("https://example.com/");
  const blink::StorageKey kTestStorageKeyTwo =
      blink::StorageKey::CreateFromStringForTesting("https://exampletwo.com/");

  std::unique_ptr<CdmStorageDatabase> cdm_storage_database_;

  void SetUpDatabase(base::FilePath file_path) {
    cdm_storage_database_ = std::make_unique<CdmStorageDatabase>(file_path);
  }

  base::ScopedTempDir profile_path_;
};

// This class is to test for when db_.Open() fails.
class CdmStorageDatabaseInvalidOpenTest : public CdmStorageDatabaseTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(profile_path_.CreateUniqueTempDir());
    SetUpDatabase(profile_path_.GetPath());
  }

  void TearDown() override { ASSERT_TRUE(profile_path_.Delete()); }
};

// This class tests the CdmStorageDatabase when the path is empty.
class CdmStorageDatabaseInMemoryTest : public CdmStorageDatabaseTest {
 public:
  void SetUp() override { SetUpDatabase(base::FilePath()); }
};

// This class tests the CdmStorageDatabase with the CdmStorage path.
class CdmStorageDatabaseValidPathTest : public CdmStorageDatabaseTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(profile_path_.CreateUniqueTempDir());
    const base::FilePath cdm_storage_path =
        profile_path_.GetPath().AppendASCII("CdmStorage.db");
    SetUpDatabase(cdm_storage_path);
  }

  void TearDown() override {
    cdm_storage_database_->ClearDatabase();
    ASSERT_TRUE(profile_path_.Delete());
  }
};

TEST_F(CdmStorageDatabaseInvalidOpenTest, EnsureOpenFails) {
  auto error = cdm_storage_database_->EnsureOpenForTesting();

  // The database cannot be opened in a temporary directory, as it requires to
  // be opened at a file, thus we get an SQL open error. We return this as a
  // `kDatabaseOpenError`.
  EXPECT_EQ(error, CdmStorageHostOpenError::kDatabaseOpenError);
}

TEST_F(CdmStorageDatabaseInMemoryTest, EnsureOpenWithoutErrors) {
  auto error = cdm_storage_database_->EnsureOpenForTesting();

  EXPECT_EQ(error, CdmStorageHostOpenError::kOk);
}

TEST_F(CdmStorageDatabaseInMemoryTest, FileManipulation) {
  // When nothing is written to the database, we return an empty file.
  EXPECT_TRUE(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName)
          ->empty());

  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));

  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName),
      kPopulatedFileValue);

  EXPECT_TRUE(
      cdm_storage_database_->DeleteFile(kTestStorageKey, kCdmType, kFileName));

  EXPECT_TRUE(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName)
          ->empty());
}

TEST_F(CdmStorageDatabaseInMemoryTest, DeleteDatabase) {
  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));

  EXPECT_TRUE(cdm_storage_database_->ClearDatabase());

  auto error = cdm_storage_database_->EnsureOpenForTesting();

  EXPECT_EQ(error, CdmStorageHostOpenError::kOk);

  EXPECT_TRUE(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName)
          ->empty());
}

TEST_F(CdmStorageDatabaseInMemoryTest, DeleteForStorageKey) {
  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));
  EXPECT_TRUE(cdm_storage_database_->WriteFile(
      kTestStorageKey, kCdmType, kFileNameTwo, kPopulatedFileValue));

  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKeyTwo, kCdmType,
                                               kFileName, kPopulatedFileValue));
  EXPECT_TRUE(cdm_storage_database_->WriteFile(
      kTestStorageKeyTwo, kCdmType, kFileNameTwo, kPopulatedFileValue));

  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName),
      kPopulatedFileValue);
  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileNameTwo),
      kPopulatedFileValue);

  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKeyTwo, kCdmType, kFileName),
      kPopulatedFileValue);
  EXPECT_EQ(cdm_storage_database_->ReadFile(kTestStorageKeyTwo, kCdmType,
                                            kFileNameTwo),
            kPopulatedFileValue);

  EXPECT_TRUE(cdm_storage_database_->DeleteDataForStorageKey(kTestStorageKey,
                                                             kCdmType));

  // Expect that for the storage key, all of the file content returned is empty.
  EXPECT_TRUE(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName)
          ->empty());
  EXPECT_TRUE(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileNameTwo)
          ->empty());

  // Expect that its not deleted for other storage keys.
  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKeyTwo, kCdmType, kFileName),
      kPopulatedFileValue);
  EXPECT_EQ(cdm_storage_database_->ReadFile(kTestStorageKeyTwo, kCdmType,
                                            kFileNameTwo),
            kPopulatedFileValue);
}

TEST_F(CdmStorageDatabaseInMemoryTest, DeleteForStorageKeyWithNoData) {
  EXPECT_TRUE(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName)
          ->empty());

  // Even if there is no data for the storage key, the SQL statement should
  // still run properly.
  EXPECT_TRUE(cdm_storage_database_->DeleteDataForStorageKey(kTestStorageKey,
                                                             kCdmType));

  EXPECT_TRUE(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName)
          ->empty());
}

TEST_F(CdmStorageDatabaseValidPathTest, EnsureOpenWithoutErrors) {
  auto error = cdm_storage_database_->EnsureOpenForTesting();

  EXPECT_EQ(error, CdmStorageHostOpenError::kOk);
}

TEST_F(CdmStorageDatabaseValidPathTest, FileManipulation) {
  // When nothing is written to the database, we return an empty file.
  EXPECT_TRUE(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName)
          ->empty());

  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));

  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName),
      kPopulatedFileValue);

  EXPECT_TRUE(
      cdm_storage_database_->DeleteFile(kTestStorageKey, kCdmType, kFileName));

  EXPECT_TRUE(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName)
          ->empty());
}

TEST_F(CdmStorageDatabaseValidPathTest, DeleteDatabase) {
  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));

  EXPECT_TRUE(cdm_storage_database_->ClearDatabase());

  auto error = cdm_storage_database_->EnsureOpenForTesting();

  EXPECT_EQ(error, CdmStorageHostOpenError::kOk);

  EXPECT_TRUE(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName)
          ->empty());
}

TEST_F(CdmStorageDatabaseValidPathTest, DeleteForStorageKey) {
  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));
  EXPECT_TRUE(cdm_storage_database_->WriteFile(
      kTestStorageKey, kCdmType, kFileNameTwo, kPopulatedFileValue));

  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKeyTwo, kCdmType,
                                               kFileName, kPopulatedFileValue));
  EXPECT_TRUE(cdm_storage_database_->WriteFile(
      kTestStorageKeyTwo, kCdmType, kFileNameTwo, kPopulatedFileValue));

  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName),
      kPopulatedFileValue);
  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileNameTwo),
      kPopulatedFileValue);

  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKeyTwo, kCdmType, kFileName),
      kPopulatedFileValue);
  EXPECT_EQ(cdm_storage_database_->ReadFile(kTestStorageKeyTwo, kCdmType,
                                            kFileNameTwo),
            kPopulatedFileValue);

  EXPECT_TRUE(cdm_storage_database_->DeleteDataForStorageKey(kTestStorageKey,
                                                             kCdmType));

  // Expect that for the storage key, all of the file content returned is empty.
  EXPECT_TRUE(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName)
          ->empty());
  EXPECT_TRUE(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileNameTwo)
          ->empty());

  // Expect that its not deleted for other storage keys.
  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKeyTwo, kCdmType, kFileName),
      kPopulatedFileValue);
  EXPECT_EQ(cdm_storage_database_->ReadFile(kTestStorageKeyTwo, kCdmType,
                                            kFileNameTwo),
            kPopulatedFileValue);
}

TEST_F(CdmStorageDatabaseValidPathTest, DeleteForStorageKeyWithNoData) {
  EXPECT_TRUE(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName)
          ->empty());

  // Even if there is no data for the storage key, the SQL statement should
  // still run properly.
  EXPECT_TRUE(cdm_storage_database_->DeleteDataForStorageKey(kTestStorageKey,
                                                             kCdmType));

  EXPECT_TRUE(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName)
          ->empty());
}

}  // namespace content
