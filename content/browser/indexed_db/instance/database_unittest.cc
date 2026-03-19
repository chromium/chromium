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

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "content/browser/indexed_db/indexed_db_test_base.h"
#include "content/browser/indexed_db/instance/bucket_context.h"
#include "content/browser/indexed_db/instance/connection.h"
#include "content/browser/indexed_db/instance/database_callbacks.h"
#include "content/browser/indexed_db/instance/transaction.h"
#include "content/browser/indexed_db/mock_mojo_indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/mock_mojo_indexed_db_factory_client.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/test_support/fake_message_dispatch_context.h"
#include "mojo/public/cpp/test_support/test_utils.h"
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

class DatabaseTest : public IndexedDBTestBase,
                     public testing::WithParamInterface<bool> {
 public:
  DatabaseTest()
      : IndexedDBTestBase(/*use_default_buckets=*/true,
                          /*use_sqlite=*/GetParam()) {}

  void SetUp() override {
    IndexedDBTestBase::SetUp();

    bucket_context_ = InitBucketContext(GetTestStorageKey()).AsWeakPtr();
    bucket_context_->InitBackingStore(/*create_if_missing=*/true);
    db_ = bucket_context_->CreateAndAddDatabase(u"db");
  }

  void TearDown() override {
    db_ = nullptr;
    IndexedDBTestBase::TearDown();
  }

 protected:
  base::WeakPtr<BucketContext> bucket_context_;

  // As this is owned by the BucketContext, tests that cause the database
  // to be destroyed must manually reset this to null to avoid triggering
  // dangling pointer warnings.
  raw_ptr<Database> db_ = nullptr;
};

INSTANTIATE_TEST_SUITE_P(
    IndexedDB,
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
  db_->ScheduleOpenConnection(std::move(connection1),
                              /*synchronous_duration=*/{});
  db_ = nullptr;
  run_loop.Run();

  pending_connection.reset();

  ASSERT_TRUE(base::test::RunUntil([&]() { return !bucket_context_; }));
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
  db_->ScheduleOpenConnection(std::move(connection),
                              /*synchronous_duration=*/{});
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
  db_->ScheduleOpenConnection(std::move(connection),
                              /*synchronous_duration=*/{});
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
  db_->ScheduleOpenConnection(std::move(connection2),
                              /*synchronous_duration=*/{});
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 1UL);

  MockMojoFactoryClient request3;
  auto non_associated3 = request3.CreateInterfacePtrAndBind();
  non_associated3.EnableUnassociatedUsage();

  base::RunLoop delete_loop;
  EXPECT_CALL(request3, Error)
      .WillOnce(::base::test::RunClosure(delete_loop.QuitClosure()));
  EXPECT_CALL(request3, Blocked).Times(0);
  db_->ScheduleDeleteDatabase(
      mojo::AssociatedRemote<blink::mojom::IDBFactoryClient>(
          std::move(non_associated3)),
      /*on_deletion_complete=*/base::DoNothing(),
      /*synchronous_duration=*/{});
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 1UL);
  db_ = nullptr;

  bucket_context_->ForceClose(false, kTestForceCloseMessage);
  delete_loop.Run();

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
  db_->ScheduleOpenConnection(std::move(connection),
                              /*synchronous_duration=*/{});
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
