// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <utility>

#include "base/auto_reset.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_data_format_version.h"
#include "content/browser/indexed_db/indexed_db_factory_impl.h"
#include "content/browser/indexed_db/mock_indexed_db_callbacks.h"
#include "content/browser/indexed_db/mock_indexed_db_database_callbacks.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/indexeddb/web_idb_types.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_database_exception.h"
#include "url/gurl.h"
#include "url/origin.h"

using base::ASCIIToUTF16;
using blink::IndexedDBDatabaseMetadata;
using url::Origin;

namespace content {

namespace {

class MockIDBFactory : public IndexedDBFactoryImpl {
 public:
  explicit MockIDBFactory(IndexedDBContextImpl* context)
      : MockIDBFactory(context, base::DefaultClock::GetInstance()) {}
  MockIDBFactory(IndexedDBContextImpl* context, base::Clock* clock)
      : IndexedDBFactoryImpl(context, clock) {}
  scoped_refptr<IndexedDBBackingStore> TestOpenBackingStore(
      const Origin& origin,
      const base::FilePath& data_directory) {
    IndexedDBDataLossInfo data_loss_info;
    bool disk_full;
    leveldb::Status s;
    auto backing_store = OpenBackingStore(origin, data_directory,
                                          &data_loss_info, &disk_full, &s);
    EXPECT_EQ(blink::kWebIDBDataLossNone, data_loss_info.status);
    return backing_store;
  }

  void TestCloseBackingStore(IndexedDBBackingStore* backing_store) {
    CloseBackingStore(backing_store->origin());
  }

  void TestReleaseBackingStore(IndexedDBBackingStore* backing_store,
                               bool immediate) {
    ReleaseBackingStore(backing_store->origin(), immediate);
  }

 private:
  ~MockIDBFactory() override {}

  DISALLOW_COPY_AND_ASSIGN(MockIDBFactory);
};

}  // namespace

class IndexedDBFactoryTest : public testing::Test {
 public:
  IndexedDBFactoryTest()
      : quota_manager_proxy_(
            base::MakeRefCounted<MockQuotaManagerProxy>(nullptr, nullptr)) {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    context_ = base::MakeRefCounted<IndexedDBContextImpl>(
        temp_dir_.GetPath(), /*special_storage_policy=*/nullptr,
        quota_manager_proxy_.get());
  }

  void TearDown() override {
    quota_manager_proxy_->SimulateQuotaManagerDestroyed();
  }

 protected:
  IndexedDBContextImpl* context() const { return context_.get(); }

 private:
  TestBrowserThreadBundle thread_bundle_;

  base::ScopedTempDir temp_dir_;
  scoped_refptr<MockQuotaManagerProxy> quota_manager_proxy_;
  scoped_refptr<IndexedDBContextImpl> context_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBFactoryTest);
};

