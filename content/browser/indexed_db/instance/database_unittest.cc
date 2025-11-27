// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/database.h"

#include <stdint.h>

#include <array>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/containers/span.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "content/browser/indexed_db/instance/bucket_context.h"
#include "content/browser/indexed_db/instance/connection.h"
#include "content/browser/indexed_db/instance/database_callbacks.h"
#include "content/browser/indexed_db/instance/mock_blob_storage_context.h"
#include "content/browser/indexed_db/instance/mock_file_system_access_context.h"
#include "content/browser/indexed_db/instance/transaction.h"
#include "content/browser/indexed_db/mock_mojo_indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/mock_mojo_indexed_db_factory_client.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/test_support/fake_message_dispatch_context.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::IndexedDBDatabaseMetadata;
using blink::IndexedDBIndexKeys;

namespace content::indexed_db {

namespace {
constexpr char kTestForceCloseMessage[] =
    "The database's connection is force-closed.";

ACTION_TEMPLATE(MoveArgPointee,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(out)) {
  *out = std::move(*::testing::get<k>(args));
}

}  // namespace

class DatabaseTest : public ::testing::Test,
                     public testing::WithParamInterface<bool> {
 public:
  DatabaseTest()
      : sqlite_override_(BucketContext::OverrideShouldUseSqliteForTesting(
            IsSqliteBackingStoreEnabled())) {}

  bool IsSqliteBackingStoreEnabled() { return GetParam(); }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        /*is_incognito=*/false, temp_dir_.GetPath(),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        /*special_storage_policy=*/nullptr);

    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        quota_manager_.get(),
        base::SingleThreadTaskRunner::GetCurrentDefault().get());

    BucketContext::Delegate delegate;
    delegate.on_ready_for_destruction =
        base::BindOnce(&DatabaseTest::OnBucketContextReadyForDestruction,
                       weak_factory_.GetWeakPtr());

    mojo::PendingRemote<storage::mojom::BlobStorageContext>
        blob_storage_context;
    blob_storage_context_.Clone(
        blob_storage_context.InitWithNewPipeAndPassReceiver());
    mojo::PendingRemote<storage::mojom::FileSystemAccessContext> fsa_context;
    file_system_access_context_ =
        std::make_unique<test::MockFileSystemAccessContext>();
    file_system_access_context_->Clone(
        fsa_context.InitWithNewPipeAndPassReceiver());

    bucket_context_ = std::make_unique<BucketContext>(
        storage::BucketInfo(), temp_dir_.GetPath(), std::move(delegate),
        quota_manager_proxy_,
        /*blob_storage_context=*/std::move(blob_storage_context),
        /*file_system_access_context=*/std::move(fsa_context));

    bucket_context_->InitBackingStore(true);
    db_ = bucket_context_->CreateAndAddDatabase(u"db");
  }

  void TearDown() override { db_ = nullptr; }

  void OnBucketContextReadyForDestruction() { bucket_context_.reset(); }

  void RunPostedTasks() {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

 protected:
  base::AutoReset<std::optional<bool>> sqlite_override_;
  base::test::TaskEnvironment task_environment_;

  base::ScopedTempDir temp_dir_;
  MockBlobStorageContext blob_storage_context_;
  std::unique_ptr<test::MockFileSystemAccessContext>
      file_system_access_context_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy_;
  std::unique_ptr<BucketContext> bucket_context_;

  // As this is owned by `bucket_context_`, tests that cause the database to
  // be destroyed must manually reset this to null to avoid triggering dangling
  // pointer warnings.
  raw_ptr<Database> db_ = nullptr;

  base::WeakPtrFactory<DatabaseTest> weak_factory_{this};
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    DatabaseTest,
    /*use SQLite backing store*/ testing::Bool(),
    [](const testing::TestParamInfo<DatabaseTest::ParamType>& info) {
      return info.param ? "SQLite" : "LevelDB";
    });

TEST_P(DatabaseTest, ConnectionLifecycle) {
  MockMojoDatabaseCallbacks database_callbacks;
  MockMojoFactoryClient request;
  auto non_associated = request.CreateInterfacePtrAndBind();
  non_associated.EnableUnassociatedUsage();

  base::RunLoop run_loop;
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_connection;
  EXPECT_CALL(request, MockedUpgradeNeeded)
      .WillOnce(
          testing::DoAll(MoveArgPointee<0>(&pending_connection),
                         ::base::test::RunClosure(run_loop.QuitClosure())));
  const int64_t transaction_id1 = 1;

  auto connection1 = std::make_unique<PendingConnection>(
      mojo::AssociatedRemote<blink::mojom::IDBFactoryClient>(
          std::move(non_associated)),
      std::make_unique<DatabaseCallbacks>(
          database_callbacks.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id1, IndexedDBDatabaseMetadata::NO_VERSION,
      mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection1));
  db_ = nullptr;
  run_loop.Run();

  pending_connection.reset();

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return bucket_context_->GetDatabasesForTesting().empty(); }));
}

