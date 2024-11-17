// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/thread.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
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

namespace content::indexed_db {

// Base class for our test fixtures.
class IndexedDBQuotaClientTest : public testing::Test,
                                 public testing::WithParamInterface<bool> {
 public:
  const StorageKey kStorageKeyFirstPartyA;
  const StorageKey kStorageKeyFirstPartyB;
  StorageKey kStorageKeyThirdPartyA;
  StorageKey kStorageKeyThirdPartyB;

  IndexedDBQuotaClientTest()
      : kStorageKeyFirstPartyA(
            StorageKey::CreateFromStringForTesting("http://host")),
        kStorageKeyFirstPartyB(
            StorageKey::CreateFromStringForTesting("http://host:8000")),
        special_storage_policy_(
            base::MakeRefCounted<storage::MockSpecialStoragePolicy>()) {
    scoped_feature_list_.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning,
        IsThirdPartyStoragePartitioningEnabled());
    // This cannot be created above as the kThirdPartyStoragePartitioning must
    // be set.
    kStorageKeyThirdPartyA =
        StorageKey::Create(url::Origin::Create(GURL("http://host")),
                           net::SchemefulSite(GURL("http://other")),
                           blink::mojom::AncestorChainBit::kCrossSite);
    kStorageKeyThirdPartyB =
        StorageKey::Create(url::Origin::Create(GURL("http://host:8000")),
                           net::SchemefulSite(GURL("http://other")),
                           blink::mojom::AncestorChainBit::kCrossSite);
    CreateTempDir();
    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        /*in_memory=*/false, temp_dir_.GetPath(),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        special_storage_policy_);