TEST_F(IndexedDBFactoryTest, BackingStoreLifetime) {
  base::RunLoop loop;
  context()->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        auto factory = base::MakeRefCounted<MockIDBFactory>(context());

        const Origin origin1 = Origin::Create(GURL("http://localhost:81"));
        const Origin origin2 = Origin::Create(GURL("http://localhost:82"));

        auto disk_store1 =
            factory->TestOpenBackingStore(origin1, context()->data_path());

        auto disk_store2 =
            factory->TestOpenBackingStore(origin1, context()->data_path());
        EXPECT_EQ(disk_store1.get(), disk_store2.get());

        auto disk_store3 =
            factory->TestOpenBackingStore(origin2, context()->data_path());

        factory->TestCloseBackingStore(disk_store1.get());
        factory->TestCloseBackingStore(disk_store3.get());

        EXPECT_FALSE(disk_store1->HasOneRef());
        EXPECT_FALSE(disk_store2->HasOneRef());
        EXPECT_TRUE(disk_store3->HasOneRef());

        disk_store2 = nullptr;
        EXPECT_TRUE(disk_store1->HasOneRef());
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(IndexedDBFactoryTest, BackingStoreLazyClose) {
  base::RunLoop loop;
  context()->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        auto factory = base::MakeRefCounted<MockIDBFactory>(context());

        const Origin origin = Origin::Create(GURL("http://localhost:81"));

        auto store =
            factory->TestOpenBackingStore(origin, context()->data_path());

        // Give up the local refptr so that the factory has the only
        // outstanding reference.
        IndexedDBBackingStore* store_ptr = store.get();
        store = nullptr;
        EXPECT_FALSE(store_ptr->close_timer()->IsRunning());
        factory->TestReleaseBackingStore(store_ptr, false);
        EXPECT_TRUE(store_ptr->close_timer()->IsRunning());

        factory->TestOpenBackingStore(origin, context()->data_path());
        EXPECT_FALSE(store_ptr->close_timer()->IsRunning());
        factory->TestReleaseBackingStore(store_ptr, false);
        EXPECT_TRUE(store_ptr->close_timer()->IsRunning());

        // Take back a ref ptr and ensure that the actual close
        // stops a running timer.
        store = store_ptr;
        factory->TestCloseBackingStore(store_ptr);
        EXPECT_FALSE(store_ptr->close_timer()->IsRunning());
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(IndexedDBFactoryTest, BackingStoreNoSweeping) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {}, {kIDBTombstoneDeletion, kIDBTombstoneStatistics});
  base::RunLoop loop;
  context()->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        base::SimpleTestClock clock;
        clock.SetNow(base::Time::Now());

        auto factory = base::MakeRefCounted<MockIDBFactory>(context(), &clock);

        const Origin origin = Origin::Create(GURL("http://localhost:81"));

        auto store =
            factory->TestOpenBackingStore(origin, context()->data_path());

        // Give up the local refptr so that the factory has the only
        // outstanding reference.
        IndexedDBBackingStore* store_ptr = store.get();
        store = nullptr;
        EXPECT_FALSE(store_ptr->close_timer()->IsRunning());
        factory->TestReleaseBackingStore(store_ptr, false);
        EXPECT_TRUE(store_ptr->close_timer()->IsRunning());
        EXPECT_EQ(nullptr, store_ptr->pre_close_task_queue());

        // Reset the timer & stop the closing.
        factory->TestOpenBackingStore(origin, context()->data_path());
        EXPECT_FALSE(store_ptr->close_timer()->IsRunning());
        factory->TestReleaseBackingStore(store_ptr, false);
        EXPECT_TRUE(store_ptr->close_timer()->IsRunning());
        store_ptr->close_timer()->FireNow();

        // Backing store should be totally closed.
        EXPECT_FALSE(factory->IsBackingStoreOpen(origin));

        store = factory->TestOpenBackingStore(origin, context()->data_path());
        store_ptr = store.get();
        store = nullptr;
        EXPECT_FALSE(store_ptr->close_timer()->IsRunning());

        // Move the clock to start the next sweep.
        clock.Advance(IndexedDBFactoryImpl::kMaxEarliestGlobalSweepFromNow);
        factory->TestReleaseBackingStore(store_ptr, false);

        // Sweep should NOT be occurring.
        EXPECT_TRUE(store_ptr->close_timer()->IsRunning());
        store_ptr->close_timer()->FireNow();

        // Backing store should be totally closed.
        EXPECT_FALSE(factory->IsBackingStoreOpen(origin));
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(IndexedDBFactoryTest, BackingStoreRunPreCloseTasks) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kIDBTombstoneStatistics},
                                {kIDBTombstoneDeletion});
  base::RunLoop loop;
  context()->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        base::SimpleTestClock clock;
        clock.SetNow(base::Time::Now());

        auto factory = base::MakeRefCounted<MockIDBFactory>(context(), &clock);

        const Origin origin = Origin::Create(GURL("http://localhost:81"));

        auto store =
            factory->TestOpenBackingStore(origin, context()->data_path());

        // Give up the local refptr so that the factory has the only
        // outstanding reference.
        IndexedDBBackingStore* store_ptr = store.get();
        store = nullptr;
        EXPECT_FALSE(store_ptr->close_timer()->IsRunning());
        factory->TestReleaseBackingStore(store_ptr, false);
        EXPECT_TRUE(store_ptr->close_timer()->IsRunning());
        EXPECT_EQ(nullptr, store_ptr->pre_close_task_queue());

        // Reset the timer & stop the closing.
        factory->TestOpenBackingStore(origin, context()->data_path());
        EXPECT_FALSE(store_ptr->close_timer()->IsRunning());
        factory->TestReleaseBackingStore(store_ptr, false);
        EXPECT_TRUE(store_ptr->close_timer()->IsRunning());
        store_ptr->close_timer()->FireNow();

        // Backing store should be totally closed.
        EXPECT_FALSE(factory->IsBackingStoreOpen(origin));

        store = factory->TestOpenBackingStore(origin, context()->data_path());
        store_ptr = store.get();
        store = nullptr;
        EXPECT_FALSE(store_ptr->close_timer()->IsRunning());

        // Move the clock to start the next sweep.
        clock.Advance(IndexedDBFactoryImpl::kMaxEarliestGlobalSweepFromNow);
        factory->TestReleaseBackingStore(store_ptr, false);

        // Sweep should be occuring.
        EXPECT_TRUE(store_ptr->close_timer()->IsRunning());
        store_ptr->close_timer()->FireNow();
        ASSERT_NE(nullptr, store_ptr->pre_close_task_queue());
        EXPECT_TRUE(store_ptr->pre_close_task_queue()->started());

        // Stop sweep by opening a connection.
        factory->TestOpenBackingStore(origin, context()->data_path());
        EXPECT_EQ(nullptr, store_ptr->pre_close_task_queue());

        // Move clock forward to trigger next sweep, but origin has longer
        // sweep minimum, so nothing happens.
        clock.Advance(IndexedDBFactoryImpl::kMaxEarliestGlobalSweepFromNow);

        factory->TestReleaseBackingStore(store_ptr, false);
        EXPECT_TRUE(store_ptr->close_timer()->IsRunning());
        EXPECT_EQ(nullptr, store_ptr->pre_close_task_queue());

        // Reset, and move clock forward so the origin should allow a sweep.
        factory->TestOpenBackingStore(origin, context()->data_path());
        EXPECT_EQ(nullptr, store_ptr->pre_close_task_queue());
        clock.Advance(IndexedDBFactoryImpl::kMaxEarliestOriginSweepFromNow);
        factory->TestReleaseBackingStore(store_ptr, false);

        // Sweep should be occuring.
        EXPECT_TRUE(store_ptr->close_timer()->IsRunning());
        store_ptr->close_timer()->FireNow();
        ASSERT_NE(nullptr, store_ptr->pre_close_task_queue());
        EXPECT_TRUE(store_ptr->pre_close_task_queue()->started());

        // Take back a ref ptr and ensure that the actual close
        // stops a running timer.
        store = store_ptr;
        factory->TestCloseBackingStore(store_ptr);
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(IndexedDBFactoryTest, BackingStoreCloseImmediatelySwitch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kIDBTombstoneStatistics},
                                {kIDBTombstoneDeletion});
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      kIDBCloseImmediatelySwitch);
  base::RunLoop loop;
  context()->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        base::SimpleTestClock clock;
        clock.SetNow(base::Time::Now());

        auto factory = base::MakeRefCounted<MockIDBFactory>(context(), &clock);

        const Origin origin = Origin::Create(GURL("http://localhost:81"));

        auto store =
            factory->TestOpenBackingStore(origin, context()->data_path());

        // Give up the local refptr so that the factory has the only
        // outstanding reference.
        IndexedDBBackingStore* store_ptr = store.get();
        store = nullptr;
        EXPECT_FALSE(store_ptr->close_timer()->IsRunning());
        factory->TestReleaseBackingStore(store_ptr, false);
        EXPECT_FALSE(factory->IsBackingStoreOpen(origin));
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(IndexedDBFactoryTest, MemoryBackingStoreLifetime) {
  base::RunLoop loop;
  context()->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        auto factory = base::MakeRefCounted<MockIDBFactory>(context());

        const Origin origin1 = Origin::Create(GURL("http://localhost:81"));
        const Origin origin2 = Origin::Create(GURL("http://localhost:82"));

        auto mem_store1 =
            factory->TestOpenBackingStore(origin1, base::FilePath());

        auto mem_store2 =
            factory->TestOpenBackingStore(origin1, base::FilePath());
        EXPECT_EQ(mem_store1.get(), mem_store2.get());

        auto mem_store3 =
            factory->TestOpenBackingStore(origin2, base::FilePath());

        factory->TestCloseBackingStore(mem_store1.get());
        factory->TestCloseBackingStore(mem_store3.get());

        EXPECT_FALSE(mem_store1->HasOneRef());
        EXPECT_FALSE(mem_store2->HasOneRef());
        EXPECT_FALSE(mem_store3->HasOneRef());

        factory = nullptr;
        EXPECT_FALSE(mem_store1->HasOneRef());  // mem_store1 and 2
        EXPECT_FALSE(mem_store2->HasOneRef());  // mem_store1 and 2
        EXPECT_TRUE(mem_store3->HasOneRef());

        mem_store2 = nullptr;
        EXPECT_TRUE(mem_store1->HasOneRef());
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(IndexedDBFactoryTest, RejectLongOrigins) {
  base::RunLoop loop;
  context()->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        base::FilePath temp_dir = context()->data_path().DirName();
        int limit = base::GetMaximumPathComponentLength(temp_dir);
        EXPECT_GT(limit, 0);

        auto factory = base::MakeRefCounted<MockIDBFactory>(context());

        std::string origin(limit + 1, 'x');
        Origin too_long_origin =
            Origin::Create(GURL("http://" + origin + ":81/"));
        auto diskStore1 = factory->TestOpenBackingStore(too_long_origin,
                                                        context()->data_path());
        EXPECT_FALSE(diskStore1.get());

        Origin ok_origin = Origin::Create(GURL("http://someorigin.com:82/"));
        auto diskStore2 =
            factory->TestOpenBackingStore(ok_origin, context()->data_path());
        EXPECT_TRUE(diskStore2.get());
        // We need a manual close or Windows can't delete the temp
        // directory.
        factory->TestCloseBackingStore(diskStore2.get());
        loop.Quit();
      }));

  loop.Run();
}

