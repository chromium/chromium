// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/barrier_closure.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "content/browser/indexed_db/file_path_util.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_test_base.h"
#include "content/browser/indexed_db/mock_mojo_indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/mock_mojo_indexed_db_factory_client.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "storage/browser/test/quota_manager_proxy_sync.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content::indexed_db {
namespace {

class IndexedDBContextTest : public IndexedDBTestBase {
 public:
  class MockIndexedDBClientStateChecker
      : public storage::mojom::IndexedDBClientStateChecker {
   public:
    MockIndexedDBClientStateChecker() = default;
    ~MockIndexedDBClientStateChecker() override = default;

    // storage::mojom::IndexedDBClientStateChecker overrides
    void DisallowInactiveClient(
        int32_t connection_id,
        storage::mojom::DisallowInactiveClientReason reason,
        mojo::PendingReceiver<storage::mojom::IndexedDBClientKeepActive>
            keep_active,
        storage::mojom::IndexedDBClientStateChecker::
            DisallowInactiveClientCallback callback) override {}
    void MakeClone(
        mojo::PendingReceiver<storage::mojom::IndexedDBClientStateChecker>
            checker) override {}
  };

  IndexedDBContextTest()
      : IndexedDBTestBase(/*use_default_buckets=*/true, /*use_sqlite=*/false) {}

 protected:
  MockIndexedDBClientStateChecker example_checker_;

  const blink::StorageKey example_storage_key_ =
      blink::StorageKey::CreateFromStringForTesting("https://example.com");
  const blink::StorageKey google_storage_key_ =
      blink::StorageKey::CreateFromStringForTesting("https://google.com");
};

TEST_F(IndexedDBContextTest, DefaultBucketCreatedOnBindIndexedDB) {
  mojo::Remote<blink::mojom::IDBFactory> example_remote;
  mojo::Receiver<storage::mojom::IndexedDBClientStateChecker>
      example_checker_receiver(&example_checker_);
  context()->BindIndexedDB(
      storage::BucketLocator::ForDefaultBucket(example_storage_key_),
      storage::BucketClientInfo{},
      example_checker_receiver.BindNewPipeAndPassRemote(),
      example_remote.BindNewPipeAndPassReceiver());

  mojo::Remote<blink::mojom::IDBFactory> google_remote;
  mojo::Receiver<storage::mojom::IndexedDBClientStateChecker>
      google_checker_receiver(&example_checker_);
  context()->BindIndexedDB(
      storage::BucketLocator::ForDefaultBucket(google_storage_key_),
      storage::BucketClientInfo{},
      google_checker_receiver.BindNewPipeAndPassRemote(),
      google_remote.BindNewPipeAndPassReceiver());

  storage::QuotaManagerProxySync quota_manager_proxy_sync(
      quota_manager_proxy_.get());

  // Call a method on both IDBFactory remotes and wait for both replies
  // to ensure that BindIndexedDB has completed for both storage keys.
  base::test::TestFuture<std::vector<blink::mojom::IDBNameAndVersionPtr>,
                         blink::mojom::IDBErrorPtr>
      info_future;
  example_remote->GetDatabaseInfo(info_future.GetCallback());
  ASSERT_TRUE(info_future.Wait());

  base::test::TestFuture<std::vector<blink::mojom::IDBNameAndVersionPtr>,
                         blink::mojom::IDBErrorPtr>
      info_future2;
  google_remote->GetDatabaseInfo(info_future2.GetCallback());
  ASSERT_TRUE(info_future2.Wait());

  // Check default bucket exists for https://example.com.
  ASSERT_OK_AND_ASSIGN(storage::BucketInfo result,
                       quota_manager_proxy_sync.GetBucket(
                           example_storage_key_, storage::kDefaultBucketName));
  EXPECT_EQ(result.name, storage::kDefaultBucketName);
  EXPECT_EQ(result.storage_key, example_storage_key_);
  EXPECT_GT(result.id.value(), 0);

  // Check default bucket exists for https://google.com.
  ASSERT_OK_AND_ASSIGN(
      result, quota_manager_proxy_sync.GetBucket(google_storage_key_,
                                                 storage::kDefaultBucketName));
  EXPECT_EQ(result.name, storage::kDefaultBucketName);
  EXPECT_EQ(result.storage_key, google_storage_key_);
  EXPECT_GT(result.id.value(), 0);
}

TEST_F(IndexedDBContextTest, GetDefaultBucketError) {
  // Disable database so it will return errors when getting the default bucket.
  quota_manager_->SetDisableDatabase(true);

  mojo::Remote<blink::mojom::IDBFactory> example_remote;
  mojo::Receiver<storage::mojom::IndexedDBClientStateChecker>
      example_checker_receiver(&example_checker_);
  context()->BindIndexedDB(
      storage::BucketLocator::ForDefaultBucket(example_storage_key_),
      storage::BucketClientInfo{},
      example_checker_receiver.BindNewPipeAndPassRemote(),
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
      std::make_unique<testing::StrictMock<MockMojoFactoryClient>>();
  auto database_callbacks = std::make_unique<MockMojoDatabaseCallbacks>();
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
                       /*transaction_id=*/0, /*priority=*/0);
  loop_2.Run();

  // IDBFactory::DeleteDatabase
  base::RunLoop loop_3;
  mock_factory_client =
      std::make_unique<testing::StrictMock<MockMojoFactoryClient>>();
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

// Regression test for crbug.com/1472826
TEST_F(IndexedDBContextTest, DontChokeOnBadLegacyFiles) {
  base::CreateDirectory(context()
                            ->GetFirstPartyDataPathForTesting()
                            .AppendASCII("invalid_storage_key")
                            .AddExtension(indexed_db::kIndexedDBExtension)
                            .AddExtension(indexed_db::kLevelDBExtension));

  base::RunLoop run_loop;
  context()->ForceInitializeFromFilesForTesting(run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(IndexedDBContextTest, ShutdownDurationHistogramWithBucket) {
  base::HistogramTester histogram_tester;
  InitBucketContext();
  IndexedDBContextImpl::Shutdown(std::move(context_));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester.GetAllSamples("IndexedDB.ContextShutdownDuration2")
               .size() == 1u;
  }));
}

TEST_F(IndexedDBContextTest, ShutdownDurationHistogramWithoutBucket) {
  base::HistogramTester histogram_tester;

  // `RunPostedTasks()` doesn't work after `Shutdown()`.
  base::RunLoop loop;
  auto runner = context_->idb_task_runner();
  IndexedDBContextImpl::Shutdown(std::move(context_));
  runner->PostTask(FROM_HERE, loop.QuitClosure());
  loop.Run();

  histogram_tester.ExpectTotalCount("IndexedDB.ContextShutdownDuration2", 1);
}

TEST_F(IndexedDBContextTest, ShutdownDurationHistogramNotRecordedForInMemory) {
  base::HistogramTester histogram_tester;
  SetUpInMemoryContext();
  InitBucketContext();

  // `RunPostedTasks()` doesn't work after `Shutdown()`.
  base::RunLoop loop;
  auto runner = context_->idb_task_runner();
  IndexedDBContextImpl::Shutdown(std::move(context_));
  runner->PostTask(FROM_HERE, loop.QuitClosure());
  loop.Run();

  histogram_tester.ExpectTotalCount("IndexedDB.ContextShutdownDuration2", 0);
}

}  // namespace
}  // namespace content::indexed_db