    idb_context_ = std::make_unique<IndexedDBContextImpl>(
        temp_dir_.GetPath(), quota_manager_->proxy(),
        /*blob_storage_context=*/mojo::NullRemote(),
        /*file_system_access_context=*/mojo::NullRemote(),
        base::SequencedTaskRunner::GetCurrentDefault());
    base::RunLoop().RunUntilIdle();
    SetupTempDir();
  }

  void CreateTempDir() { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void SetupTempDir() {
    ASSERT_TRUE(
        base::CreateDirectory(idb_context_->GetFirstPartyDataPathForTesting()));
  }

  IndexedDBQuotaClientTest(const IndexedDBQuotaClientTest&) = delete;
  IndexedDBQuotaClientTest& operator=(const IndexedDBQuotaClientTest&) = delete;

  ~IndexedDBQuotaClientTest() override {
    base::RunLoop().RunUntilIdle();
    idb_context_ = nullptr;
    base::RunLoop().RunUntilIdle();
  }

  int64_t GetBucketUsage(const storage::BucketLocator& bucket,
                         IndexedDBContextImpl* context = nullptr) {
    base::test::TestFuture<int64_t> future;
    (context ? context : idb_context_.get())
        ->GetBucketUsage(bucket, future.GetCallback());
    int64_t result = future.Get();
    EXPECT_GT(result, -1);
    return result;
  }

  std::vector<StorageKey> GetStorageKeysForType(
      StorageType type,
      IndexedDBContextImpl* context = nullptr) {
    std::vector<StorageKey> result;
    base::RunLoop loop;
    (context ? context : idb_context_.get())
        ->GetStorageKeysForType(
            type, base::BindLambdaForTesting(
                      [&](const std::vector<StorageKey>& storage_keys) {
                        result = storage_keys;
                        loop.Quit();
                      }));
    loop.Run();
    return result;
  }

  blink::mojom::QuotaStatusCode DeleteBucketData(
      const storage::BucketLocator& bucket,
      IndexedDBContextImpl* context = nullptr) {
    base::test::TestFuture<blink::mojom::QuotaStatusCode> future;
    (context ? context : idb_context_.get())
        ->DeleteBucketData(bucket, future.GetCallback());
    return future.Get();
  }

  IndexedDBContextImpl* idb_context() { return idb_context_.get(); }

  void SetFileSizeTo(const base::FilePath& path, int size) {
    std::string junk(size, 'a');
    ASSERT_TRUE(base::WriteFile(path, junk));
  }

  void AddFakeIndexedDB(const StorageKey& storage_key, int size) {
    // Create default bucket for `storage_key`.
    ASSERT_OK_AND_ASSIGN(
        auto bucket,
        GetOrCreateBucket(storage_key, storage::kDefaultBucketName));
    AddFakeIndexedDBForBucket(bucket, size);
  }

  void AddFakeIndexedDBForBucket(const storage::BucketLocator& bucket,
                                 int size) {
    base::FilePath file_path_storage_key;
    {
      base::test::TestFuture<base::FilePath> future;
      idb_context()->GetFilePathForTesting(
          bucket, future.GetCallback<const base::FilePath&>());
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

    // Ensure files are read from disk.
    {
      base::RunLoop run_loop;
      idb_context()->ForceInitializeFromFilesForTesting(run_loop.QuitClosure());
      run_loop.Run();
    }
  }

  storage::QuotaErrorOr<storage::BucketLocator> GetBucket(
      const StorageKey& storage_key,
      const std::string& name) {
    base::test::TestFuture<storage::QuotaErrorOr<storage::BucketInfo>> future;
    quota_manager_->GetBucketByNameUnsafe(storage_key, name, kTemp,
                                          future.GetCallback());
    return future.Take().transform(&storage::BucketInfo::ToBucketLocator);
  }

  storage::QuotaErrorOr<storage::BucketLocator> GetOrCreateBucket(
      const StorageKey& storage_key,
      const std::string& name) {
    base::test::TestFuture<storage::QuotaErrorOr<storage::BucketInfo>> future;
    storage::BucketInitParams params(storage_key, name);
    quota_manager_->UpdateOrCreateBucket(params, future.GetCallback());
    return future.Take().transform(&storage::BucketInfo::ToBucketLocator);
  }

  bool IsThirdPartyStoragePartitioningEnabled() { return GetParam(); }

 protected:
  scoped_refptr<storage::MockSpecialStoragePolicy> special_storage_policy_;
  base::test::ScopedFeatureList scoped_feature_list_;

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;

  std::unique_ptr<IndexedDBContextImpl> idb_context_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  base::WeakPtrFactory<IndexedDBQuotaClientTest> weak_factory_{this};
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    IndexedDBQuotaClientTest,
    testing::Bool());

TEST_P(IndexedDBQuotaClientTest, GetBucketUsageFirstParty) {
  AddFakeIndexedDB(kStorageKeyFirstPartyA, 6);
  AddFakeIndexedDB(kStorageKeyFirstPartyB, 3);
  ASSERT_OK_AND_ASSIGN(auto bucket_a, GetBucket(kStorageKeyFirstPartyA,
                                                storage::kDefaultBucketName));
  ASSERT_OK_AND_ASSIGN(auto bucket_b, GetBucket(kStorageKeyFirstPartyB,
                                                storage::kDefaultBucketName));
  EXPECT_EQ(6, GetBucketUsage(bucket_a));
  EXPECT_EQ(3, GetBucketUsage(bucket_b));

  AddFakeIndexedDB(kStorageKeyFirstPartyA, 1000);
  EXPECT_EQ(1000, GetBucketUsage(bucket_a));
  EXPECT_EQ(3, GetBucketUsage(bucket_b));
}

TEST_P(IndexedDBQuotaClientTest, GetBucketUsageThirdParty) {
  AddFakeIndexedDB(kStorageKeyThirdPartyA, 6);
  AddFakeIndexedDB(kStorageKeyThirdPartyB, 3);
  ASSERT_OK_AND_ASSIGN(auto bucket_a, GetBucket(kStorageKeyThirdPartyA,
                                                storage::kDefaultBucketName));
  ASSERT_OK_AND_ASSIGN(auto bucket_b, GetBucket(kStorageKeyThirdPartyB,
                                                storage::kDefaultBucketName));
  EXPECT_EQ(6, GetBucketUsage(bucket_a));
  EXPECT_EQ(3, GetBucketUsage(bucket_b));

  AddFakeIndexedDB(kStorageKeyThirdPartyA, 1000);
  EXPECT_EQ(1000, GetBucketUsage(bucket_a));
  EXPECT_EQ(3, GetBucketUsage(bucket_b));
}

TEST_P(IndexedDBQuotaClientTest, GetBucketUsageMixedParty) {
  AddFakeIndexedDB(kStorageKeyFirstPartyA, 6);
  AddFakeIndexedDB(kStorageKeyThirdPartyA, 3);
  ASSERT_OK_AND_ASSIGN(auto bucket_a, GetBucket(kStorageKeyFirstPartyA,
                                                storage::kDefaultBucketName));
  ASSERT_OK_AND_ASSIGN(auto bucket_b, GetBucket(kStorageKeyThirdPartyA,
                                                storage::kDefaultBucketName));
  if (IsThirdPartyStoragePartitioningEnabled()) {
    EXPECT_NE(bucket_a, bucket_b);
  } else {
    EXPECT_EQ(bucket_a, bucket_b);
  }
  if (IsThirdPartyStoragePartitioningEnabled()) {
    EXPECT_EQ(6, GetBucketUsage(bucket_a));
  } else {
    EXPECT_EQ(3, GetBucketUsage(bucket_a));
  }
  EXPECT_EQ(3, GetBucketUsage(bucket_b));

  AddFakeIndexedDB(kStorageKeyFirstPartyA, 1000);
  EXPECT_EQ(1000, GetBucketUsage(bucket_a));
  if (IsThirdPartyStoragePartitioningEnabled()) {
    EXPECT_EQ(3, GetBucketUsage(bucket_b));
  } else {
    EXPECT_EQ(1000, GetBucketUsage(bucket_b));
  }
}

TEST_P(IndexedDBQuotaClientTest, GetBucketUsageCustom) {
  ASSERT_OK_AND_ASSIGN(auto bucket_a,
                       GetOrCreateBucket(kStorageKeyFirstPartyA, "inbox"));
  ASSERT_OK_AND_ASSIGN(auto bucket_b,
                       GetOrCreateBucket(kStorageKeyFirstPartyB, "drafts"));
  AddFakeIndexedDBForBucket(bucket_a, 6);
  AddFakeIndexedDBForBucket(bucket_b, 3);
  EXPECT_EQ(6, GetBucketUsage(bucket_a));
  EXPECT_EQ(3, GetBucketUsage(bucket_b));

  AddFakeIndexedDBForBucket(bucket_a, 1000);
  EXPECT_EQ(1000, GetBucketUsage(bucket_a));
  EXPECT_EQ(3, GetBucketUsage(bucket_b));
}

TEST_P(IndexedDBQuotaClientTest, GetStorageKeysForTypeFirstParty) {
  EXPECT_TRUE(GetStorageKeysForType(kTemp).empty());

  AddFakeIndexedDB(kStorageKeyFirstPartyA, 1000);
  std::vector<StorageKey> storage_keys = GetStorageKeysForType(kTemp);
  EXPECT_EQ(storage_keys.size(), 1ul);
  EXPECT_THAT(storage_keys, testing::Contains(kStorageKeyFirstPartyA));
}

TEST_P(IndexedDBQuotaClientTest, GetStorageKeysForTypeThirdParty) {
  EXPECT_TRUE(GetStorageKeysForType(kTemp).empty());

  AddFakeIndexedDB(kStorageKeyThirdPartyA, 1000);
  std::vector<StorageKey> storage_keys = GetStorageKeysForType(kTemp);
  EXPECT_EQ(storage_keys.size(), 1ul);
  EXPECT_THAT(storage_keys, testing::Contains(kStorageKeyThirdPartyA));
}

TEST_P(IndexedDBQuotaClientTest, DeleteBucketFirstParty) {
  AddFakeIndexedDB(kStorageKeyFirstPartyA, 1000);
  AddFakeIndexedDB(kStorageKeyFirstPartyB, 50);
  ASSERT_OK_AND_ASSIGN(auto bucket_a, GetBucket(kStorageKeyFirstPartyA,
                                                storage::kDefaultBucketName));
  ASSERT_OK_AND_ASSIGN(auto bucket_b, GetBucket(kStorageKeyFirstPartyB,
                                                storage::kDefaultBucketName));
  EXPECT_EQ(1000, GetBucketUsage(bucket_a));
  EXPECT_EQ(50, GetBucketUsage(bucket_b));

  blink::mojom::QuotaStatusCode delete_status = DeleteBucketData(bucket_a);
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk, delete_status);
  EXPECT_EQ(0, GetBucketUsage(bucket_a));
  EXPECT_EQ(50, GetBucketUsage(bucket_b));
}