class DiskFullFactory : public IndexedDBFactoryImpl {
 public:
  explicit DiskFullFactory(IndexedDBContextImpl* context)
      : IndexedDBFactoryImpl(context, base::DefaultClock::GetInstance()) {}

 private:
  ~DiskFullFactory() override {}
  scoped_refptr<IndexedDBBackingStore> OpenBackingStore(
      const Origin& origin,
      const base::FilePath& data_directory,
      IndexedDBDataLossInfo* data_loss_info,
      bool* disk_full,
      leveldb::Status* s) override {
    *disk_full = true;
    *s = leveldb::Status::IOError("Disk is full");
    return scoped_refptr<IndexedDBBackingStore>();
  }

  DISALLOW_COPY_AND_ASSIGN(DiskFullFactory);
};

class LookingForQuotaErrorMockCallbacks : public IndexedDBCallbacks {
 public:
  LookingForQuotaErrorMockCallbacks()
      : IndexedDBCallbacks(nullptr,
                           url::Origin(),
                           nullptr,
                           base::SequencedTaskRunnerHandle::Get()),
        error_called_(false) {}
  void OnError(const IndexedDBDatabaseError& error) override {
    error_called_ = true;
    EXPECT_EQ(blink::kWebIDBDatabaseExceptionQuotaError, error.code());
  }
  bool error_called() const { return error_called_; }

