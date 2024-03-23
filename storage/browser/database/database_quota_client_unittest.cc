// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/sequence_checker_impl.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "storage/browser/database/database_quota_client.h"
#include "storage/browser/database/database_tracker.h"
#include "storage/browser/database/database_util.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/common/database/database_identifier.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace storage {

// Declared to shorten the line lengths.
static const blink::mojom::StorageType kTemp =
    blink::mojom::StorageType::kTemporary;

// Mocks DatabaseTracker methods used by DatabaseQuotaClient.
class MockDatabaseTracker : public DatabaseTracker {
 public:
  MockDatabaseTracker(const base::FilePath& path,
                      bool is_incognito,
                      scoped_refptr<QuotaManagerProxy> quota_manager_proxy)
      : DatabaseTracker(path,
                        is_incognito,
                        std::move(quota_manager_proxy),
                        DatabaseTracker::CreatePassKey()) {}

  bool GetOriginInfo(const std::string& origin_identifier,
                     OriginInfo* info) override {
    auto found =
        mock_origin_infos_.find(GetOriginFromIdentifier(origin_identifier));
    if (found == mock_origin_infos_.end()) {
      return false;
    }
    *info = OriginInfo(found->second);
    return true;
  }

  bool GetAllOriginIdentifiers(
      std::vector<std::string>* origins_identifiers) override {
    for (const auto& origin_info : mock_origin_infos_) {
      origins_identifiers->push_back(origin_info.second.GetOriginIdentifier());
    }
    return true;
  }

  bool GetAllOriginsInfo(std::vector<OriginInfo>* origins_info) override {
    for (const auto& origin_info : mock_origin_infos_) {
      origins_info->emplace_back(origin_info.second);
    }
    return true;
  }

  void DeleteDataForOrigin(const url::Origin& origin,
                           net::CompletionOnceCallback callback) override {
    ++delete_called_count_;
    if (async_delete()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&MockDatabaseTracker::AsyncDeleteDataForOrigin, this,
                         std::move(callback)));
      return;
    }
    std::move(callback).Run(net::OK);
  }

  void AsyncDeleteDataForOrigin(net::CompletionOnceCallback callback) {
    std::move(callback).Run(net::OK);
  }

  void AddMockDatabase(const url::Origin& origin, const char* name, int size) {
    MockStorageKeyInfo& info = mock_origin_infos_[origin];
    info.set_origin(GetIdentifierFromOrigin(origin));
    info.AddMockDatabase(base::ASCIIToUTF16(name), size);
  }

  int delete_called_count() { return delete_called_count_; }
  bool async_delete() { return async_delete_; }
  void set_async_delete(bool async) { async_delete_ = async; }

 protected:
  ~MockDatabaseTracker() override = default;

 private:
  class MockStorageKeyInfo : public OriginInfo {
   public:
    void set_origin(const std::string& origin_identifier) {
      origin_identifier_ = origin_identifier;
    }

    void AddMockDatabase(const std::u16string& name, int size) {
      EXPECT_FALSE(base::Contains(database_sizes_, name));
      database_sizes_[name] = size;
      total_size_ += size;
    }
  };

  int delete_called_count_ = 0;
  bool async_delete_ = false;
  std::map<url::Origin, MockStorageKeyInfo> mock_origin_infos_;
};

// Base class for our test fixtures.
class DatabaseQuotaClientTest : public testing::TestWithParam<bool> {
 public:
  const blink::StorageKey kStorageKeyA;
  const blink::StorageKey kStorageKeyB;

  DatabaseQuotaClientTest()
      : kStorageKeyA(
            blink::StorageKey::CreateFromStringForTesting("http://host")),
        kStorageKeyB(
            blink::StorageKey::CreateFromStringForTesting("http://host:8000")) {
  }
  ~DatabaseQuotaClientTest() override = default;

  DatabaseQuotaClientTest(const DatabaseQuotaClientTest&) = delete;
  DatabaseQuotaClientTest& operator=(const DatabaseQuotaClientTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    quota_manager_ = base::MakeRefCounted<QuotaManager>(
        /*is_incognito_=*/false, data_dir_.GetPath(),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        /*quota_change_callback=*/base::DoNothing(), special_storage_policy_,
        GetQuotaSettingsFunc());
    mock_tracker_ = base::MakeRefCounted<MockDatabaseTracker>(
        data_dir_.GetPath(), is_incognito(), quota_manager_->proxy());
  }

  void TearDown() override {
    base::RunLoop run_loop;
    mock_tracker_->task_runner()->PostTask(FROM_HERE,
                                           base::BindLambdaForTesting([&]() {
                                             mock_tracker_->Shutdown();
                                             run_loop.Quit();
                                           }));
    run_loop.Run();
  }

  bool is_incognito() const { return GetParam(); }