TEST_P(IndexedDBQuotaClientTest, DeleteBucketThirdParty) {
  AddFakeIndexedDB(kStorageKeyThirdPartyA, 1000);
  AddFakeIndexedDB(kStorageKeyThirdPartyB, 50);
  ASSERT_OK_AND_ASSIGN(auto bucket_a, GetBucket(kStorageKeyThirdPartyA,
                                                storage::kDefaultBucketName));
  ASSERT_OK_AND_ASSIGN(auto bucket_b, GetBucket(kStorageKeyThirdPartyB,
                                                storage::kDefaultBucketName));
  EXPECT_EQ(1000, GetBucketUsage(bucket_a));
  EXPECT_EQ(50, GetBucketUsage(bucket_b));

  blink::mojom::QuotaStatusCode delete_status = DeleteBucketData(bucket_a);
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk, delete_status);
  EXPECT_EQ(0, GetBucketUsage(bucket_a));
  EXPECT_EQ(50, GetBucketUsage(bucket_b));
}

TEST_P(IndexedDBQuotaClientTest, DeleteBucketMixedParty) {
  AddFakeIndexedDB(kStorageKeyFirstPartyA, 1000);
  AddFakeIndexedDB(kStorageKeyThirdPartyA, 50);
  ASSERT_OK_AND_ASSIGN(auto bucket_a, GetBucket(kStorageKeyFirstPartyA,
                                                storage::kDefaultBucketName));
  ASSERT_OK_AND_ASSIGN(auto bucket_b, GetBucket(kStorageKeyThirdPartyA,
                                                storage::kDefaultBucketName));
  if (IsThirdPartyStoragePartitioningEnabled()) {
    EXPECT_NE(bucket_a, bucket_b);
  } else {
    EXPECT_EQ(bucket_a, bucket_b);
  }
  if (IsThirdPartyStoragePartitioningEnabled()) {
    EXPECT_EQ(1000, GetBucketUsage(bucket_a));
  } else {
    EXPECT_EQ(50, GetBucketUsage(bucket_a));
  }
  EXPECT_EQ(50, GetBucketUsage(bucket_b));

  blink::mojom::QuotaStatusCode delete_status = DeleteBucketData(bucket_a);
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk, delete_status);
  EXPECT_EQ(0, GetBucketUsage(bucket_a));
  if (IsThirdPartyStoragePartitioningEnabled()) {
    EXPECT_EQ(50, GetBucketUsage(bucket_b));
  } else {
    EXPECT_EQ(0, GetBucketUsage(bucket_b));
  }
}

