// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <utility>

#include "base/auto_reset.h"
#include "base/barrier_closure.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "components/services/storage/indexed_db/leveldb/fake_leveldb_factory.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_data_format_version.h"
#include "content/browser/indexed_db/indexed_db_execution_context_connection_tracker.h"
#include "content/browser/indexed_db/indexed_db_factory_impl.h"
#include "content/browser/indexed_db/indexed_db_leveldb_env.h"
#include "content/browser/indexed_db/indexed_db_origin_state.h"
#include "content/browser/indexed_db/indexed_db_pre_close_task_queue.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/browser/indexed_db/mock_indexed_db_callbacks.h"
#include "content/browser/indexed_db/mock_indexed_db_database_callbacks.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/indexeddb/web_idb_types.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

using base::ASCIIToUTF16;
using blink::IndexedDBDatabaseMetadata;
using url::Origin;

namespace content {

namespace {

base::FilePath CreateAndReturnTempDir(base::ScopedTempDir* temp_dir) {
  CHECK(temp_dir->CreateUniqueTempDir());
  return temp_dir->GetPath();
}

void CreateAndBindTransactionPlaceholder(
    base::WeakPtr<IndexedDBTransaction> transaction) {}

}  // namespace

class IndexedDBFactoryTest : public testing::Test {
 public:
  IndexedDBFactoryTest()
      : task_environment_(std::make_unique<BrowserTaskEnvironment>()),
        quota_manager_proxy_(
            base::MakeRefCounted<MockQuotaManagerProxy>(nullptr, nullptr)) {}

  explicit IndexedDBFactoryTest(
      std::unique_ptr<BrowserTaskEnvironment> task_environment)
      : task_environment_(std::move(task_environment)),
        quota_manager_proxy_(
            base::MakeRefCounted<MockQuotaManagerProxy>(nullptr, nullptr)) {}

  void TearDown() override {
    quota_manager_proxy_->SimulateQuotaManagerDestroyed();

    if (context_ && !context_->IsInMemoryContext()) {
      IndexedDBFactoryImpl* factory = context_->GetIDBFactory();

      // Loop through all open origins, and force close them, and request the
      // deletion of the leveldb state. Once the states are no longer around,
      // delete all of the databases on disk.
      auto open_factory_origins = factory->GetOpenOrigins();
      base::RunLoop loop;
      auto callback = base::BarrierClosure(
          open_factory_origins.size(), base::BindLambdaForTesting([&]() {
            // All leveldb databases are closed, and they can be deleted.
            for (auto origin : context_->GetAllOrigins()) {
              context_->DeleteForOrigin(origin);
            }
            loop.Quit();
          }));
      for (auto origin : open_factory_origins) {
        IndexedDBOriginState* per_origin_factory =
            factory->GetOriginFactory(origin);
        per_origin_factory->backing_store()
            ->db()
            ->leveldb_state()
            ->RequestDestruction(callback,
                                 base::SequencedTaskRunnerHandle::Get());
        context_->ForceClose(origin,
                             IndexedDBContextImpl::FORCE_CLOSE_DELETE_ORIGIN);
      }
      loop.Run();
    }
    if (temp_dir_.IsValid())
      ASSERT_TRUE(temp_dir_.Delete());
    IndexedDBClassFactory::Get()->SetLevelDBFactoryForTesting(nullptr);
  }

  void SetupContext() {
    context_ = base::MakeRefCounted<IndexedDBContextImpl>(
        CreateAndReturnTempDir(&temp_dir_),
        /*special_storage_policy=*/nullptr, quota_manager_proxy_.get(),
        base::DefaultClock::GetInstance(),
        base::SequencedTaskRunnerHandle::Get());
  }

  void SetupInMemoryContext() {
    context_ = base::MakeRefCounted<IndexedDBContextImpl>(
        base::FilePath(),
        /*special_storage_policy=*/nullptr, quota_manager_proxy_.get(),
        base::DefaultClock::GetInstance(),
        base::SequencedTaskRunnerHandle::Get());
  }

  void SetupContextWithFactories(LevelDBFactory* factory, base::Clock* clock) {
    context_ = base::MakeRefCounted<IndexedDBContextImpl>(
        CreateAndReturnTempDir(&temp_dir_),
        /*special_storage_policy=*/nullptr, quota_manager_proxy_.get(), clock,
        base::SequencedTaskRunnerHandle::Get());
    if (factory)
      IndexedDBClassFactory::Get()->SetLevelDBFactoryForTesting(factory);
  }

