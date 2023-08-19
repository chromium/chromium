// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/barrier_closure.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/thread.h"
#include "base/time/default_clock.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/browser/indexed_db/mock_mojo_indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/mock_mojo_indexed_db_factory_client.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/quota_manager_proxy_sync.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

class IndexedDBContextTest : public testing::Test {
 public:
  class MockIndexedDBClientStateChecker
      : public storage::mojom::IndexedDBClientStateChecker {
   public:
    MockIndexedDBClientStateChecker() = default;
    ~MockIndexedDBClientStateChecker() override = default;

    // storage::mojom::IndexedDBClientStateChecker overrides
    void DisallowInactiveClient(
        storage::mojom::DisallowInactiveClientReason reason,
        mojo::PendingReceiver<storage::mojom::IndexedDBClientKeepActive>
            keep_active,
        storage::mojom::IndexedDBClientStateChecker::
            DisallowInactiveClientCallback callback) override {}
  };

  IndexedDBContextTest()
      : special_storage_policy_(
            base::MakeRefCounted<storage::MockSpecialStoragePolicy>()) {}
  ~IndexedDBContextTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        /*is_incognito=*/false, temp_dir_.GetPath(),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        special_storage_policy_);
    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        quota_manager_.get(),
        base::SingleThreadTaskRunner::GetCurrentDefault());
    indexed_db_context_ = base::MakeRefCounted<IndexedDBContextImpl>(
        temp_dir_.GetPath(), quota_manager_proxy_,
        base::DefaultClock::GetInstance(),
        /*blob_storage_context=*/mojo::NullRemote(),
        /*file_system_access_context=*/mojo::NullRemote(),
        base::SequencedTaskRunner::GetCurrentDefault(),
        base::SequencedTaskRunner::GetCurrentDefault());
  }

 protected:
  scoped_refptr<storage::MockSpecialStoragePolicy> special_storage_policy_;

  base::ScopedTempDir temp_dir_;

  // These tests need a full TaskEnvironment because IndexedDBContextImpl
  // uses the thread pool for querying QuotaDatabase.
  base::test::TaskEnvironment task_environment_;

  scoped_refptr<IndexedDBContextImpl> indexed_db_context_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;

  MockIndexedDBClientStateChecker example_checker;

  const blink::StorageKey example_storage_key_ =
      blink::StorageKey::CreateFromStringForTesting("https://example.com");
  const blink::StorageKey google_storage_key_ =
      blink::StorageKey::CreateFromStringForTesting("https://google.com");
};

TEST_F(IndexedDBContextTest, DefaultBucketCreatedOnBindIndexedDB) {
  mojo::Remote<blink::mojom::IDBFactory> example_remote;
  mojo::AssociatedReceiver<storage::mojom::IndexedDBClientStateChecker>
      example_checker_receiver(&example_checker);
  indexed_db_context_->BindIndexedDB(
      example_storage_key_,
      example_checker_receiver.BindNewEndpointAndPassDedicatedRemote(),
      example_remote.BindNewPipeAndPassReceiver());

  mojo::Remote<blink::mojom::IDBFactory> google_remote;
  mojo::AssociatedReceiver<storage::mojom::IndexedDBClientStateChecker>
      google_checker_receiver(&example_checker);
  indexed_db_context_->BindIndexedDB(
      google_storage_key_,
      google_checker_receiver.BindNewEndpointAndPassDedicatedRemote(),
      google_remote.BindNewPipeAndPassReceiver());

  storage::QuotaManagerProxySync quota_manager_proxy_sync(
      quota_manager_proxy_.get());

  // Call a method on both IDBFactory remotes and wait for both replies
  // to ensure that BindIndexedDB has completed for both storage keys.
  base::test::TestFuture<std::vector<blink::mojom::IDBNameAndVersionPtr>,
                         blink::mojom::IDBErrorPtr>
      info_future;
  auto example_bucket_locator = storage::BucketLocator();
  example_bucket_locator.storage_key = example_storage_key_;
  indexed_db_context_->GetIDBFactory()->GetDatabaseInfo(
      example_bucket_locator,
      indexed_db_context_->GetDataPath(example_bucket_locator),
      info_future.GetCallback());
  ASSERT_TRUE(info_future.Wait());

  base::test::TestFuture<std::vector<blink::mojom::IDBNameAndVersionPtr>,
                         blink::mojom::IDBErrorPtr>
      info_future2;
  auto google_bucket_locator = storage::BucketLocator();
  google_bucket_locator.storage_key = google_storage_key_;
  indexed_db_context_->GetIDBFactory()->GetDatabaseInfo(
      google_bucket_locator,
      indexed_db_context_->GetDataPath(google_bucket_locator),
      info_future2.GetCallback());
  ASSERT_TRUE(info_future2.Wait());

  // Check default bucket exists for https://example.com.
  ASSERT_OK_AND_ASSIGN(storage::BucketInfo result,
                       quota_manager_proxy_sync.GetBucket(
                           example_storage_key_, storage::kDefaultBucketName,
                           blink::mojom::StorageType::kTemporary));
  EXPECT_EQ(result.name, storage::kDefaultBucketName);
  EXPECT_EQ(result.storage_key, example_storage_key_);
  EXPECT_GT(result.id.value(), 0);

  // Check default bucket exists for https://google.com.
  ASSERT_OK_AND_ASSIGN(result,
                       quota_manager_proxy_sync.GetBucket(
                           google_storage_key_, storage::kDefaultBucketName,
                           blink::mojom::StorageType::kTemporary));
  EXPECT_EQ(result.name, storage::kDefaultBucketName);
  EXPECT_EQ(result.storage_key, google_storage_key_);
  EXPECT_GT(result.id.value(), 0);
}