TEST_P(IndexedDBQuotaClientTest, DeleteBucketCustom) {
  ASSERT_OK_AND_ASSIGN(auto bucket_a,
                       GetOrCreateBucket(kStorageKeyFirstPartyA, "inbox"));
  ASSERT_OK_AND_ASSIGN(auto bucket_b,
                       GetOrCreateBucket(kStorageKeyFirstPartyB, "drafts"));
  AddFakeIndexedDBForBucket(bucket_a, 1000);
  AddFakeIndexedDBForBucket(bucket_b, 50);
  EXPECT_EQ(1000, GetBucketUsage(bucket_a));
  EXPECT_EQ(50, GetBucketUsage(bucket_b));

  blink::mojom::QuotaStatusCode delete_status = DeleteBucketData(bucket_a);
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk, delete_status);
  EXPECT_EQ(0, GetBucketUsage(bucket_a));
  EXPECT_EQ(50, GetBucketUsage(bucket_b));
}

TEST_P(IndexedDBQuotaClientTest, NonDefaultBucketFirstParty) {
  ASSERT_OK_AND_ASSIGN(
      auto bucket, GetOrCreateBucket(kStorageKeyFirstPartyA, "logs_bucket"));
  ASSERT_FALSE(bucket.is_default);

  EXPECT_EQ(0, GetBucketUsage(bucket));
  blink::mojom::QuotaStatusCode delete_status = DeleteBucketData(bucket);
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk, delete_status);
}

TEST_P(IndexedDBQuotaClientTest, NonDefaultBucketThirdParty) {
  ASSERT_OK_AND_ASSIGN(
      auto bucket, GetOrCreateBucket(kStorageKeyThirdPartyA, "logs_bucket"));
  ASSERT_FALSE(bucket.is_default);

  EXPECT_EQ(0, GetBucketUsage(bucket));
  blink::mojom::QuotaStatusCode delete_status = DeleteBucketData(bucket);
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk, delete_status);
}

TEST_P(IndexedDBQuotaClientTest,
       GetStorageKeyUsageForNonexistentKeyFirstParty) {
  AddFakeIndexedDB(kStorageKeyFirstPartyA, 1000);
  std::vector<StorageKey> storage_keys = GetStorageKeysForType(kTemp);
  EXPECT_EQ(storage_keys.size(), 1ul);
  EXPECT_THAT(storage_keys, testing::Contains(kStorageKeyFirstPartyA));

  ASSERT_OK_AND_ASSIGN(
      auto bucket_b,
      GetOrCreateBucket(kStorageKeyFirstPartyB, storage::kDefaultBucketName));
  EXPECT_EQ(0, GetBucketUsage(bucket_b));
}