 private:
  ~LookingForQuotaErrorMockCallbacks() override {}
  bool error_called_;

  DISALLOW_COPY_AND_ASSIGN(LookingForQuotaErrorMockCallbacks);
};

TEST_F(IndexedDBFactoryTest, QuotaErrorOnDiskFull) {
  auto callbacks = base::MakeRefCounted<LookingForQuotaErrorMockCallbacks>();
  auto dummy_database_callbacks =
      base::MakeRefCounted<IndexedDBDatabaseCallbacks>(nullptr, nullptr);

  base::RunLoop loop;
  context()->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        const Origin origin = Origin::Create(GURL("http://localhost:81"));
        auto factory = base::MakeRefCounted<DiskFullFactory>(context());
        const base::string16 name(ASCIIToUTF16("name"));
        auto connection = std::make_unique<IndexedDBPendingConnection>(
            callbacks, dummy_database_callbacks, /*child_process_id=*/0,
            /*transaction_id=*/2, /*version=*/1);
        factory->Open(name, std::move(connection), origin,
                      context()->data_path());
        EXPECT_TRUE(callbacks->error_called());
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(IndexedDBFactoryTest, BackingStoreReleasedOnForcedClose) {
  auto callbacks = base::MakeRefCounted<MockIndexedDBCallbacks>();
  auto db_callbacks = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  base::RunLoop loop;
  context()->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        auto factory = base::MakeRefCounted<MockIDBFactory>(context());

        const Origin origin = Origin::Create(GURL("http://localhost:81"));
        const int64_t transaction_id = 1;
        auto connection = std::make_unique<IndexedDBPendingConnection>(
            callbacks, db_callbacks, /*child_process_id=*/0, transaction_id,
            IndexedDBDatabaseMetadata::DEFAULT_VERSION);
        factory->Open(ASCIIToUTF16("db"), std::move(connection), origin,
                      context()->data_path());

        EXPECT_TRUE(callbacks->connection());

        EXPECT_TRUE(factory->IsBackingStoreOpen(origin));
        EXPECT_FALSE(factory->IsBackingStorePendingClose(origin));

        callbacks->connection()->ForceClose();

        EXPECT_FALSE(factory->IsBackingStoreOpen(origin));
        EXPECT_FALSE(factory->IsBackingStorePendingClose(origin));
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(IndexedDBFactoryTest, BackingStoreReleaseDelayedOnClose) {
  auto callbacks = base::MakeRefCounted<MockIndexedDBCallbacks>();
  auto db_callbacks = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();

  base::RunLoop loop;
  context()->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        auto factory = base::MakeRefCounted<MockIDBFactory>(context());

        const Origin origin = Origin::Create(GURL("http://localhost:81"));
        const int64_t transaction_id = 1;
        auto connection = std::make_unique<IndexedDBPendingConnection>(
            callbacks, db_callbacks, /*child_process_id=*/0, transaction_id,
            IndexedDBDatabaseMetadata::DEFAULT_VERSION);
        factory->Open(ASCIIToUTF16("db"), std::move(connection), origin,
                      context()->data_path());

        EXPECT_TRUE(callbacks->connection());
        IndexedDBBackingStore* store =
            callbacks->connection()->database()->backing_store();
        EXPECT_FALSE(store->HasOneRef());  // Factory and database.

        EXPECT_TRUE(factory->IsBackingStoreOpen(origin));
        callbacks->connection()->Close();
        EXPECT_TRUE(store->HasOneRef());  // Factory.
        EXPECT_TRUE(factory->IsBackingStoreOpen(origin));
        EXPECT_TRUE(factory->IsBackingStorePendingClose(origin));
        EXPECT_TRUE(store->close_timer()->IsRunning());

        // Take a ref so it won't be destroyed out from under the test.
        scoped_refptr<IndexedDBBackingStore> store_ref = store;

        // Now simulate shutdown, which should stop the timer.
        factory->ContextDestroyed();
        EXPECT_TRUE(store->HasOneRef());  // Local.
        EXPECT_FALSE(store->close_timer()->IsRunning());
        EXPECT_FALSE(factory->IsBackingStoreOpen(origin));
        EXPECT_FALSE(factory->IsBackingStorePendingClose(origin));
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(IndexedDBFactoryTest, DeleteDatabaseClosesBackingStore) {
  auto callbacks = base::MakeRefCounted<MockIndexedDBCallbacks>(
      /*expect_connection=*/false);

  base::RunLoop loop;
  context()->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        auto factory = base::MakeRefCounted<MockIDBFactory>(context());

        const Origin origin = Origin::Create(GURL("http://localhost:81"));
        EXPECT_FALSE(factory->IsBackingStoreOpen(origin));

        factory->DeleteDatabase(ASCIIToUTF16("db"), callbacks, origin,
                                context()->data_path(),
                                /*force_close=*/false);

        EXPECT_TRUE(factory->IsBackingStoreOpen(origin));
        EXPECT_TRUE(factory->IsBackingStorePendingClose(origin));

        // Now simulate shutdown, which should stop the timer.
        factory->ContextDestroyed();

        EXPECT_FALSE(factory->IsBackingStoreOpen(origin));
        EXPECT_FALSE(factory->IsBackingStorePendingClose(origin));

        loop.Quit();
      }));
  loop.Run();
}

