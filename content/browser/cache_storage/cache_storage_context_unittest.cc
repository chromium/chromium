// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "content/browser/cache_storage/cache_storage_context_impl.h"
#include "content/public/browser/storage_usage_info.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/quota_manager_proxy_sync.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"

namespace content {

const char kExampleStorageKey[] = "https://example.com";
const char kGoogleStorageKey[] = "https://google.com";

class CacheStorageContextTest : public testing::Test {
 public:
  CacheStorageContextTest()
      : special_storage_policy_(
            base::MakeRefCounted<storage::MockSpecialStoragePolicy>()) {}
  ~CacheStorageContextTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        /*is_incognito=*/false, data_dir_.GetPath(),
        base::SingleThreadTaskRunner::GetCurrentDefault().get(),
        /*special storage policy=*/nullptr);
    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        quota_manager_.get(),
        base::SingleThreadTaskRunner::GetCurrentDefault());
    cache_storage_context_ =
        std::make_unique<CacheStorageContextImpl>(quota_manager_proxy());

    cache_storage_context_->Init(
        cache_storage_control_remote_.BindNewPipeAndPassReceiver(),
        base::FilePath(),
        cache_quota_client_remote_.BindNewPipeAndPassReceiver(),
        background_fetch_quota_client_remote_.BindNewPipeAndPassReceiver(),
        blob_receiver_.InitWithNewPipeAndPassRemote());
  }

  void AddReceiver(
      mojo::PendingReceiver<blink::mojom::CacheStorage> cache_storage_receiver,
      const blink::StorageKey& storage_key) {
    network::CrossOriginEmbedderPolicy cross_origin_embedder_policy;
    network::DocumentIsolationPolicy document_isolation_policy;
    cache_storage_context_->AddReceiver(
        cross_origin_embedder_policy, mojo::NullRemote(),
        document_isolation_policy,
        storage::BucketLocator::ForDefaultBucket(storage_key),
        storage::mojom::CacheStorageOwner::kCacheAPI,
        std::move(cache_storage_receiver));
  }

 protected:
  storage::QuotaManagerProxy* quota_manager_proxy() {
    return quota_manager_proxy_.get();
  }

  scoped_refptr<storage::MockSpecialStoragePolicy> special_storage_policy_;

  base::ScopedTempDir data_dir_;

  // These tests need a full TaskEnvironment because CacheStorageContextImpl
  // uses the thread pool for querying QuotaDatabase.
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<CacheStorageContextImpl> cache_storage_context_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;
  mojo::Remote<storage::mojom::CacheStorageControl>
      cache_storage_control_remote_;
  mojo::Remote<storage::mojom::QuotaClient> cache_quota_client_remote_;
  mojo::Remote<storage::mojom::QuotaClient>
      background_fetch_quota_client_remote_;
  mojo::PendingReceiver<storage::mojom::BlobStorageContext> blob_receiver_;
};

