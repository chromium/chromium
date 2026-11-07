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
#include "components/services/storage/privileged/cpp/bucket_client_info.h"
#include "content/browser/indexed_db/indexed_db_test_base.h"
#include "content/browser/indexed_db/instance/bucket_context.h"
#include "content/browser/indexed_db/instance/connection.h"
#include "content/browser/indexed_db/instance/database_callbacks.h"
#include "content/browser/indexed_db/instance/pending_connection.h"
#include "content/browser/indexed_db/instance/transaction.h"
#include "content/browser/indexed_db/mock_mojo_indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/mock_mojo_indexed_db_factory_client.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/test_support/fake_message_dispatch_context.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"

using blink::IndexedDBDatabaseMetadata;
using blink::IndexedDBIndexKeys;

namespace content::indexed_db {

namespace {

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

    bucket_context_ = InitBucketContext();
    db_ = bucket_context_->CreateAndAddDatabase(u"db");
  }

  void TearDown() override {
    db_ = nullptr;
    IndexedDBTestBase::TearDown();
  }

 protected:
  void ScheduleOpen(
      MockMojoFactoryClient& factory_client,
      const storage::BucketClientInfo& client_info,
      int64_t transaction_id,
      int64_t version,
      std::optional<int64_t> shared_connection_id = std::nullopt,
      mojo::PendingAssociatedReceiver<blink::mojom::IDBTransaction>
          transaction_receiver = mojo::NullAssociatedReceiver()) {
    auto factory_client_remote = factory_client.CreateInterfacePtrAndBind();
    factory_client_remote.EnableUnassociatedUsage();
    auto connection = std::make_unique<PendingConnection>(
        mojo::AssociatedRemote<blink::mojom::IDBFactoryClient>(
            std::move(factory_client_remote)),
        std::make_unique<DatabaseCallbacks>(mojo::NullAssociatedRemote()),
        transaction_id, version, std::move(transaction_receiver));
    connection->client_info = client_info;
    connection->shared_connection_id = shared_connection_id;
    db_->ScheduleOpenConnection(std::move(connection),
                                /*synchronous_duration=*/{});
  }

  mojo::AssociatedRemote<blink::mojom::IDBDatabase> OpenWithUpgrade(
      const storage::BucketClientInfo& client_info,
      int64_t transaction_id,
      int64_t version,
      bool commit = true) {
    MockMojoFactoryClient factory_client;
    mojo::AssociatedRemote<blink::mojom::IDBTransaction> transaction;
    mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database;
    base::RunLoop upgrade_loop;
    EXPECT_CALL(factory_client, MockedUpgradeNeeded)
        .WillOnce(
            testing::DoAll(MoveArgPointee<0>(&pending_database),
                           base::test::RunClosure(upgrade_loop.QuitClosure())));
    ScheduleOpen(factory_client, client_info, transaction_id, version,
                 /*shared_connection_id=*/std::nullopt,
                 transaction.BindNewEndpointAndPassDedicatedReceiver());
    upgrade_loop.Run();
    mojo::AssociatedRemote<blink::mojom::IDBDatabase> database(
        std::move(pending_database));

    base::RunLoop finish_loop;
    if (commit) {
      EXPECT_CALL(factory_client, MockedOpenSuccess)
          .WillOnce(base::test::RunClosure(finish_loop.QuitClosure()));
      transaction->Commit(0);
    } else {
      EXPECT_CALL(factory_client,
                  Error(blink::mojom::IDBException::kAbortError, testing::_))
          .WillOnce(base::test::RunClosure(finish_loop.QuitClosure()));
      database->Abort(transaction_id);
    }
    finish_loop.Run();
    return database;
  }

  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> OpenShared(
      const storage::BucketClientInfo& client_info,
      int64_t transaction_id,
      int64_t shared_connection_id) {
    MockMojoFactoryClient factory_client;
    mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database;
    base::RunLoop run_loop;
    EXPECT_CALL(factory_client, MockedOpenSuccess)
        .WillOnce(
            testing::DoAll(MoveArgPointee<0>(&pending_database),
                           base::test::RunClosure(run_loop.QuitClosure())));
    ScheduleOpen(factory_client, client_info, transaction_id,
                 IndexedDBDatabaseMetadata::NO_VERSION, shared_connection_id);
    run_loop.Run();
    return pending_database;
  }

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
  bucket_context_->ForceClose(false);
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