TEST_F(IndexedDBFactoryTest, GetDatabaseNamesClosesBackingStore) {
  auto callbacks = base::MakeRefCounted<MockIndexedDBCallbacks>(
      /*expect_connection=*/false);

  base::RunLoop loop;
  context()->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        auto factory = base::MakeRefCounted<MockIDBFactory>(context());

        const Origin origin = Origin::Create(GURL("http://localhost:81"));
        EXPECT_FALSE(factory->IsBackingStoreOpen(origin));

        factory->GetDatabaseNames(callbacks, origin, context()->data_path());

        EXPECT_TRUE(factory->IsBackingStoreOpen(origin));
        EXPECT_TRUE(factory->IsBackingStorePendingClose(origin));

        // Now simulate shutdown, which should stop the timer.
        factory->ContextDestroyed();

        EXPECT_FALSE(factory->IsBackingStoreOpen(origin));
        EXPECT_FALSE(factory->IsBackingStorePendingClose(origin));

        loop.Quit();
      }));
  loop.Run();
}

TEST_F(IndexedDBFactoryTest, ForceCloseReleasesBackingStore) {
  auto callbacks = base::MakeRefCounted<MockIndexedDBCallbacks>();
  auto db_callbacks = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();

  base::RunLoop loop;
  context()->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        auto factory = base::MakeRefCounted<MockIDBFactory>(context());

        const Origin origin = Origin::Create(GURL("http://localhost:81"));
        const int64_t transaction_id = 1;
        auto connection = std::make_unique<IndexedDBPendingConnection>(
            callbacks, db_callbacks, /*child_process_id=*/0, transaction_id,
            IndexedDBDatabaseMetadata::DEFAULT_VERSION);
        factory->Open(ASCIIToUTF16("db"), std::move(connection), origin,
                      context()->data_path());

        EXPECT_TRUE(callbacks->connection());
        EXPECT_TRUE(factory->IsBackingStoreOpen(origin));
        EXPECT_FALSE(factory->IsBackingStorePendingClose(origin));

        callbacks->connection()->Close();

        EXPECT_TRUE(factory->IsBackingStoreOpen(origin));
        EXPECT_TRUE(factory->IsBackingStorePendingClose(origin));

        factory->ForceClose(origin, /*delete_in_memory_store=*/false);

        EXPECT_FALSE(factory->IsBackingStoreOpen(origin));
        EXPECT_FALSE(factory->IsBackingStorePendingClose(origin));

        // Ensure it is safe if the store is not open.
        factory->ForceClose(origin, /*delete_in_memory_store=*/false);

        loop.Quit();
      })),
      loop.Run();
}

