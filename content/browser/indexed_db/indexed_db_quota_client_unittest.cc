// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_quota_client.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

using ::blink::StorageKey;
using ::blink::mojom::StorageType;

// Declared to shorten the line lengths.
static const StorageType kTemp = StorageType::kTemporary;

namespace content {

// Base class for our test fixtures.
class IndexedDBQuotaClientTest : public testing::Test {
 public:
  const StorageKey kStorageKeyA;
  const StorageKey kStorageKeyB;

  IndexedDBQuotaClientTest()
      : kStorageKeyA(StorageKey::CreateFromStringForTesting("http://host")),
        kStorageKeyB(
            StorageKey::CreateFromStringForTesting("http://host:8000")),
        special_storage_policy_(
            base::MakeRefCounted<storage::MockSpecialStoragePolicy>()) {
    CreateTempDir();
    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        /*in_memory=*/false, temp_dir_.GetPath(),
        base::ThreadTaskRunnerHandle::Get(), special_storage_policy_);

    idb_context_ = base::MakeRefCounted<IndexedDBContextImpl>(
        temp_dir_.GetPath(), quota_manager_->proxy(),
        base::DefaultClock::GetInstance(),
        /*blob_storage_context=*/mojo::NullRemote(),
        /*file_system_access_context=*/mojo::NullRemote(),
        base::SequencedTaskRunnerHandle::Get(),
        base::SequencedTaskRunnerHandle::Get());
    base::RunLoop().RunUntilIdle();
    SetupTempDir();
  }

  void CreateTempDir() { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void SetupTempDir() {
    ASSERT_TRUE(base::CreateDirectory(idb_context_->data_path()));
  }

  IndexedDBQuotaClientTest(const IndexedDBQuotaClientTest&) = delete;
  IndexedDBQuotaClientTest& operator=(const IndexedDBQuotaClientTest&) = delete;

  ~IndexedDBQuotaClientTest() override {
    base::RunLoop().RunUntilIdle();
    idb_context_ = nullptr;
    base::RunLoop().RunUntilIdle();
  }

  static int64_t GetBucketUsage(storage::mojom::QuotaClient& client,
                                const storage::BucketLocator& bucket) {
    base::test::TestFuture<int64_t> future;
    client.GetBucketUsage(bucket, future.GetCallback());
    int64_t result = future.Get();
    EXPECT_GT(result, -1);
    return result;
  }

  static std::vector<StorageKey> GetStorageKeysForType(
      storage::mojom::QuotaClient& client,
      StorageType type) {
    std::vector<StorageKey> result;
    base::RunLoop loop;
    client.GetStorageKeysForType(
        type, base::BindLambdaForTesting(
                  [&](const std::vector<StorageKey>& storage_keys) {
                    result = storage_keys;
                    loop.Quit();
                  }));
    loop.Run();
    return result;
  }

  static blink::mojom::QuotaStatusCode DeleteBucketData(
      storage::mojom::QuotaClient& client,
      const storage::BucketLocator& bucket) {
    base::test::TestFuture<blink::mojom::QuotaStatusCode> future;
    client.DeleteBucketData(bucket, future.GetCallback());
    return future.Get();
  }

  IndexedDBContextImpl* idb_context() { return idb_context_.get(); }

  void SetFileSizeTo(const base::FilePath& path, int size) {
    std::string junk(size, 'a');
    ASSERT_TRUE(base::WriteFile(path, junk));
  }

  void AddFakeIndexedDB(const StorageKey& storage_key, int size) {
    // Create default bucket for `storage_key`.
    auto bucket = GetOrCreateBucket(storage_key, storage::kDefaultBucketName);
    base::FilePath file_path_storage_key;
    {
      base::test::TestFuture<base::FilePath> future;
      idb_context()->GetFilePathForTesting(
          blink::StorageKey(storage_key),
          future.GetCallback<const base::FilePath&>());
      file_path_storage_key = future.Take();
    }
    if (!base::CreateDirectory(file_path_storage_key)) {
      LOG(ERROR) << "failed to base::CreateDirectory "
                 << file_path_storage_key.value();
    }
    file_path_storage_key =
        file_path_storage_key.Append(FILE_PATH_LITERAL("fake_file"));
    SetFileSizeTo(file_path_storage_key, size);

    {
      base::RunLoop run_loop;
      idb_context()->ResetCachesForTesting(run_loop.QuitClosure());
      run_loop.Run();
    }
  }

  storage::BucketLocator GetBucket(const StorageKey& storage_key,
                                   const std::string& name) {
    base::test::TestFuture<storage::QuotaErrorOr<storage::BucketInfo>> future;
    quota_manager_->GetBucket(storage_key, name, kTemp, future.GetCallback());
    auto bucket = future.Take();
    EXPECT_TRUE(bucket.ok());
    return bucket->ToBucketLocator();
  }

  storage::BucketLocator GetOrCreateBucket(const StorageKey& storage_key,
                                           const std::string& name) {
    base::test::TestFuture<storage::QuotaErrorOr<storage::BucketInfo>> future;
    quota_manager_->GetOrCreateBucket(storage_key, name, future.GetCallback());
    auto bucket = future.Take();
    EXPECT_TRUE(bucket.ok());
    return bucket->ToBucketLocator();
  }

 protected:
  scoped_refptr<storage::MockSpecialStoragePolicy> special_storage_policy_;

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;

  scoped_refptr<IndexedDBContextImpl> idb_context_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  base::WeakPtrFactory<IndexedDBQuotaClientTest> weak_factory_{this};
};

TEST_F(IndexedDBQuotaClientTest, GetBucketUsage) {
  IndexedDBQuotaClient client(*idb_context());

  AddFakeIndexedDB(kStorageKeyA, 6);
  AddFakeIndexedDB(kStorageKeyB, 3);
  auto bucket_a = GetBucket(kStorageKeyA, storage::kDefaultBucketName);
  auto bucket_b = GetBucket(kStorageKeyB, storage::kDefaultBucketName);
  EXPECT_EQ(6, GetBucketUsage(client, bucket_a));
  EXPECT_EQ(3, GetBucketUsage(client, bucket_b));

  AddFakeIndexedDB(kStorageKeyA, 1000);
  EXPECT_EQ(1000, GetBucketUsage(client, bucket_a));
  EXPECT_EQ(3, GetBucketUsage(client, bucket_b));
}

TEST_F(IndexedDBQuotaClientTest, GetStorageKeysForType) {
  IndexedDBQuotaClient client(*idb_context());

  EXPECT_TRUE(GetStorageKeysForType(client, kTemp).empty());

  AddFakeIndexedDB(kStorageKeyA, 1000);
  std::vector<StorageKey> storage_keys = GetStorageKeysForType(client, kTemp);
  EXPECT_EQ(storage_keys.size(), 1ul);
  EXPECT_THAT(storage_keys, testing::Contains(kStorageKeyA));
}

TEST_F(IndexedDBQuotaClientTest, DeleteBucket) {
  IndexedDBQuotaClient client(*idb_context());

  AddFakeIndexedDB(kStorageKeyA, 1000);
  AddFakeIndexedDB(kStorageKeyB, 50);
  auto bucket_a = GetBucket(kStorageKeyA, storage::kDefaultBucketName);
  auto bucket_b = GetBucket(kStorageKeyB, storage::kDefaultBucketName);
  EXPECT_EQ(1000, GetBucketUsage(client, bucket_a));
  EXPECT_EQ(50, GetBucketUsage(client, bucket_b));

  blink::mojom::QuotaStatusCode delete_status =
      DeleteBucketData(client, bucket_a);
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk, delete_status);
  EXPECT_EQ(0, GetBucketUsage(client, bucket_a));
  EXPECT_EQ(50, GetBucketUsage(client, bucket_b));
}