TEST_P(IndexedDBQuotaClientTest,
       GetStorageKeyUsageForNonexistentKeyThirdParty) {
  AddFakeIndexedDB(kStorageKeyThirdPartyA, 1000);
  std::vector<StorageKey> storage_keys = GetStorageKeysForType(kTemp);
  EXPECT_EQ(storage_keys.size(), 1ul);
  EXPECT_THAT(storage_keys, testing::Contains(kStorageKeyThirdPartyA));

  ASSERT_OK_AND_ASSIGN(
      auto bucket_b,
      GetOrCreateBucket(kStorageKeyThirdPartyB, storage::kDefaultBucketName));
  EXPECT_EQ(0, GetBucketUsage(bucket_b));
}

TEST_P(IndexedDBQuotaClientTest,
       GetStorageKeyUsageForNonexistentKeyMixedParty) {
  AddFakeIndexedDB(kStorageKeyFirstPartyA, 1000);
  std::vector<StorageKey> storage_keys = GetStorageKeysForType(kTemp);
  EXPECT_EQ(storage_keys.size(), 1ul);
  EXPECT_THAT(storage_keys, testing::Contains(kStorageKeyFirstPartyA));

  ASSERT_OK_AND_ASSIGN(
      auto bucket_b,
      GetOrCreateBucket(kStorageKeyThirdPartyA, storage::kDefaultBucketName));
  if (IsThirdPartyStoragePartitioningEnabled()) {
    EXPECT_EQ(0, GetBucketUsage(bucket_b));
  } else {
    EXPECT_EQ(1000, GetBucketUsage(bucket_b));
  }
}

TEST_P(IndexedDBQuotaClientTest, IncognitoQuotaFirstParty) {
  auto quota_manager = base::MakeRefCounted<storage::MockQuotaManager>(
      /*in_memory=*/true, base::FilePath(),
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      special_storage_policy_);
  auto incognito_idb_context = std::make_unique<IndexedDBContextImpl>(
      base::FilePath(), quota_manager->proxy(),
      /*blob_storage_context=*/mojo::NullRemote(),
      /*file_system_access_context=*/mojo::NullRemote(),
      base::SequencedTaskRunner::GetCurrentDefault());
  base::RunLoop().RunUntilIdle();

  base::test::TestFuture<storage::QuotaErrorOr<storage::BucketInfo>>
      bucket_future;
  quota_manager->CreateBucketForTesting(kStorageKeyFirstPartyA,
                                        storage::kDefaultBucketName, kTemp,
                                        bucket_future.GetCallback());
  ASSERT_OK_AND_ASSIGN(auto bucket_a, bucket_future.Take());

  // No FakeIndexDB is added.
  EXPECT_TRUE(
      GetStorageKeysForType(kTemp, incognito_idb_context.get()).empty());
  EXPECT_EQ(0, GetBucketUsage(bucket_a.ToBucketLocator(),
                              incognito_idb_context.get()));
}

TEST_P(IndexedDBQuotaClientTest, IncognitoQuotaThirdParty) {
  auto quota_manager = base::MakeRefCounted<storage::MockQuotaManager>(
      /*in_memory=*/true, base::FilePath(),
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      special_storage_policy_);
  auto incognito_idb_context = std::make_unique<IndexedDBContextImpl>(
      base::FilePath(), quota_manager->proxy(),
      /*blob_storage_context=*/mojo::NullRemote(),
      /*file_system_access_context=*/mojo::NullRemote(),
      base::SequencedTaskRunner::GetCurrentDefault());
  base::RunLoop().RunUntilIdle();

  base::test::TestFuture<storage::QuotaErrorOr<storage::BucketInfo>>
      bucket_future;
  quota_manager->CreateBucketForTesting(kStorageKeyThirdPartyA,
                                        storage::kDefaultBucketName, kTemp,
                                        bucket_future.GetCallback());
  ASSERT_OK_AND_ASSIGN(auto bucket_a, bucket_future.Take());

  // No FakeIndexDB is added.
  EXPECT_TRUE(
      GetStorageKeysForType(kTemp, incognito_idb_context.get()).empty());
  EXPECT_EQ(0, GetBucketUsage(bucket_a.ToBucketLocator(),
                              incognito_idb_context.get()));
}

}  // namespace content::indexed_db
