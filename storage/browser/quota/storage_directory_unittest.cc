// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/storage_directory.h"
#include "storage/browser/quota/storage_directory_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"

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

TEST_F(StorageDirectoryTest, CreateBucketDirectory) {
  BucketLocator example_bucket(
      BucketId(123),
      blink::StorageKey::CreateFromStringForTesting("http://example/"),
      blink::mojom::StorageType::kTemporary, /*is_default=*/false);

  base::FilePath bucket_path =
      CreateBucketPath(storage_directory_->path().DirName(), example_bucket);
  EXPECT_FALSE(base::PathExists(bucket_path));

  ASSERT_TRUE(storage_directory_->CreateBucket(example_bucket));
  EXPECT_TRUE(base::PathExists(bucket_path));

  // Should still return true if it already exists.
  ASSERT_TRUE(storage_directory_->CreateBucket(example_bucket));
  EXPECT_TRUE(base::PathExists(bucket_path));
}

TEST_F(StorageDirectoryTest, DoomAndClearBucketDirectory) {
  BucketLocator bucket1(
      BucketId(1),
      blink::StorageKey::CreateFromStringForTesting("http://example/"),
      blink::mojom::StorageType::kTemporary, /*is_default=*/false);
  BucketLocator bucket2(
      BucketId(2),
      blink::StorageKey::CreateFromStringForTesting("http://example/"),
      blink::mojom::StorageType::kTemporary, /*is_default=*/false);

  // Create directories for buckets.
  ASSERT_TRUE(storage_directory_->CreateBucket(bucket1));
  ASSERT_TRUE(storage_directory_->CreateBucket(bucket2));

  // Write data into bucket directories.
  base::FilePath bucket1_idb_path =
      CreateClientBucketPath(storage_directory_->path().DirName(), bucket1,
                             QuotaClientType::kIndexedDatabase);
  base::FilePath bucket2_idb_path =
      CreateClientBucketPath(storage_directory_->path().DirName(), bucket2,
                             QuotaClientType::kIndexedDatabase);
  ASSERT_TRUE(base::CreateDirectory(bucket1_idb_path));
  ASSERT_TRUE(base::CreateDirectory(bucket2_idb_path));
  ASSERT_TRUE(base::WriteFile(bucket1_idb_path.AppendASCII("FakeStorage"),
                              "fake_content"));
  ASSERT_TRUE(base::WriteFile(bucket2_idb_path.AppendASCII("FakeStorage"),
                              "fake_content"));

  ASSERT_TRUE(storage_directory_->DoomBucket(bucket1));
  EXPECT_FALSE(base::PathExists(bucket1_idb_path));
  EXPECT_TRUE(base::PathExists(bucket2_idb_path));

  std::set<base::FilePath> directories =
      storage_directory_->EnumerateDoomedBucketsForTesting();
  EXPECT_EQ(directories.size(), 1u);

  storage_directory_->ClearDoomedBuckets();
  directories = storage_directory_->EnumerateDoomedBucketsForTesting();
  EXPECT_EQ(directories.size(), 0u);
}

TEST_F(StorageDirectoryTest, ClearMultipleDoomedBuckets) {
  BucketLocator bucket1(
      BucketId(1),
      blink::StorageKey::CreateFromStringForTesting("http://example/"),
      blink::mojom::StorageType::kTemporary, /*is_default=*/false);
  BucketLocator bucket2(
      BucketId(2),
      blink::StorageKey::CreateFromStringForTesting("http://example/"),
      blink::mojom::StorageType::kTemporary, /*is_default=*/false);

  ASSERT_TRUE(storage_directory_->CreateBucket(bucket1));
  ASSERT_TRUE(storage_directory_->CreateBucket(bucket2));

  ASSERT_TRUE(storage_directory_->DoomBucket(bucket1));
  ASSERT_TRUE(storage_directory_->DoomBucket(bucket2));

  std::set<base::FilePath> directories =
      storage_directory_->EnumerateDoomedBucketsForTesting();
  EXPECT_EQ(directories.size(), 2u);

  storage_directory_->ClearDoomedBuckets();
  directories = storage_directory_->EnumerateDoomedBucketsForTesting();
  EXPECT_EQ(directories.size(), 0u);
}

}  // namespace storage