TEST_F(CacheStorageContextTest, DefaultBucketCreatedOnAddReceiver) {
  mojo::Remote<blink::mojom::CacheStorage> example_remote;
  AddReceiver(
      example_remote.BindNewPipeAndPassReceiver(),
      blink::StorageKey::CreateFromStringForTesting(kExampleStorageKey));

  mojo::Remote<blink::mojom::CacheStorage> google_remote;
  AddReceiver(google_remote.BindNewPipeAndPassReceiver(),
              blink::StorageKey::CreateFromStringForTesting(kGoogleStorageKey));

  storage::QuotaManagerProxySync quota_manager_proxy_sync(
      quota_manager_proxy());

  // Call method on remote to ensure that AddReceiver task has completed.
  base::RunLoop loop;
  google_remote->Keys(
      /*trace_id=*/0,
      base::BindLambdaForTesting(
          [&](const std::vector<std::u16string>& keys) { loop.Quit(); }));
  loop.Run();

  // Check default bucket exists for https://example.com.
  ASSERT_OK_AND_ASSIGN(
      storage::BucketInfo result,
      quota_manager_proxy_sync.GetBucket(
          blink::StorageKey::CreateFromStringForTesting(kExampleStorageKey),
          storage::kDefaultBucketName, blink::mojom::StorageType::kTemporary));
  EXPECT_EQ(result.name, storage::kDefaultBucketName);
  EXPECT_EQ(result.storage_key,
            blink::StorageKey::CreateFromStringForTesting(kExampleStorageKey));
  EXPECT_GT(result.id.value(), 0);

  // Check default bucket exists for https://google.com.
  ASSERT_OK_AND_ASSIGN(
      result,
      quota_manager_proxy_sync.GetBucket(
          blink::StorageKey::CreateFromStringForTesting(kGoogleStorageKey),
          storage::kDefaultBucketName, blink::mojom::StorageType::kTemporary));
  EXPECT_EQ(result.name, storage::kDefaultBucketName);
  EXPECT_EQ(result.storage_key,
            blink::StorageKey::CreateFromStringForTesting(kGoogleStorageKey));
  EXPECT_GT(result.id.value(), 0);
}

TEST_F(CacheStorageContextTest, GetDefaultBucketError) {
  // Disable database so it will return errors when getting the default bucket.
  quota_manager_->SetDisableDatabase(true);

  mojo::Remote<blink::mojom::CacheStorage> example_remote;
  AddReceiver(
      example_remote.BindNewPipeAndPassReceiver(),
      blink::StorageKey::CreateFromStringForTesting(kExampleStorageKey));

  storage::QuotaManagerProxySync quota_manager_proxy_sync(
      quota_manager_proxy());

  // CacheStorage::Has
  base::RunLoop loop_1;
  example_remote->Has(
      u"cache_name", /*trace_id=*/0,
      base::BindLambdaForTesting([&](blink::mojom::CacheStorageError error) {
        EXPECT_EQ(error, blink::mojom::CacheStorageError::kErrorStorage);
        loop_1.Quit();
      }));
  loop_1.Run();

  // CacheStorage::Delete
  base::RunLoop loop_2;
  example_remote->Delete(
      u"cache_name", /*trace_id=*/0,
      base::BindLambdaForTesting([&](blink::mojom::CacheStorageError error) {
        EXPECT_EQ(error, blink::mojom::CacheStorageError::kErrorStorage);
        loop_2.Quit();
      }));
  loop_2.Run();

  // CacheStorage::Keys
  base::RunLoop loop_3;
  example_remote->Keys(
      /*trace_id=*/0,
      base::BindLambdaForTesting([&](const std::vector<std::u16string>& keys) {
        EXPECT_EQ(keys, std::vector<std::u16string>());
        loop_3.Quit();
      }));
  loop_3.Run();

  // CacheStorage::Match
  auto options = blink::mojom::MultiCacheQueryOptions::New();
  options->query_options = blink::mojom::CacheQueryOptions::New();
  options->cache_name = u"cache_name";

  base::RunLoop loop_4;
  example_remote->Match(
      blink::mojom::FetchAPIRequest::New(), std::move(options),
      /*in_related_fetch_event=*/false, /*in_range_fetch_event=*/false,
      /*trace_id=*/0,
      base::BindLambdaForTesting([&](blink::mojom::MatchResultPtr result) {
        EXPECT_EQ(result->get_status(),
                  blink::mojom::CacheStorageError::kErrorStorage);
        loop_4.Quit();
      }));
  loop_4.Run();

  // CacheStorage::Open
  base::RunLoop loop_5;
  example_remote->Open(
      u"cache_name", /*trace_id=*/0,
      base::BindLambdaForTesting([&](blink::mojom::OpenResultPtr result) {
        EXPECT_EQ(result->get_status(),
                  blink::mojom::CacheStorageError::kErrorStorage);
        loop_5.Quit();
      }));
  loop_5.Run();
}

}  // namespace content
