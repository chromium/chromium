/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/indexeddb/idb_request.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_factory_client.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_path.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_metadata.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_object_store.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_open_db_request.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_request_queue_item.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_test_helper.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_transaction.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value_wrapping.h"
#include "third_party/blink/renderer/modules/indexeddb/mock_idb_database.h"
#include "third_party/blink/renderer/modules/indexeddb/mock_idb_transaction.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

namespace blink {

class BackendDatabaseWithMockedClose
    : public testing::StrictMock<mojom::blink::IDBDatabase> {
 public:
  explicit BackendDatabaseWithMockedClose(
      mojo::PendingAssociatedReceiver<mojom::blink::IDBDatabase>
          pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {
    receiver_.set_disconnect_handler(
        WTF::BindOnce(&BackendDatabaseWithMockedClose::DatabaseDestroyed,
                      base::Unretained(this)));
  }

  void DatabaseDestroyed() { destroyed_ = true; }

  MOCK_METHOD0(Close, void());

  bool destroyed() { return destroyed_; }

 private:
  bool destroyed_ = false;
  mojo::AssociatedReceiver<mojom::blink::IDBDatabase> receiver_;
};

// TODO(crbug.com/1510052): Many of these tests depend on a microtask running
// to satisfy the expected mock behavior, which we wait for with
// RunPendingTasks. We should make this less hacky.
class IDBRequestTest : public testing::Test {
 protected:
  void SetUp() override {
    url_loader_mock_factory_ = URLLoaderMockFactory::GetSingletonInstance();
    WebURLResponse response;
    response.SetCurrentRequestUrl(KURL("blob:"));
    url_loader_mock_factory_->RegisterURLProtocol(WebString("blob"), response,
                                                  "");
  }

  void TearDown() override {
    url_loader_mock_factory_->UnregisterAllURLsAndClearMemoryCache();
  }

  void BuildTransaction(V8TestingScope& scope,
                        MockIDBDatabase& mock_database,
                        MockIDBTransaction& mock_transaction_remote) {
    auto* execution_context = scope.GetExecutionContext();

    db_ = MakeGarbageCollected<IDBDatabase>(
        execution_context, mojo::NullAssociatedReceiver(), mojo::NullRemote(),
        mock_database.BindNewEndpointAndPassDedicatedRemote(), /*priority=*/0);

    IDBTransaction::TransactionMojoRemote transaction_remote(execution_context);
    mojo::PendingAssociatedReceiver<mojom::blink::IDBTransaction> receiver =
        transaction_remote.BindNewEndpointAndPassReceiver(
            blink::scheduler::GetSingleThreadTaskRunnerForTesting());
    receiver.EnableUnassociatedUsage();
    mock_transaction_remote.Bind(std::move(receiver));

    HashSet<String> transaction_scope = {"store"};
    transaction_ = IDBTransaction::CreateNonVersionChange(
        scope.GetScriptState(), std::move(transaction_remote), kTransactionId,
        transaction_scope, mojom::IDBTransactionMode::ReadOnly,
        mojom::IDBTransactionDurability::Relaxed, db_.Get());

    IDBKeyPath store_key_path("primaryKey");
    scoped_refptr<IDBObjectStoreMetadata> store_metadata = base::AdoptRef(
        new IDBObjectStoreMetadata("store", kStoreId, store_key_path, true, 1));
    store_ = MakeGarbageCollected<IDBObjectStore>(store_metadata, transaction_);
  }

  void EnsureRequestResponsesDontThrow(IDBRequest* request,
                                       ExceptionState& exception_state) {
    ASSERT_TRUE(request->transaction());
    V8TestingScope scope;

    request->HandleResponse(IDBKey::CreateInvalid());
    request->HandleResponse(CreateNullIDBValueForTesting(scope.GetIsolate()));
    request->HandleResponse(static_cast<int64_t>(0));
    request->HandleResponse();
    request->HandleResponseAdvanceCursor(
        IDBKey::CreateInvalid(), IDBKey::CreateInvalid(),
        CreateNullIDBValueForTesting(scope.GetIsolate()));

    EXPECT_TRUE(!exception_state.HadException());
  }

  void SimulateErrorResult(IDBRequest* request) {
    request->HandleError(nullptr);
  }

  void FinishLoadingResult(IDBRequest* request) {
    V8TestingScope scope;
    Vector<std::unique_ptr<IDBValue>> values;
    values.push_back(CreateIDBValueForTesting(scope.GetIsolate(),
                                              /*create_wrapped_value=*/false));
    request->queue_item_->OnLoadComplete(std::move(values),
                                         /*error=*/nullptr);
  }

  test::TaskEnvironment task_environment_;
  raw_ptr<URLLoaderMockFactory> url_loader_mock_factory_;
  Persistent<IDBDatabase> db_;
  Persistent<IDBTransaction> transaction_;
  Persistent<IDBObjectStore> store_;
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;

  static constexpr int64_t kTransactionId = 1234;
  static constexpr int64_t kStoreId = 5678;
};

const int64_t IDBRequestTest::kTransactionId;

TEST_F(IDBRequestTest, EventsAfterEarlyDeathStop) {
  V8TestingScope scope;
  MockIDBDatabase database_backend;
  MockIDBTransaction transaction_backend;
  EXPECT_CALL(database_backend, OnDisconnect()).Times(1);
  EXPECT_CALL(transaction_backend, Commit(0)).Times(1);
  BuildTransaction(scope, database_backend, transaction_backend);

  ASSERT_TRUE(!scope.GetExceptionState().HadException());
  ASSERT_TRUE(transaction_);

  IDBRequest* request =
      IDBRequest::Create(scope.GetScriptState(), store_.Get(),
                         transaction_.Get(), IDBRequest::AsyncTraceState());

  EXPECT_EQ(request->readyState(), "pending");
  ASSERT_TRUE(!scope.GetExceptionState().HadException());
  ASSERT_TRUE(request->transaction());
  scope.GetExecutionContext()->NotifyContextDestroyed();

  EnsureRequestResponsesDontThrow(request, scope.GetExceptionState());
  transaction_->FlushForTesting();
  database_backend.Flush();

  scope.PerformMicrotaskCheckpoint();
  test::RunPendingTasks();
}

TEST_F(IDBRequestTest, EventsAfterDoneStop) {
  V8TestingScope scope;
  MockIDBDatabase database_backend;
  MockIDBTransaction transaction_backend;
  EXPECT_CALL(database_backend, OnDisconnect()).Times(1);
  EXPECT_CALL(transaction_backend, Commit(0)).Times(1);
  BuildTransaction(scope, database_backend, transaction_backend);

  ASSERT_TRUE(!scope.GetExceptionState().HadException());
  ASSERT_TRUE(transaction_);

  IDBRequest* request =
      IDBRequest::Create(scope.GetScriptState(), store_.Get(),
                         transaction_.Get(), IDBRequest::AsyncTraceState());
  // The transaction won't be ready until it sets itself to inactive.
  scope.PerformMicrotaskCheckpoint();

  ASSERT_TRUE(!scope.GetExceptionState().HadException());
  ASSERT_TRUE(request->transaction());
  request->HandleResponse(CreateIDBValueForTesting(scope.GetIsolate(), false));
  scope.GetExecutionContext()->NotifyContextDestroyed();

  EnsureRequestResponsesDontThrow(request, scope.GetExceptionState());
  transaction_->FlushForTesting();
  database_backend.Flush();
}

TEST_F(IDBRequestTest, EventsAfterEarlyDeathStopWithQueuedResult) {
  V8TestingScope scope;
  MockIDBDatabase database_backend;
  MockIDBTransaction transaction_backend;
  EXPECT_CALL(database_backend, OnDisconnect()).Times(1);
  EXPECT_CALL(transaction_backend, Commit(0)).Times(1);
  BuildTransaction(scope, database_backend, transaction_backend);

  ASSERT_TRUE(!scope.GetExceptionState().HadException());
  ASSERT_TRUE(transaction_);

  IDBRequest* request =
      IDBRequest::Create(scope.GetScriptState(), store_.Get(),
                         transaction_.Get(), IDBRequest::AsyncTraceState());
  EXPECT_EQ(request->readyState(), "pending");
  ASSERT_TRUE(!scope.GetExceptionState().HadException());
  ASSERT_TRUE(request->transaction());
  request->HandleResponse(CreateIDBValueForTesting(scope.GetIsolate(), true));
  scope.GetExecutionContext()->NotifyContextDestroyed();

  EnsureRequestResponsesDontThrow(request, scope.GetExceptionState());
  url_loader_mock_factory_->ServeAsynchronousRequests();
  EnsureRequestResponsesDontThrow(request, scope.GetExceptionState());
  transaction_->FlushForTesting();
  database_backend.Flush();

  scope.PerformMicrotaskCheckpoint();
  test::RunPendingTasks();
}

// This test is flaky on Marshmallow 64 bit Tester because the test is
// crashing. See <http://crbug.com/1068057>.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_EventsAfterEarlyDeathStopWithTwoQueuedResults \
  DISABLED_EventsAfterEarlyDeathStopWithTwoQueuedResults
#else
#define MAYBE_EventsAfterEarlyDeathStopWithTwoQueuedResults \
  EventsAfterEarlyDeathStopWithTwoQueuedResults
#endif

TEST_F(IDBRequestTest, MAYBE_EventsAfterEarlyDeathStopWithTwoQueuedResults) {
  V8TestingScope scope;
  MockIDBDatabase database_backend;
  MockIDBTransaction transaction_backend;
  EXPECT_CALL(database_backend, OnDisconnect()).Times(1);
  EXPECT_CALL(transaction_backend, Commit(0)).Times(1);
  BuildTransaction(scope, database_backend, transaction_backend);

  ASSERT_TRUE(!scope.GetExceptionState().HadException());
  ASSERT_TRUE(transaction_);

  IDBRequest* request1 =
      IDBRequest::Create(scope.GetScriptState(), store_.Get(),
                         transaction_.Get(), IDBRequest::AsyncTraceState());
  IDBRequest* request2 =
      IDBRequest::Create(scope.GetScriptState(), store_.Get(),
                         transaction_.Get(), IDBRequest::AsyncTraceState());
  EXPECT_EQ(request1->readyState(), "pending");
  EXPECT_EQ(request2->readyState(), "pending");
  ASSERT_TRUE(!scope.GetExceptionState().HadException());
  ASSERT_TRUE(request1->transaction());
  ASSERT_TRUE(request2->transaction());
  request1->HandleResponse(CreateIDBValueForTesting(scope.GetIsolate(), true));
  request2->HandleResponse(CreateIDBValueForTesting(scope.GetIsolate(), true));
  scope.GetExecutionContext()->NotifyContextDestroyed();

  EnsureRequestResponsesDontThrow(request1, scope.GetExceptionState());
  EnsureRequestResponsesDontThrow(request2, scope.GetExceptionState());
  url_loader_mock_factory_->ServeAsynchronousRequests();
  EnsureRequestResponsesDontThrow(request1, scope.GetExceptionState());
  EnsureRequestResponsesDontThrow(request2, scope.GetExceptionState());
  transaction_->FlushForTesting();
  database_backend.Flush();

  scope.PerformMicrotaskCheckpoint();
  test::RunPendingTasks();
}

// Regression test for crbug.com/1470485
TEST_F(IDBRequestTest, ErrorWithQueuedLoadingResult) {
  V8TestingScope scope;
  MockIDBDatabase database_backend;
  MockIDBTransaction transaction_backend;
  BuildTransaction(scope, database_backend, transaction_backend);

  IDBRequest* request1 =
      IDBRequest::Create(scope.GetScriptState(), store_.Get(),
                         transaction_.Get(), IDBRequest::AsyncTraceState());
  IDBRequest* request2 =
      IDBRequest::Create(scope.GetScriptState(), store_.Get(),
                         transaction_.Get(), IDBRequest::AsyncTraceState());
  IDBRequest* request3 =
      IDBRequest::Create(scope.GetScriptState(), store_.Get(),
                         transaction_.Get(), IDBRequest::AsyncTraceState());
  // The transaction won't be ready until it sets itself to inactive.
  scope.PerformMicrotaskCheckpoint();

  // Three requests:
  // * the first is just there to block the queue so the error result doesn't
  // dispatch right away.
  // * the second is an error result which will cause the transaction to abort
  // since the result event isn't preventDefault()ed.
  // * the third is another result which is still in the loading state when the
  // transaction starts aborting its requests.
  request1->HandleResponse(CreateIDBValueForTesting(
      scope.GetIsolate(), /*create_wrapped_value=*/true));
  SimulateErrorResult(request2);
  request3->HandleResponse(CreateIDBValueForTesting(
      scope.GetIsolate(), /*create_wrapped_value=*/true));

  FinishLoadingResult(request1);
}

TEST_F(IDBRequestTest, ContextDestroyedWithQueuedErrorResult) {
  V8TestingScope scope;
  MockIDBDatabase database_backend;
  MockIDBTransaction transaction_backend;
  BuildTransaction(scope, database_backend, transaction_backend);

  IDBRequest* request1 =
      IDBRequest::Create(scope.GetScriptState(), store_.Get(),
                         transaction_.Get(), IDBRequest::AsyncTraceState());
  IDBRequest* request2 =
      IDBRequest::Create(scope.GetScriptState(), store_.Get(),
                         transaction_.Get(), IDBRequest::AsyncTraceState());
  // The transaction won't be ready until it sets itself to inactive.
  scope.PerformMicrotaskCheckpoint();

  request1->HandleResponse(CreateIDBValueForTesting(
      scope.GetIsolate(), /*create_wrapped_value=*/true));
  SimulateErrorResult(request2);

  // No crash at the end of this test as the context is destroyed indicates
  // success. This test is potentially flaky when failing because the order of
  // `ContextDestroyed` calls matters to the test, but is not predictable.
}

TEST_F(IDBRequestTest, ConnectionsAfterStopping) {
  V8TestingScope scope;
  const int64_t kVersion = 1;
  const int64_t kOldVersion = 0;
  const IDBDatabaseMetadata metadata;

  {
    MockIDBDatabase mock_database;
    mojo::AssociatedRemote<mojom::blink::IDBDatabase> remote;
    mock_database.Bind(remote.BindNewEndpointAndPassDedicatedReceiver());
    EXPECT_CALL(mock_database, OnDisconnect()).Times(1);

    auto* execution_context = scope.GetExecutionContext();
    IDBTransaction::TransactionMojoRemote transaction_remote(execution_context);
    mojo::PendingAssociatedReceiver<mojom::blink::IDBTransaction>
        transaction_receiver =
            transaction_remote.BindNewEndpointAndPassReceiver(
                blink::scheduler::GetSingleThreadTaskRunnerForTesting());
    transaction_receiver.EnableUnassociatedUsage();

    auto* request = MakeGarbageCollected<IDBOpenDBRequest>(
        scope.GetScriptState(), mojo::NullAssociatedReceiver(),
        std::move(transaction_remote), kTransactionId, kVersion,
        IDBRequest::AsyncTraceState(), mojo::NullRemote());
    EXPECT_EQ(request->readyState(), "pending");
    std::unique_ptr<IDBFactoryClient> factory_client =
        request->CreateFactoryClient();

    scope.GetExecutionContext()->NotifyContextDestroyed();
    factory_client->UpgradeNeeded(remote.Unbind(), kOldVersion,
                                  mojom::IDBDataLoss::None, String(), metadata);
    platform_->RunUntilIdle();
  }

  {
    MockIDBDatabase mock_database;
    mojo::AssociatedRemote<mojom::blink::IDBDatabase> remote;
    mock_database.Bind(remote.BindNewEndpointAndPassDedicatedReceiver());
    EXPECT_CALL(mock_database, OnDisconnect()).Times(1);

    auto* execution_context = scope.GetExecutionContext();
    IDBTransaction::TransactionMojoRemote transaction_remote(execution_context);
    mojo::PendingAssociatedReceiver<mojom::blink::IDBTransaction>
        transaction_receiver =
            transaction_remote.BindNewEndpointAndPassReceiver(
                blink::scheduler::GetSingleThreadTaskRunnerForTesting());
    transaction_receiver.EnableUnassociatedUsage();

    auto* request = MakeGarbageCollected<IDBOpenDBRequest>(
        scope.GetScriptState(), mojo::NullAssociatedReceiver(),
        std::move(transaction_remote), kTransactionId, kVersion,
        IDBRequest::AsyncTraceState(), mojo::NullRemote());
    EXPECT_EQ(request->readyState(), "pending");
    std::unique_ptr<IDBFactoryClient> factory_client =
        request->CreateFactoryClient();

    scope.GetExecutionContext()->NotifyContextDestroyed();
    factory_client->OpenSuccess(remote.Unbind(), metadata);
    platform_->RunUntilIdle();
  }
}

// Expose private state for testing.
class AsyncTraceStateForTesting : public IDBRequest::AsyncTraceState {
 public:
  explicit AsyncTraceStateForTesting(IDBRequest::TypeForMetrics type)
      : IDBRequest::AsyncTraceState(type) {}
  AsyncTraceStateForTesting() = default;
  AsyncTraceStateForTesting(AsyncTraceStateForTesting&& other)
      : IDBRequest::AsyncTraceState(std::move(other)) {}
  AsyncTraceStateForTesting& operator=(AsyncTraceStateForTesting&& rhs) {
    AsyncTraceState::operator=(std::move(rhs));
    return *this;
  }

  std::optional<IDBRequest::TypeForMetrics> type() const {
    return IDBRequest::AsyncTraceState::type();
  }
  const base::TimeTicks& start_time() const {
    return IDBRequest::AsyncTraceState::start_time();
  }
  size_t id() const { return IDBRequest::AsyncTraceState::id(); }
};

TEST(IDBRequestAsyncTraceStateTest, EmptyConstructor) {
  AsyncTraceStateForTesting state;

  EXPECT_FALSE(state.type());
  EXPECT_TRUE(state.IsEmpty());
}

TEST(IDBRequestAsyncTraceStateTest, MoveConstructor) {
  IDBRequest::TypeForMetrics type =
      IDBRequest::TypeForMetrics::kObjectStoreGetAllKeys;
  AsyncTraceStateForTesting source_state(type);
  size_t id = source_state.id();
  base::TimeTicks start_time = source_state.start_time();

  AsyncTraceStateForTesting state(std::move(source_state));
  EXPECT_EQ(type, *state.type());
  EXPECT_EQ(id, state.id());
  EXPECT_EQ(start_time, state.start_time());
  EXPECT_TRUE(source_state.IsEmpty());
}

TEST(IDBRequestAsyncTraceStateTest, MoveAssignment) {
  IDBRequest::TypeForMetrics type =
      IDBRequest::TypeForMetrics::kObjectStoreGetAllKeys;
  AsyncTraceStateForTesting source_state(type);
  size_t id = source_state.id();
  base::TimeTicks start_time = source_state.start_time();

  AsyncTraceStateForTesting state;
  EXPECT_TRUE(state.IsEmpty());

  state = std::move(source_state);
  EXPECT_EQ(type, *state.type());
  EXPECT_EQ(id, state.id());
  EXPECT_EQ(start_time, state.start_time());
  EXPECT_TRUE(source_state.IsEmpty());
}

}  // namespace blink