  storage::QuotaErrorOr<BucketLocator> CreateBucketForTesting(
      const blink::StorageKey& storage_key,
      const std::string& name,
      blink::mojom::StorageType type) {
    base::test::TestFuture<storage::QuotaErrorOr<storage::BucketInfo>>
        bucket_future;
    quota_manager_->proxy()->CreateBucketForTesting(
        storage_key, name, type, base::SequencedTaskRunner::GetCurrentDefault(),
        bucket_future.GetCallback());
    return bucket_future.Take().transform(
        &storage::BucketInfo::ToBucketLocator);
  }

  int64_t GetBucketUsage(mojom::QuotaClient& client,
                         const BucketLocator& bucket) {
    base::test::TestFuture<int64_t> usage_future;
    client.GetBucketUsage(bucket, usage_future.GetCallback());
    return usage_future.Get();
  }

  static std::vector<blink::StorageKey> GetStorageKeysForType(
      mojom::QuotaClient& client,
      blink::mojom::StorageType type) {
    std::vector<blink::StorageKey> result;
    base::RunLoop loop;
    client.GetStorageKeysForType(
        type, base::BindLambdaForTesting(
                  [&](const std::vector<blink::StorageKey>& storage_keys) {
                    result = storage_keys;
                    loop.Quit();
                  }));
    loop.Run();
    return result;
  }

  blink::mojom::QuotaStatusCode DeleteBucketData(mojom::QuotaClient& client,
                                                 const BucketLocator& bucket) {
    base::test::TestFuture<blink::mojom::QuotaStatusCode> delete_future;
    client.DeleteBucketData(bucket, delete_future.GetCallback());
    return delete_future.Get();
  }

  scoped_refptr<MockSpecialStoragePolicy> special_storage_policy_;

  base::ScopedTempDir data_dir_;

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<MockDatabaseTracker> mock_tracker_;
  scoped_refptr<QuotaManager> quota_manager_;
  base::WeakPtrFactory<DatabaseQuotaClientTest> weak_factory_{this};
};

TEST_P(DatabaseQuotaClientTest, GetBucketUsage) {
  DatabaseQuotaClient client(*mock_tracker_);
  ASSERT_OK_AND_ASSIGN(
      auto bucket_a,
      CreateBucketForTesting(kStorageKeyA, kDefaultBucketName, kTemp));
  ASSERT_OK_AND_ASSIGN(
      auto bucket_b,
      CreateBucketForTesting(kStorageKeyB, kDefaultBucketName, kTemp));

  EXPECT_EQ(0, GetBucketUsage(client, bucket_a));

  mock_tracker_->AddMockDatabase(kStorageKeyA.origin(), "fooDB", 1000);
  EXPECT_EQ(1000, GetBucketUsage(client, bucket_a));

  EXPECT_EQ(0, GetBucketUsage(client, bucket_b));
}

TEST_P(DatabaseQuotaClientTest, GetStorageKeysForType) {
  DatabaseQuotaClient client(*mock_tracker_);

  EXPECT_TRUE(GetStorageKeysForType(client, kTemp).empty());

  mock_tracker_->AddMockDatabase(kStorageKeyA.origin(), "fooDB", 1000);
  std::vector<blink::StorageKey> storage_keys =
      GetStorageKeysForType(client, kTemp);
  EXPECT_EQ(storage_keys.size(), 1ul);
  EXPECT_THAT(storage_keys, testing::Contains(kStorageKeyA));
}

TEST_P(DatabaseQuotaClientTest, DeleteBucketData) {
  DatabaseQuotaClient client(*mock_tracker_);
  ASSERT_OK_AND_ASSIGN(
      auto bucket_a,
      CreateBucketForTesting(kStorageKeyA, kDefaultBucketName, kTemp));

  mock_tracker_->set_async_delete(false);
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk,
            DeleteBucketData(client, bucket_a));
  EXPECT_EQ(1, mock_tracker_->delete_called_count());

  mock_tracker_->set_async_delete(true);
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk,
            DeleteBucketData(client, bucket_a));
  EXPECT_EQ(2, mock_tracker_->delete_called_count());
}

TEST_P(DatabaseQuotaClientTest, NonDefaultBucket) {
  DatabaseQuotaClient client(*mock_tracker_);
  ASSERT_OK_AND_ASSIGN(
      auto bucket, CreateBucketForTesting(kStorageKeyA, "inbox_bucket", kTemp));
  ASSERT_FALSE(bucket.is_default);

  EXPECT_EQ(0, GetBucketUsage(client, bucket));
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk,
            DeleteBucketData(client, bucket));
}

INSTANTIATE_TEST_SUITE_P(DatabaseQuotaClientTests,
                         DatabaseQuotaClientTest,
                         testing::Bool());

}  // namespace storage