  // Runs through the upgrade flow to create a basic database connection. There
  // is no actual data in the database.
  std::tuple<std::unique_ptr<IndexedDBConnection>,
             scoped_refptr<MockIndexedDBDatabaseCallbacks>>
  CreateConnectionForDatatabase(const Origin& origin,
                                const base::string16& name) {
    auto callbacks = base::MakeRefCounted<MockIndexedDBCallbacks>();
    auto db_callbacks = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
    const int64_t transaction_id = 1;
    auto create_transaction_callback =
        base::BindOnce(&CreateAndBindTransactionPlaceholder);
    auto connection = std::make_unique<IndexedDBPendingConnection>(
        callbacks, db_callbacks,
        IndexedDBExecutionContextConnectionTracker::Handle::CreateForTesting(),
        transaction_id, IndexedDBDatabaseMetadata::NO_VERSION,
        std::move(create_transaction_callback));

    // Do the first half of the upgrade, and request the upgrade from renderer.
    {
      base::RunLoop loop;
      callbacks->CallOnUpgradeNeeded(
          base::BindLambdaForTesting([&]() { loop.Quit(); }));
      factory()->Open(name, std::move(connection), origin,
                      context()->data_path());
      loop.Run();
    }

    EXPECT_TRUE(callbacks->upgrade_called());
    EXPECT_TRUE(callbacks->connection());
    EXPECT_TRUE(callbacks->connection()->database());
    if (!callbacks->connection())
      return {nullptr, nullptr};

    // Finish the upgrade by committing the transaction.
    {
      base::RunLoop loop;
      callbacks->CallOnDBSuccess(
          base::BindLambdaForTesting([&]() { loop.Quit(); }));
      callbacks->connection()
          ->transactions()
          .find(transaction_id)
          ->second->SetCommitFlag();
      loop.Run();
    }
    return {callbacks->TakeConnection(), db_callbacks};
  }

  void RunPostedTasks() {
    base::RunLoop loop;
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     loop.QuitClosure());
    loop.Run();
  }

 protected:
  IndexedDBContextImpl* context() const { return context_.get(); }

  IndexedDBFactoryImpl* factory() const { return context_->GetIDBFactory(); }

  BrowserTaskEnvironment* task_environment() const {
    return task_environment_.get();
  }

  IndexedDBOriginState* OriginStateFromHandle(
      IndexedDBOriginStateHandle& handle) {
    return handle.origin_state();
  }

 private:
  std::unique_ptr<BrowserTaskEnvironment> task_environment_;

  base::ScopedTempDir temp_dir_;
  scoped_refptr<MockQuotaManagerProxy> quota_manager_proxy_;
  scoped_refptr<IndexedDBContextImpl> context_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBFactoryTest);
};

class IndexedDBFactoryTestWithMockTime : public IndexedDBFactoryTest {
 public:
  IndexedDBFactoryTestWithMockTime()
      : IndexedDBFactoryTest(std::make_unique<BrowserTaskEnvironment>(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME)) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(IndexedDBFactoryTestWithMockTime);
};