class UpgradeNeededCallbacks : public MockIndexedDBCallbacks {
 public:
  UpgradeNeededCallbacks() {}

  void OnSuccess(std::unique_ptr<IndexedDBConnection> connection,
                 const IndexedDBDatabaseMetadata& metadata) override {
    EXPECT_TRUE(connection_.get());
    EXPECT_FALSE(connection.get());
  }

  void OnUpgradeNeeded(int64_t old_version,
                       std::unique_ptr<IndexedDBConnection> connection,
                       const IndexedDBDatabaseMetadata& metadata,
                       const IndexedDBDataLossInfo& data_loss_info) override {
    connection_ = std::move(connection);
  }

 protected:
  ~UpgradeNeededCallbacks() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(UpgradeNeededCallbacks);
};

class ErrorCallbacks : public MockIndexedDBCallbacks {
 public:
  ErrorCallbacks() : MockIndexedDBCallbacks(false), saw_error_(false) {}

  void OnError(const IndexedDBDatabaseError& error) override {
    saw_error_ = true;
  }
  bool saw_error() const { return saw_error_; }

 private:
  ~ErrorCallbacks() override {}
  bool saw_error_;

  DISALLOW_COPY_AND_ASSIGN(ErrorCallbacks);
};

TEST_F(IndexedDBFactoryTest, DatabaseFailedOpen) {
  const Origin origin = Origin::Create(GURL("http://localhost:81"));
  const base::string16 db_name(ASCIIToUTF16("db"));
  const int64_t transaction_id = 1;

  // These objects are retained across posted tasks, so despite being used
  // exclusively on the IDB sequence.

  // Created and used on IDB sequence.
  scoped_refptr<MockIDBFactory> factory;

  // Created on IO thread, used on IDB sequence.
  auto upgrade_callbacks = base::MakeRefCounted<UpgradeNeededCallbacks>();
  auto db_callbacks = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  auto failed_open_callbacks = base::MakeRefCounted<ErrorCallbacks>();
  auto db_callbacks2 = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();

  {
    base::RunLoop loop;
    context()->TaskRunner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          factory = base::MakeRefCounted<MockIDBFactory>(context());
          // Open at version 2.
          const int64_t db_version = 2;
          factory->Open(db_name,
                        std::make_unique<IndexedDBPendingConnection>(
                            upgrade_callbacks, db_callbacks,
                            /*child_process_id=*/0, transaction_id, db_version),
                        origin, context()->data_path());
          EXPECT_TRUE(factory->IsDatabaseOpen(origin, db_name));
          loop.Quit();
        }));
    loop.Run();
  }

  {
    base::RunLoop loop;
    context()->TaskRunner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          // Close the connection.
          {
            EXPECT_TRUE(upgrade_callbacks->connection());
            upgrade_callbacks->connection()->database()->Commit(
                upgrade_callbacks->connection()->GetTransaction(
                    transaction_id));
            upgrade_callbacks->connection()->Close();
            EXPECT_FALSE(factory->IsDatabaseOpen(origin, db_name));
          }

          // Open at version < 2, which will fail; ensure factory doesn't
          // retain the database object.
          {
            const int64_t db_version = 1;
            auto connection = std::make_unique<IndexedDBPendingConnection>(
                failed_open_callbacks, db_callbacks2,
                /*child_process_id=*/0, transaction_id, db_version);
            factory->Open(db_name, std::move(connection), origin,
                          context()->data_path());
            EXPECT_TRUE(failed_open_callbacks->saw_error());
            EXPECT_FALSE(factory->IsDatabaseOpen(origin, db_name));
          }

          // Terminate all pending-close timers.
          factory->ForceClose(origin, /*delete_in_memory_store=*/false);
          loop.Quit();
        }));
    loop.Run();
  }
}