  bucket_context_->ForceClose(false);
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

TEST_P(DatabaseTest, SharedConnectionRequiresMatchingClient) {
  const blink::DocumentToken document_token;
  const storage::BucketClientInfo client1 = {1, blink::LocalFrameToken(),
                                             document_token};
  const storage::BucketClientInfo client2 = {1, blink::LocalFrameToken(),
                                             blink::DocumentToken()};
  // A dedicated worker of client 1's document shares the document token, but
  // has its own `IDBFactory` and cannot share client 1's connections.
  const storage::BucketClientInfo worker_client = {
      1, blink::DedicatedWorkerToken(), document_token};

  auto database1 =
      OpenWithUpgrade(client1, /*transaction_id=*/1, /*version=*/1);
  auto database2 = OpenShared(client2, /*transaction_id=*/2,
                              /*shared_connection_id=*/1);
  EXPECT_TRUE(database2.is_valid());
  auto database3 = OpenShared(worker_client, /*transaction_id=*/3,
                              /*shared_connection_id=*/1);
  EXPECT_TRUE(database3.is_valid());
  EXPECT_FALSE(OpenShared(client1, /*transaction_id=*/4,
                          /*shared_connection_id=*/1)
                   .is_valid());
  EXPECT_EQ(db_->ConnectionCount(), 3UL);
}

// Regression test for crbug.com/576189987: the connection of an open request
// whose upgrade was aborted must not be shared, even if it is still open.
TEST_P(DatabaseTest, SharedConnectionNotReturnedForAbortedUpgrade) {
  const storage::BucketClientInfo client = {1, blink::DedicatedWorkerToken()};

  // 1. Create the database at version 1, then close the connection (processed
  // once the next request is scheduled so `db_` is not released).
  OpenWithUpgrade(client, /*transaction_id=*/1, /*version=*/1).reset();

  // 2. Start an upgrade to version 2 and abort it while keeping its connection
  // open.
  auto database2 = OpenWithUpgrade(client, /*transaction_id=*/2, /*version=*/2,
                                   /*commit=*/false);
  EXPECT_EQ(db_->version(), 1);
  EXPECT_EQ(db_->ConnectionCount(), 1UL);

  // 3. A request that asks to share the aborted upgrade's connection is given a
  // new connection.
  auto database3 =
      OpenShared(client, /*transaction_id=*/3, /*shared_connection_id=*/2);
  EXPECT_TRUE(database3.is_valid());
  EXPECT_EQ(db_->ConnectionCount(), 2UL);
}

// Regression test for crbug.com/576189987: a request may only share the
// connection created by the request it names, and only if that request
// succeeded, regardless of other connections that the client has.
TEST_P(DatabaseTest, SharedConnectionRequiresSuccessfulOpenRequest) {
  const storage::BucketClientInfo client = {1, blink::LocalFrameToken(),
                                            blink::DocumentToken()};

  // 1. Create the database at version 2 and keep the connection.
  auto database1 = OpenWithUpgrade(client, /*transaction_id=*/1, /*version=*/2);

  // 2. A request for version 1 fails with a VersionError.
  MockMojoFactoryClient factory_client2;
  base::RunLoop error_loop;
  EXPECT_CALL(factory_client2,
              Error(blink::mojom::IDBException::kVersionError, testing::_))
      .WillOnce(base::test::RunClosure(error_loop.QuitClosure()));
  ScheduleOpen(factory_client2, client, /*transaction_id=*/2, /*version=*/1);
  error_loop.Run();

  // 3. A request that asks to share the failed request's connection is given a
  // new connection, even though the client holds `database1`.
  auto database3 =
      OpenShared(client, /*transaction_id=*/3, /*shared_connection_id=*/2);
  EXPECT_TRUE(database3.is_valid());
  EXPECT_EQ(db_->ConnectionCount(), 2UL);

  // 4. Requests that ask to share the connections of the successful third and
  // first requests share them.
  EXPECT_FALSE(OpenShared(client, /*transaction_id=*/4,
                          /*shared_connection_id=*/3)
                   .is_valid());
  EXPECT_FALSE(OpenShared(client, /*transaction_id=*/5,
                          /*shared_connection_id=*/1)
                   .is_valid());
  EXPECT_EQ(db_->ConnectionCount(), 2UL);
}

}  // namespace content::indexed_db