TEST_P(DatabaseTest, ForcedClose) {
  MockMojoDatabaseCallbacks database_callbacks;
  MockMojoFactoryClient request;
  auto non_associated = request.CreateInterfacePtrAndBind();
  non_associated.EnableUnassociatedUsage();

  const int64_t upgrade_transaction_id = 3;
  auto connection = std::make_unique<PendingConnection>(
      mojo::AssociatedRemote<blink::mojom::IDBFactoryClient>(
          std::move(non_associated)),
      std::make_unique<DatabaseCallbacks>(
          database_callbacks.BindNewEndpointAndPassDedicatedRemote()),
      upgrade_transaction_id, IndexedDBDatabaseMetadata::NO_VERSION,
      mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection));
  db_ = nullptr;

  base::RunLoop run_loop;
  EXPECT_CALL(request, Error);
  EXPECT_CALL(database_callbacks, ForcedClose)
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  bucket_context_->ForceClose(false, kTestForceCloseMessage);
  run_loop.Run();
}

TEST_P(DatabaseTest, ForceCloseWithConnectionsInVariousStates) {
  MockMojoFactoryClient request;
  auto non_associated = request.CreateInterfacePtrAndBind();
  non_associated.EnableUnassociatedUsage();
  EXPECT_CALL(request, MockedUpgradeNeeded);

  MockMojoDatabaseCallbacks database_callbacks;
  const int64_t transaction_id1 = 1;
  auto connection = std::make_unique<PendingConnection>(
      mojo::AssociatedRemote<blink::mojom::IDBFactoryClient>(
          std::move(non_associated)),
      std::make_unique<DatabaseCallbacks>(
          database_callbacks.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id1, IndexedDBDatabaseMetadata::NO_VERSION,
      mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection));
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 0UL);

  MockMojoFactoryClient request2;
  EXPECT_CALL(request2, Error);
  auto non_associated2 = request2.CreateInterfacePtrAndBind();
  non_associated2.EnableUnassociatedUsage();
  MockMojoDatabaseCallbacks database_callbacks2;
  EXPECT_CALL(database_callbacks2, ForcedClose);
  EXPECT_CALL(database_callbacks2, Abort);
  const int64_t transaction_id2 = 2;
  auto connection2 = std::make_unique<PendingConnection>(
      mojo::AssociatedRemote<blink::mojom::IDBFactoryClient>(
          std::move(non_associated2)),
      std::make_unique<DatabaseCallbacks>(
          database_callbacks2.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id2, /*version=*/3, mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection2));
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 1UL);

  MockMojoFactoryClient request3;
  auto non_associated3 = request3.CreateInterfacePtrAndBind();
  non_associated3.EnableUnassociatedUsage();

  // Delete succeeds as the database didn't successfully make it through
  // creation.
  base::RunLoop delete_success_loop;
  EXPECT_CALL(request3, DeleteSuccess)
      .WillOnce(::base::test::RunClosure(delete_success_loop.QuitClosure()));
  EXPECT_CALL(request3, Blocked).Times(0);
  db_->ScheduleDeleteDatabase(
      mojo::AssociatedRemote<blink::mojom::IDBFactoryClient>(
          std::move(non_associated3)),
      /*on_deletion_complete=*/base::DoNothing());
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 1UL);
  db_ = nullptr;

  bucket_context_->ForceClose(false, kTestForceCloseMessage);
  delete_success_loop.Run();

  // Wait for various mock expectations.
  RunPostedTasks();
}

// Verifies that a bad parameter (in this case, a version change transaction
// type) passed in a mojo call will cause an error to be reported.
TEST_P(DatabaseTest, MojomWithInvalidParameter) {
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;
  MockMojoDatabaseCallbacks database_callbacks;
  MockMojoFactoryClient request;
  auto non_associated = request.CreateInterfacePtrAndBind();
  non_associated.EnableUnassociatedUsage();

  base::RunLoop run_loop;
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_connection;
  EXPECT_CALL(request, MockedUpgradeNeeded)
      .WillOnce(
          testing::DoAll(MoveArgPointee<0>(&pending_connection),
                         ::base::test::RunClosure(run_loop.QuitClosure())));
  const int64_t transaction_id1 = 1;

  auto connection = std::make_unique<PendingConnection>(
      mojo::AssociatedRemote<blink::mojom::IDBFactoryClient>(
          std::move(non_associated)),
      std::make_unique<DatabaseCallbacks>(
          database_callbacks.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id1, IndexedDBDatabaseMetadata::NO_VERSION,
      mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection));
  db_ = nullptr;
  run_loop.Run();

  mojo::AssociatedRemote<blink::mojom::IDBDatabase> mojo_connection(
      std::move(pending_connection));
  mojo::PendingAssociatedRemote<blink::mojom::IDBTransaction>
      pending_transaction;
  mojo_connection->CreateTransaction(
      pending_transaction.InitWithNewEndpointAndPassReceiver(),
      /*transaction_id=*/2, {}, blink::mojom::IDBTransactionMode::VersionChange,
      blink::mojom::IDBTransactionDurability::Strict);

  EXPECT_EQ("Bad transaction mode", bad_message_observer.WaitForBadMessage());

  // This test also verifies that a bad message which is received by the
  // `Connection`, and which leads to killing the renderer, will not leak the
  // `Connection`. This is a risk because the `Connection` is self-owned, but
  // dispatching a bad message via `mojom::ReportBadMessage()` will not run
  // a disconnect handler, including the one that `SelfOwnedAssociatedReceiver`
  // uses to delete itself (and the `Connection`). A leak of `Connection` at the
  // wrong moment is particularly pernicious as `Connection` owns transactions,
  // which in the LevelDB world, can hold references to the `LevelDBState`,
  // which in turn will block `BackingStore` teardown, causing this test to
  // timeout during destruction.
  //
  // (The SQLite backing store does not have crazy reference counting issues.)
}

}  // namespace content::indexed_db