namespace {

class DataLossCallbacks final : public MockIndexedDBCallbacks {
 public:
  blink::WebIDBDataLoss data_loss() const { return data_loss_; }
  void OnSuccess(std::unique_ptr<IndexedDBConnection> connection,
                 const IndexedDBDatabaseMetadata& metadata) override {
    if (!connection_)
      connection_ = std::move(connection);
  }
  void OnError(const IndexedDBDatabaseError& error) final {
    ADD_FAILURE() << "Unexpected IDB error: " << error.message();
  }
  void OnUpgradeNeeded(int64_t old_version,
                       std::unique_ptr<IndexedDBConnection> connection,
                       const IndexedDBDatabaseMetadata& metadata,
                       const IndexedDBDataLossInfo& data_loss) final {
    connection_ = std::move(connection);
    data_loss_ = data_loss.status;
  }

 private:
  ~DataLossCallbacks() final {}
  blink::WebIDBDataLoss data_loss_ = blink::kWebIDBDataLossNone;
};

TEST_F(IndexedDBFactoryTest, DataFormatVersion) {
  auto try_open = [this](const Origin& origin,
                         const IndexedDBDataFormatVersion& version) {
    base::AutoReset<IndexedDBDataFormatVersion> override_version(
        &IndexedDBDataFormatVersion::GetMutableCurrentForTesting(), version);

    // These objects are retained across posted tasks, so despite being used
    // exclusively on the IDB sequence.

    // Created and used on IDB sequence.
    scoped_refptr<MockIDBFactory> factory;

    // Created on IO thread, used on IDB sequence.
    auto callbacks = base::MakeRefCounted<DataLossCallbacks>();

    const int64_t transaction_id = 1;
    blink::WebIDBDataLoss result;
    auto db_callbacks = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();

    {
      base::RunLoop loop;
      context()->TaskRunner()->PostTask(
          FROM_HERE, base::BindLambdaForTesting([&]() {
            factory = base::MakeRefCounted<MockIDBFactory>(context());
            factory->Open(ASCIIToUTF16("test_db"),
                          std::make_unique<IndexedDBPendingConnection>(
                              callbacks, db_callbacks, /*child_process_id=*/0,
                              transaction_id, /*version=*/1),
                          origin, context()->data_path());
            loop.Quit();
          }));
      loop.Run();
    }
    {
      base::RunLoop loop;
      context()->TaskRunner()->PostTask(
          FROM_HERE, base::BindLambdaForTesting([&]() {
            auto* connection = callbacks->connection();
            EXPECT_TRUE(connection);
            connection->database()->Commit(
                connection->GetTransaction(transaction_id));
            connection->Close();
            factory->ForceClose(origin, /*delete_in_memory_store=*/false);
            result = callbacks->data_loss();
            loop.Quit();
          }));
      loop.Run();
    }
    return result;
  };

  using blink::kWebIDBDataLossNone;
  using blink::kWebIDBDataLossTotal;
  static const struct {
    const char* origin;
    IndexedDBDataFormatVersion open_version_1;
    IndexedDBDataFormatVersion open_version_2;
    blink::WebIDBDataLoss expected_data_loss;
  } kTestCases[] = {
      {"http://same-version.com/", {3, 4}, {3, 4}, kWebIDBDataLossNone},
      {"http://blink-upgrade.com/", {3, 4}, {3, 5}, kWebIDBDataLossNone},
      {"http://v8-upgrade.com/", {3, 4}, {4, 4}, kWebIDBDataLossNone},
      {"http://both-upgrade.com/", {3, 4}, {4, 5}, kWebIDBDataLossNone},
      {"http://blink-downgrade.com/", {3, 4}, {3, 3}, kWebIDBDataLossTotal},
      {"http://v8-downgrade.com/", {3, 4}, {2, 4}, kWebIDBDataLossTotal},
      {"http://both-downgrade.com/", {3, 4}, {2, 3}, kWebIDBDataLossTotal},
      {"http://v8-up-blink-down.com/", {3, 4}, {4, 2}, kWebIDBDataLossTotal},
      {"http://v8-down-blink-up.com/", {3, 4}, {2, 5}, kWebIDBDataLossTotal},
  };
  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.origin);
    const Origin origin = Origin::Create(GURL(test.origin));
    ASSERT_EQ(kWebIDBDataLossNone, try_open(origin, test.open_version_1));
    EXPECT_EQ(test.expected_data_loss, try_open(origin, test.open_version_2));
  }
}

}  // namespace

}  // namespace content