TEST_F(IndexedDBQuotaClientTest, NonDefaultBucket) {
  IndexedDBQuotaClient client(*idb_context());
  auto bucket = GetOrCreateBucket(kStorageKeyA, "logs_bucket");
  ASSERT_FALSE(bucket.is_default);

  EXPECT_EQ(0, GetBucketUsage(client, bucket));
  blink::mojom::QuotaStatusCode delete_status =
      DeleteBucketData(client, bucket);
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk, delete_status);
}

TEST_F(IndexedDBQuotaClientTest, GetStorageKeyUsageForNonexistentKey) {
  IndexedDBQuotaClient client(*idb_context());

  AddFakeIndexedDB(kStorageKeyA, 1000);
  std::vector<StorageKey> storage_keys = GetStorageKeysForType(client, kTemp);
  EXPECT_EQ(storage_keys.size(), 1ul);
  EXPECT_THAT(storage_keys, testing::Contains(kStorageKeyA));

  auto bucket_b = GetOrCreateBucket(kStorageKeyB, storage::kDefaultBucketName);
  EXPECT_EQ(0, GetBucketUsage(client, bucket_b));
}

TEST_F(IndexedDBQuotaClientTest, IncognitoQuota) {
  auto quota_manager = base::MakeRefCounted<storage::MockQuotaManager>(
      /*in_memory=*/true, base::FilePath(), base::ThreadTaskRunnerHandle::Get(),
      special_storage_policy_);
  auto incognito_idb_context = base::MakeRefCounted<IndexedDBContextImpl>(
      base::FilePath(), quota_manager->proxy(),
      base::DefaultClock::GetInstance(),
      /*blob_storage_context=*/mojo::NullRemote(),
      /*file_system_access_context=*/mojo::NullRemote(),
      base::SequencedTaskRunnerHandle::Get(),
      base::SequencedTaskRunnerHandle::Get());
  base::RunLoop().RunUntilIdle();

  IndexedDBQuotaClient client(*incognito_idb_context.get());

  base::test::TestFuture<storage::QuotaErrorOr<storage::BucketInfo>>
      bucket_future;
  quota_manager->CreateBucketForTesting(kStorageKeyA,
                                        storage::kDefaultBucketName, kTemp,
                                        bucket_future.GetCallback());
  auto bucket_a = bucket_future.Take();
  EXPECT_TRUE(bucket_a.ok());

  // No FakeIndexDB is added.
  EXPECT_TRUE(GetStorageKeysForType(client, kTemp).empty());
  EXPECT_EQ(0, GetBucketUsage(client, bucket_a->ToBucketLocator()));
}

}  // namespace content