TEST_F(IndexedDBFactoryTest, BasicFactoryCreationAndTearDown) {
  SetupContext();

  const Origin origin1 = Origin::Create(GURL("http://localhost:81"));
  const Origin origin2 = Origin::Create(GURL("http://localhost:82"));

  IndexedDBOriginStateHandle origin_state1_handle;
  IndexedDBOriginStateHandle origin_state2_handle;
  leveldb::Status s;

  std::tie(origin_state1_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrOpenOriginFactory(origin1, context()->data_path(),
                                        /*create_if_missing=*/true);
  EXPECT_TRUE(origin_state1_handle.IsHeld()) << s.ToString();
  EXPECT_TRUE(s.ok()) << s.ToString();

  std::tie(origin_state2_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrOpenOriginFactory(origin2, context()->data_path(),
                                        /*create_if_missing=*/true);
  EXPECT_TRUE(origin_state2_handle.IsHeld()) << s.ToString();
  EXPECT_TRUE(s.ok()) << s.ToString();

  std::vector<StorageUsageInfo> origin_info = context()->GetAllOriginsInfo();
  EXPECT_EQ(2ul, origin_info.size());
  EXPECT_EQ(2ul, factory()->GetOpenOrigins().size());
}

TEST_F(IndexedDBFactoryTest, CloseSequenceStarts) {
  SetupContext();

  const Origin origin = Origin::Create(GURL("http://localhost:81"));

  IndexedDBOriginStateHandle origin_state_handle;
  leveldb::Status s;

  std::tie(origin_state_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrOpenOriginFactory(origin, context()->data_path(),
                                        /*create_if_missing=*/true);
  EXPECT_TRUE(origin_state_handle.IsHeld()) << s.ToString();
  origin_state_handle.Release();

  EXPECT_TRUE(factory()->GetOriginFactory(origin));
  EXPECT_TRUE(factory()->GetOriginFactory(origin)->IsClosing());

  factory()->ForceClose(origin, false);
  RunPostedTasks();
  EXPECT_FALSE(factory()->GetOriginFactory(origin));
}

TEST_F(IndexedDBFactoryTest, ImmediateClose) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      kIDBCloseImmediatelySwitch);
  SetupContext();

  const Origin origin = Origin::Create(GURL("http://localhost:81"));

  IndexedDBOriginStateHandle origin_state_handle;
  leveldb::Status s;

  std::tie(origin_state_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrOpenOriginFactory(origin, context()->data_path(),
                                        /*create_if_missing=*/true);
  EXPECT_TRUE(origin_state_handle.IsHeld()) << s.ToString();
  origin_state_handle.Release();

  EXPECT_TRUE(factory()->GetOriginFactory(origin));
  RunPostedTasks();
  EXPECT_FALSE(factory()->GetOriginFactory(origin));
  EXPECT_EQ(0ul, factory()->GetOpenOrigins().size());
}

TEST_F(IndexedDBFactoryTestWithMockTime, PreCloseTasksStart) {
  base::test::ScopedFeatureList feature_list;
  base::SimpleTestClock clock;
  clock.SetNow(base::Time::Now());
  SetupContextWithFactories(nullptr, &clock);

  const Origin origin = Origin::Create(GURL("http://localhost:81"));

  IndexedDBOriginStateHandle origin_state_handle;
  leveldb::Status s;

  // Open a connection & immediately release it to cause the closing sequence to
  // start.
  std::tie(origin_state_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrOpenOriginFactory(origin, context()->data_path(),
                                        /*create_if_missing=*/true);
  EXPECT_TRUE(origin_state_handle.IsHeld()) << s.ToString();
  origin_state_handle.Release();

  EXPECT_TRUE(factory()->GetOriginFactory(origin));
  EXPECT_TRUE(factory()->GetOriginFactory(origin)->IsClosing());

  EXPECT_EQ(IndexedDBOriginState::ClosingState::kPreCloseGracePeriod,
            factory()->GetOriginFactory(origin)->closing_stage());

  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(2));

  // The factory should be closed, as the pre close tasks are delayed.
  EXPECT_FALSE(factory()->GetOriginFactory(origin));

  // Move the clock to run the tasks in the next close sequence.
  clock.Advance(IndexedDBOriginState::kMaxEarliestGlobalSweepFromNow);

  // Open a connection & immediately release it to cause the closing sequence to
  // start again.
  std::tie(origin_state_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrOpenOriginFactory(origin, context()->data_path(),
                                        /*create_if_missing=*/true);
  EXPECT_TRUE(origin_state_handle.IsHeld()) << s.ToString();
  origin_state_handle.Release();

  // Manually execute the timer so that the PreCloseTaskList task doesn't also
  // run.
  factory()->GetOriginFactory(origin)->close_timer()->FireNow();

  // The pre-close tasks should be running now.
  ASSERT_TRUE(factory()->GetOriginFactory(origin));
  EXPECT_EQ(IndexedDBOriginState::ClosingState::kRunningPreCloseTasks,
            factory()->GetOriginFactory(origin)->closing_stage());
  ASSERT_TRUE(factory()->GetOriginFactory(origin)->pre_close_task_queue());
  EXPECT_TRUE(
      factory()->GetOriginFactory(origin)->pre_close_task_queue()->started());

  // Stop sweep by opening a connection.
  std::tie(origin_state_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrOpenOriginFactory(origin, context()->data_path(),
                                        /*create_if_missing=*/true);
  EXPECT_TRUE(origin_state_handle.IsHeld()) << s.ToString();
  EXPECT_FALSE(
      OriginStateFromHandle(origin_state_handle)->pre_close_task_queue());
  origin_state_handle.Release();

  // Move clock forward to trigger next sweep, but origin has longer
  // sweep minimum, so no tasks should execute.
  clock.Advance(IndexedDBOriginState::kMaxEarliestGlobalSweepFromNow);

  origin_state_handle.Release();
  EXPECT_TRUE(factory()->GetOriginFactory(origin));
  EXPECT_EQ(IndexedDBOriginState::ClosingState::kPreCloseGracePeriod,
            factory()->GetOriginFactory(origin)->closing_stage());

  // Manually execute the timer so that the PreCloseTaskList task doesn't also
  // run.
  factory()->GetOriginFactory(origin)->close_timer()->FireNow();
  EXPECT_TRUE(factory()->GetOriginFactory(origin));
  RunPostedTasks();
  EXPECT_FALSE(factory()->GetOriginFactory(origin));

  //  Finally, move the clock forward so the origin should allow a sweep.
  clock.Advance(IndexedDBOriginState::kMaxEarliestOriginSweepFromNow);
  std::tie(origin_state_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrOpenOriginFactory(origin, context()->data_path(),
                                        /*create_if_missing=*/true);
  origin_state_handle.Release();
  factory()->GetOriginFactory(origin)->close_timer()->FireNow();

  ASSERT_TRUE(factory()->GetOriginFactory(origin));
  EXPECT_EQ(IndexedDBOriginState::ClosingState::kRunningPreCloseTasks,
            factory()->GetOriginFactory(origin)->closing_stage());
  ASSERT_TRUE(factory()->GetOriginFactory(origin)->pre_close_task_queue());
  EXPECT_TRUE(
      factory()->GetOriginFactory(origin)->pre_close_task_queue()->started());
}

TEST_F(IndexedDBFactoryTest, InMemoryFactoriesStay) {
  SetupInMemoryContext();
  ASSERT_TRUE(context()->IsInMemoryContext());

  const Origin origin = Origin::Create(GURL("http://localhost:81"));

  IndexedDBOriginStateHandle origin_state_handle;
  leveldb::Status s;

  std::tie(origin_state_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrOpenOriginFactory(origin, context()->data_path(),
                                        /*create_if_missing=*/true);
  EXPECT_TRUE(origin_state_handle.IsHeld()) << s.ToString();
  EXPECT_TRUE(OriginStateFromHandle(origin_state_handle)
                  ->backing_store()
                  ->is_incognito());
  origin_state_handle.Release();

  EXPECT_TRUE(factory()->GetOriginFactory(origin));
  EXPECT_FALSE(factory()->GetOriginFactory(origin)->IsClosing());

  factory()->ForceClose(origin, false);
  EXPECT_TRUE(factory()->GetOriginFactory(origin));

  factory()->ForceClose(origin, true);
  EXPECT_FALSE(factory()->GetOriginFactory(origin));
}

TEST_F(IndexedDBFactoryTest, TooLongOrigin) {
  SetupContext();

  base::FilePath temp_dir = context()->data_path().DirName();
  int limit = base::GetMaximumPathComponentLength(temp_dir);
  EXPECT_GT(limit, 0);

  std::string origin(limit + 1, 'x');
  Origin too_long_origin = Origin::Create(GURL("http://" + origin + ":81/"));

  IndexedDBOriginStateHandle origin_state_handle;
  leveldb::Status s;

  std::tie(origin_state_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrOpenOriginFactory(too_long_origin, context()->data_path(),
                                        /*create_if_missing=*/true);
  EXPECT_FALSE(origin_state_handle.IsHeld());
  EXPECT_TRUE(s.IsIOError());
}

TEST_F(IndexedDBFactoryTest, ContextDestructionClosesConnections) {
  SetupContext();
  const Origin origin = Origin::Create(GURL("http://localhost:81"));

  auto callbacks = base::MakeRefCounted<MockIndexedDBCallbacks>();
  auto db_callbacks = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();

  const int64_t transaction_id = 1;
  auto create_transaction_callback =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  auto connection = std::make_unique<IndexedDBPendingConnection>(
      callbacks, db_callbacks,
      IndexedDBExecutionContextConnectionTracker::Handle::CreateForTesting(),
      transaction_id, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      std::move(create_transaction_callback));
  factory()->Open(ASCIIToUTF16("db"), std::move(connection), origin,
                  context()->data_path());
  RunPostedTasks();

  // Now simulate shutdown, which should clear all factories.
  factory()->ContextDestroyed();
  EXPECT_TRUE(db_callbacks->forced_close_called());
}

TEST_F(IndexedDBFactoryTest, ContextDestructionClosesHandles) {
  SetupContext();
  const Origin origin = Origin::Create(GURL("http://localhost:81"));

  IndexedDBOriginStateHandle origin_state_handle;
  leveldb::Status s;

  std::tie(origin_state_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrOpenOriginFactory(origin, context()->data_path(),
                                        /*create_if_missing=*/true);
  EXPECT_TRUE(origin_state_handle.IsHeld()) << s.ToString();

  // Now simulate shutdown, which should clear all factories.
  factory()->ContextDestroyed();
  EXPECT_FALSE(OriginStateFromHandle(origin_state_handle));
  EXPECT_FALSE(factory()->GetOriginFactory(origin));
}

TEST_F(IndexedDBFactoryTest, FactoryForceClose) {
  SetupContext();
  const Origin origin = Origin::Create(GURL("http://localhost:81"));

  IndexedDBOriginStateHandle origin_state_handle;
  leveldb::Status s;

  std::tie(origin_state_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrOpenOriginFactory(origin, context()->data_path(),
                                        /*create_if_missing=*/true);
  EXPECT_TRUE(origin_state_handle.IsHeld()) << s.ToString();

  OriginStateFromHandle(origin_state_handle)->ForceClose();
  origin_state_handle.Release();

  EXPECT_TRUE(factory()->GetOriginFactory(origin));
  RunPostedTasks();
  EXPECT_FALSE(factory()->GetOriginFactory(origin));
}

TEST_F(IndexedDBFactoryTest, ConnectionForceClose) {
  SetupContext();
  const Origin origin = Origin::Create(GURL("http://localhost:81"));

  auto callbacks = base::MakeRefCounted<MockIndexedDBCallbacks>();
  auto db_callbacks = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();

  const int64_t transaction_id = 1;
  auto create_transaction_callback =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  auto connection = std::make_unique<IndexedDBPendingConnection>(
      callbacks, db_callbacks,
      IndexedDBExecutionContextConnectionTracker::Handle::CreateForTesting(),
      transaction_id, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      std::move(create_transaction_callback));
  factory()->Open(ASCIIToUTF16("db"), std::move(connection), origin,
                  context()->data_path());
  EXPECT_FALSE(callbacks->connection());
  RunPostedTasks();
  EXPECT_TRUE(callbacks->connection());

  EXPECT_TRUE(factory()->GetOriginFactory(origin));
  EXPECT_FALSE(factory()->GetOriginFactory(origin)->IsClosing());

  callbacks->connection()->CloseAndReportForceClose();

  EXPECT_TRUE(factory()->GetOriginFactory(origin));
  EXPECT_TRUE(factory()->GetOriginFactory(origin)->IsClosing());

  EXPECT_TRUE(db_callbacks->forced_close_called());
}

TEST_F(IndexedDBFactoryTest, DatabaseForceCloseDuringUpgrade) {
  SetupContext();
  const Origin origin = Origin::Create(GURL("http://localhost:81"));

  auto callbacks = base::MakeRefCounted<MockIndexedDBCallbacks>();
  auto db_callbacks = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();

  const int64_t transaction_id = 1;
  auto create_transaction_callback =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  auto connection = std::make_unique<IndexedDBPendingConnection>(
      callbacks, db_callbacks,
      IndexedDBExecutionContextConnectionTracker::Handle::CreateForTesting(),
      transaction_id, IndexedDBDatabaseMetadata::NO_VERSION,
      std::move(create_transaction_callback));

  // Do the first half of the upgrade, and request the upgrade from renderer.
  {
    base::RunLoop loop;
    callbacks->CallOnUpgradeNeeded(
        base::BindLambdaForTesting([&]() { loop.Quit(); }));
    factory()->Open(ASCIIToUTF16("db"), std::move(connection), origin,
                    context()->data_path());
    loop.Run();
  }

  EXPECT_TRUE(callbacks->upgrade_called());
  ASSERT_TRUE(callbacks->connection());
  ASSERT_TRUE(callbacks->connection()->database());

  callbacks->connection()->database()->ForceCloseAndRunTasks();

  EXPECT_TRUE(db_callbacks->forced_close_called());
  // Since there are no more references the factory should be closing.
  EXPECT_TRUE(factory()->GetOriginFactory(origin));
  EXPECT_TRUE(factory()->GetOriginFactory(origin)->IsClosing());
}

TEST_F(IndexedDBFactoryTest, ConnectionCloseDuringUpgrade) {
  SetupContext();
  const Origin origin = Origin::Create(GURL("http://localhost:81"));

  auto callbacks = base::MakeRefCounted<MockIndexedDBCallbacks>();
  auto db_callbacks = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();

  const int64_t transaction_id = 1;
  auto create_transaction_callback =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  auto connection = std::make_unique<IndexedDBPendingConnection>(
      callbacks, db_callbacks,
      IndexedDBExecutionContextConnectionTracker::Handle::CreateForTesting(),
      transaction_id, IndexedDBDatabaseMetadata::NO_VERSION,
      std::move(create_transaction_callback));

  // Do the first half of the upgrade, and request the upgrade from renderer.
  {
    base::RunLoop loop;
    callbacks->CallOnUpgradeNeeded(
        base::BindLambdaForTesting([&]() { loop.Quit(); }));
    factory()->Open(ASCIIToUTF16("db"), std::move(connection), origin,
                    context()->data_path());
    loop.Run();
  }

  EXPECT_TRUE(callbacks->upgrade_called());
  ASSERT_TRUE(callbacks->connection());

  // Close the connection.
  callbacks->connection()->AbortTransactionsAndClose(
      IndexedDBConnection::CloseErrorHandling::kAbortAllReturnLastError);

  // Since there are no more references the factory should be closing.
  EXPECT_TRUE(factory()->GetOriginFactory(origin));
  EXPECT_TRUE(factory()->GetOriginFactory(origin)->IsClosing());
}

TEST_F(IndexedDBFactoryTest, DatabaseForceCloseWithFullConnection) {
  SetupContext();
  const Origin origin = Origin::Create(GURL("http://localhost:81"));

  std::unique_ptr<IndexedDBConnection> connection;
  scoped_refptr<MockIndexedDBDatabaseCallbacks> db_callbacks;
  std::tie(connection, db_callbacks) =
      CreateConnectionForDatatabase(origin, ASCIIToUTF16("db"));

  // Force close the database.
  connection->database()->ForceCloseAndRunTasks();

  EXPECT_TRUE(db_callbacks->forced_close_called());
  // Since there are no more references the factory should be closing.
  EXPECT_TRUE(factory()->GetOriginFactory(origin));
  EXPECT_TRUE(factory()->GetOriginFactory(origin)->IsClosing());
}

TEST_F(IndexedDBFactoryTest, DeleteDatabase) {
  SetupContext();

  auto callbacks = base::MakeRefCounted<MockIndexedDBCallbacks>(
      /*expect_connection=*/false);

  const Origin origin = Origin::Create(GURL("http://localhost:81"));

  factory()->DeleteDatabase(ASCIIToUTF16("db"), callbacks, origin,
                            context()->data_path(),
                            /*force_close=*/false);

  // Since there are no more references the factory should be closing.
  EXPECT_TRUE(factory()->GetOriginFactory(origin));
  EXPECT_TRUE(factory()->GetOriginFactory(origin)->IsClosing());
}

TEST_F(IndexedDBFactoryTest, DeleteDatabaseWithForceClose) {
  SetupContext();

  const Origin origin = Origin::Create(GURL("http://localhost:81"));
  const base::string16 name = ASCIIToUTF16("db");

  std::unique_ptr<IndexedDBConnection> connection;
  scoped_refptr<MockIndexedDBDatabaseCallbacks> db_callbacks;
  std::tie(connection, db_callbacks) =
      CreateConnectionForDatatabase(origin, name);

  auto callbacks = base::MakeRefCounted<MockIndexedDBCallbacks>(
      /*expect_connection=*/false);

  factory()->DeleteDatabase(name, callbacks, origin, context()->data_path(),
                            /*force_close=*/true);

  // Force close means the connection has been force closed, but the factory
  // isn't force closed, and instead is going through it's shutdown sequence.
  EXPECT_FALSE(connection->IsConnected());
  EXPECT_TRUE(db_callbacks->forced_close_called());
  EXPECT_TRUE(factory()->GetOriginFactory(origin));
  EXPECT_TRUE(factory()->GetOriginFactory(origin)->IsClosing());
}

TEST_F(IndexedDBFactoryTest, GetDatabaseNames) {
  SetupContext();

  auto callbacks = base::MakeRefCounted<MockIndexedDBCallbacks>(
      /*expect_connection=*/false);

  const Origin origin = Origin::Create(GURL("http://localhost:81"));

  factory()->GetDatabaseInfo(callbacks, origin, context()->data_path());

  EXPECT_TRUE(callbacks->info_called());
  // Since there are no more references the factory should be closing.
  EXPECT_TRUE(factory()->GetOriginFactory(origin));
  EXPECT_TRUE(factory()->GetOriginFactory(origin)->IsClosing());
}

class LookingForQuotaErrorMockCallbacks : public IndexedDBCallbacks {
 public:
  LookingForQuotaErrorMockCallbacks()
      : IndexedDBCallbacks(nullptr,
                           url::Origin(),
                           mojo::NullAssociatedRemote(),
                           base::SequencedTaskRunnerHandle::Get()),
        error_called_(false) {}
  void OnError(const IndexedDBDatabaseError& error) override {
    error_called_ = true;
    EXPECT_EQ(blink::mojom::IDBException::kQuotaError, error.code());
  }
  bool error_called() const { return error_called_; }

 private:
  ~LookingForQuotaErrorMockCallbacks() override {}
  bool error_called_;

  DISALLOW_COPY_AND_ASSIGN(LookingForQuotaErrorMockCallbacks);
};

TEST_F(IndexedDBFactoryTest, QuotaErrorOnDiskFull) {
  FakeLevelDBFactory fake_ldb_factory(
      IndexedDBClassFactory::GetLevelDBOptions(), "indexed-db");
  fake_ldb_factory.EnqueueNextOpenLevelDBStateResult(
      nullptr, leveldb::Status::IOError("Disk is full."), true);
  SetupContextWithFactories(&fake_ldb_factory,
                            base::DefaultClock::GetInstance());

  auto callbacks = base::MakeRefCounted<LookingForQuotaErrorMockCallbacks>();
  auto dummy_database_callbacks =
      base::MakeRefCounted<IndexedDBDatabaseCallbacks>(
          nullptr, mojo::NullAssociatedRemote(), context()->TaskRunner());
  const Origin origin = Origin::Create(GURL("http://localhost:81"));
  const base::string16 name(ASCIIToUTF16("name"));
  auto create_transaction_callback =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  auto connection = std::make_unique<IndexedDBPendingConnection>(
      callbacks, dummy_database_callbacks,
      IndexedDBExecutionContextConnectionTracker::Handle::CreateForTesting(),
      /*transaction_id=*/1, /*version=*/1,
      std::move(create_transaction_callback));
  factory()->Open(name, std::move(connection), origin, context()->data_path());
  EXPECT_TRUE(callbacks->error_called());
}

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
  SetupContext();
  const Origin origin = Origin::Create(GURL("http://localhost:81"));
  const base::string16 db_name(ASCIIToUTF16("db"));
  const int64_t transaction_id = 1;

  auto callbacks = base::MakeRefCounted<MockIndexedDBCallbacks>();
  auto db_callbacks = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  auto failed_open_callbacks = base::MakeRefCounted<ErrorCallbacks>();
  auto db_callbacks2 = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();

  // Open at version 2.
  const int64_t db_version = 2;
  auto create_transaction_callback =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);

  auto connection = std::make_unique<IndexedDBPendingConnection>(
      callbacks, db_callbacks,
      IndexedDBExecutionContextConnectionTracker::Handle::CreateForTesting(),
      transaction_id, db_version, std::move(create_transaction_callback));
  {
    base::RunLoop loop;
    callbacks->CallOnUpgradeNeeded(
        base::BindLambdaForTesting([&]() { loop.Quit(); }));
    factory()->Open(db_name, std::move(connection), origin,
                    context()->data_path());
    loop.Run();
  }
  EXPECT_TRUE(callbacks->upgrade_called());
  EXPECT_TRUE(factory()->IsDatabaseOpen(origin, db_name));

  // Finish connecting, then close the connection.
  {
    base::RunLoop loop;
    callbacks->CallOnDBSuccess(
        base::BindLambdaForTesting([&]() { loop.Quit(); }));
    EXPECT_TRUE(callbacks->connection());
    callbacks->connection()->database()->Commit(
        callbacks->connection()->GetTransaction(transaction_id));
    loop.Run();
    callbacks->connection()->AbortTransactionsAndClose(
        IndexedDBConnection::CloseErrorHandling::kAbortAllReturnLastError);
    RunPostedTasks();
    EXPECT_FALSE(factory()->IsDatabaseOpen(origin, db_name));
  }

  // Open at version < 2, which will fail.
  {
    const int64_t db_version = 1;
    auto create_transaction_callback =
        base::BindOnce(&CreateAndBindTransactionPlaceholder);
    auto connection = std::make_unique<IndexedDBPendingConnection>(
        failed_open_callbacks, db_callbacks2,
        IndexedDBExecutionContextConnectionTracker::Handle::CreateForTesting(),
        transaction_id, db_version, std::move(create_transaction_callback));
    factory()->Open(db_name, std::move(connection), origin,
                    context()->data_path());
    EXPECT_TRUE(factory()->IsDatabaseOpen(origin, db_name));
    RunPostedTasks();
    EXPECT_TRUE(failed_open_callbacks->saw_error());
    EXPECT_FALSE(factory()->IsDatabaseOpen(origin, db_name));
  }
}

namespace {

class DataLossCallbacks final : public MockIndexedDBCallbacks {
 public:
  blink::mojom::IDBDataLoss data_loss() const { return data_loss_; }

  void OnError(const IndexedDBDatabaseError& error) final {
    ADD_FAILURE() << "Unexpected IDB error: " << error.message();
  }
  void OnUpgradeNeeded(int64_t old_version,
                       std::unique_ptr<IndexedDBConnection> connection,
                       const IndexedDBDatabaseMetadata& metadata,
                       const IndexedDBDataLossInfo& data_loss) final {
    data_loss_ = data_loss.status;
    MockIndexedDBCallbacks::OnUpgradeNeeded(old_version, std::move(connection),
                                            metadata, data_loss);
  }

 private:
  ~DataLossCallbacks() final {}
  blink::mojom::IDBDataLoss data_loss_ = blink::mojom::IDBDataLoss::None;
};

TEST_F(IndexedDBFactoryTest, DataFormatVersion) {
  SetupContext();
  auto try_open = [this](const Origin& origin,
                         const IndexedDBDataFormatVersion& version) {
    base::AutoReset<IndexedDBDataFormatVersion> override_version(
        &IndexedDBDataFormatVersion::GetMutableCurrentForTesting(), version);

    const int64_t transaction_id = 1;
    auto callbacks = base::MakeRefCounted<DataLossCallbacks>();
    auto db_callbacks = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
    auto create_transaction_callback =
        base::BindOnce(&CreateAndBindTransactionPlaceholder);
    auto pending_connection = std::make_unique<IndexedDBPendingConnection>(
        callbacks, db_callbacks,
        IndexedDBExecutionContextConnectionTracker::Handle::CreateForTesting(),
        transaction_id,
        /*version=*/1, std::move(create_transaction_callback));

    {
      base::RunLoop loop;
      bool upgraded = false;
      // The database might already exist. Wait until either a success or an
      // ugprade request.
      callbacks->CallOnUpgradeNeeded(base::BindLambdaForTesting([&]() {
        upgraded = true;
        loop.Quit();
      }));
      callbacks->CallOnDBSuccess(
          base::BindLambdaForTesting([&]() { loop.Quit(); }));

      this->factory()->Open(ASCIIToUTF16("test_db"),
                            std::move(pending_connection), origin,
                            context()->data_path());
      loop.Run();

      // If an upgrade was requested, then commit the upgrade transaction.
      if (upgraded) {
        EXPECT_TRUE(callbacks->upgrade_called());
        EXPECT_TRUE(callbacks->connection());
        EXPECT_TRUE(callbacks->connection()->database());
        // Finish the upgrade by committing the transaction.
        auto* connection = callbacks->connection();
        {
          base::RunLoop inner_loop;
          callbacks->CallOnDBSuccess(
              base::BindLambdaForTesting([&]() { inner_loop.Quit(); }));
          connection->database()->Commit(
              connection->GetTransaction(transaction_id));
          inner_loop.Run();
        }
      }
    }
    RunPostedTasks();
    factory()->ForceClose(origin, false);
    RunPostedTasks();
    return callbacks->data_loss();
  };

  static const struct {
    const char* origin;
    IndexedDBDataFormatVersion open_version_1;
    IndexedDBDataFormatVersion open_version_2;
    blink::mojom::IDBDataLoss expected_data_loss;
  } kTestCases[] = {{"http://blink-downgrade.com/",
                     {3, 4},
                     {3, 3},
                     blink::mojom::IDBDataLoss::Total}};
  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.origin);
    const Origin origin = Origin::Create(GURL(test.origin));
    ASSERT_EQ(blink::mojom::IDBDataLoss::None,
              try_open(origin, test.open_version_1));
    EXPECT_EQ(test.expected_data_loss, try_open(origin, test.open_version_2));
  }
}

}  // namespace
}  // namespace content