TEST_F(IndexedDBContextTest, GetDefaultBucketError) {
  // Disable database so it will return errors when getting the default bucket.
  quota_manager_->SetDisableDatabase(true);

  mojo::Remote<blink::mojom::IDBFactory> example_remote;
  mojo::AssociatedReceiver<storage::mojom::IndexedDBClientStateChecker>
      example_checker_receiver(&example_checker);
  indexed_db_context_->BindIndexedDB(
      example_storage_key_,
      example_checker_receiver.BindNewEndpointAndPassDedicatedRemote(),
      example_remote.BindNewPipeAndPassReceiver());

  // IDBFactory::GetDatabaseInfo
  base::test::TestFuture<std::vector<blink::mojom::IDBNameAndVersionPtr>,
                         blink::mojom::IDBErrorPtr>
      info_future;
  example_remote->GetDatabaseInfo(info_future.GetCallback());
  auto [info, error] = info_future.Take();
  EXPECT_EQ(blink::mojom::IDBException::kUnknownError, error->error_code);
  EXPECT_EQ(u"Internal error.", error->error_message);

  // IDBFactory::Open
  base::RunLoop loop_2;
  auto mock_factory_client =
      std::make_unique<testing::StrictMock<MockMojoIndexedDBFactoryClient>>();
  auto database_callbacks =
      std::make_unique<MockMojoIndexedDBDatabaseCallbacks>();
  auto transaction_remote =
      mojo::AssociatedRemote<blink::mojom::IDBTransaction>();
  EXPECT_CALL(*mock_factory_client,
              Error(blink::mojom::IDBException::kUnknownError,
                    std::u16string(u"Internal error.")))
      .Times(1)
      .WillOnce(base::test::RunClosure(loop_2.QuitClosure()));

  example_remote->Open(mock_factory_client->CreateInterfacePtrAndBind(),
                       database_callbacks->CreateInterfacePtrAndBind(),
                       u"database_name", /*version=*/1,
                       transaction_remote.BindNewEndpointAndPassReceiver(),
                       /*transaction_id=*/0);
  loop_2.Run();

  // IDBFactory::DeleteDatabase
  base::RunLoop loop_3;
  mock_factory_client =
      std::make_unique<testing::StrictMock<MockMojoIndexedDBFactoryClient>>();
  EXPECT_CALL(*mock_factory_client,
              Error(blink::mojom::IDBException::kUnknownError,
                    std::u16string(u"Internal error.")))
      .Times(1)
      .WillOnce(base::test::RunClosure(loop_3.QuitClosure()));

  example_remote->DeleteDatabase(
      mock_factory_client->CreateInterfacePtrAndBind(), u"database_name",
      /*force_close=*/true);
  loop_3.Run();
}

}  // namespace content
