/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/indexeddb/idb_transaction.h"

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event_queue.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database_callbacks.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_path.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_metadata.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_object_store.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_test_helper.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value_wrapping.h"
#include "third_party/blink/renderer/modules/indexeddb/mock_web_idb_database.h"
#include "third_party/blink/renderer/modules/indexeddb/mock_web_idb_transaction.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "v8/include/v8.h"

namespace blink {
namespace {

void DeactivateNewTransactions(v8::Isolate* isolate) {
  V8PerIsolateData::From(isolate)->RunEndOfScopeTasks();
}

class FakeIDBDatabaseCallbacks final : public IDBDatabaseCallbacks {
 public:
  FakeIDBDatabaseCallbacks() = default;
  void OnVersionChange(int64_t old_version, int64_t new_version) override {}
  void OnForcedClose() override {}
  void OnAbort(int64_t transaction_id, DOMException* error) override {}
  void OnComplete(int64_t transaction_id) override {}
};

class IDBTransactionTest : public testing::Test {
 protected:
  void SetUp() override {
    url_loader_mock_factory_ = platform_->GetURLLoaderMockFactory();
    WebURLResponse response;
    response.SetCurrentRequestUrl(KURL("blob:"));
    url_loader_mock_factory_->RegisterURLProtocol(WebString("blob"), response,
                                                  "");
  }

  void TearDown() override {
    url_loader_mock_factory_->UnregisterAllURLsAndClearMemoryCache();
  }

  void BuildTransaction(
      V8TestingScope& scope,
      std::unique_ptr<MockWebIDBDatabase> database_backend,
      std::unique_ptr<MockWebIDBTransaction> transaction_backend) {
    db_ = MakeGarbageCollected<IDBDatabase>(
        scope.GetExecutionContext(), std::move(database_backend),
        MakeGarbageCollected<FakeIDBDatabaseCallbacks>(), scope.GetIsolate());

    HashSet<String> transaction_scope = {"store"};
    transaction_ = IDBTransaction::CreateNonVersionChange(
        scope.GetScriptState(), std::move(transaction_backend), kTransactionId,
        transaction_scope, mojom::IDBTransactionMode::ReadOnly,
        mojom::IDBTransactionDurability::Relaxed, db_.Get());

    IDBKeyPath store_key_path("primaryKey");
    scoped_refptr<IDBObjectStoreMetadata> store_metadata = base::AdoptRef(
        new IDBObjectStoreMetadata("store", kStoreId, store_key_path, true, 1));
    store_ = MakeGarbageCollected<IDBObjectStore>(store_metadata, transaction_);
  }

  WebURLLoaderMockFactory* url_loader_mock_factory_;
  Persistent<IDBDatabase> db_;
  Persistent<IDBTransaction> transaction_;
  Persistent<IDBObjectStore> store_;
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;

  static constexpr int64_t kTransactionId = 1234;
  static constexpr int64_t kStoreId = 5678;
};

TEST_F(IDBTransactionTest, ContextDestroyedEarlyDeath) {
  V8TestingScope scope;
  const int64_t kTransactionId = 1234;
  auto database_backend = std::make_unique<MockWebIDBDatabase>();
  auto transaction_backend = std::make_unique<MockWebIDBTransaction>(
      scope.GetExecutionContext()->GetTaskRunner(TaskType::kDatabaseAccess),
      kTransactionId);
  EXPECT_CALL(*database_backend, Close()).Times(1);
  BuildTransaction(scope, std::move(database_backend),
                   std::move(transaction_backend));

  Persistent<HeapHashSet<WeakMember<IDBTransaction>>> live_transactions =
      MakeGarbageCollected<HeapHashSet<WeakMember<IDBTransaction>>>();
  ;
  live_transactions->insert(transaction_);

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(1u, live_transactions->size());

  Persistent<IDBRequest> request =
      IDBRequest::Create(scope.GetScriptState(), store_.Get(),
                         transaction_.Get(), IDBRequest::AsyncTraceState());

  DeactivateNewTransactions(scope.GetIsolate());

  request.Clear();  // The transaction is holding onto the request.
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(1u, live_transactions->size());

  // This will generate an Abort() call to the back end which is dropped by the
  // fake proxy, so an explicit OnAbort call is made.
  scope.GetExecutionContext()->NotifyContextDestroyed();
  transaction_->OnAbort(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kAbortError, "Aborted"));
  transaction_.Clear();
  store_.Clear();

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(0U, live_transactions->size());
}

TEST_F(IDBTransactionTest, ContextDestroyedAfterDone) {
  V8TestingScope scope;
  const int64_t kTransactionId = 1234;
  auto database_backend = std::make_unique<MockWebIDBDatabase>();
  auto transaction_backend = std::make_unique<MockWebIDBTransaction>(
      scope.GetExecutionContext()->GetTaskRunner(TaskType::kDatabaseAccess),
      kTransactionId);
  EXPECT_CALL(*database_backend, Close()).Times(1);
  BuildTransaction(scope, std::move(database_backend),
                   std::move(transaction_backend));

  Persistent<HeapHashSet<WeakMember<IDBTransaction>>> live_transactions =
      MakeGarbageCollected<HeapHashSet<WeakMember<IDBTransaction>>>();
  ;
  live_transactions->insert(transaction_);

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(1U, live_transactions->size());

  Persistent<IDBRequest> request =
      IDBRequest::Create(scope.GetScriptState(), store_.Get(),
                         transaction_.Get(), IDBRequest::AsyncTraceState());
  DeactivateNewTransactions(scope.GetIsolate());

  // This response should result in an event being enqueued immediately.
  request->HandleResponse(CreateIDBValueForTesting(scope.GetIsolate(), false));

  request.Clear();  // The transaction is holding onto the request.
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(1U, live_transactions->size());

  // This will generate an Abort() call to the back end which is dropped by the
  // fake proxy, so an explicit OnAbort call is made.
  scope.GetExecutionContext()->NotifyContextDestroyed();
  transaction_->OnAbort(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kAbortError, "Aborted"));
  transaction_.Clear();
  store_.Clear();

  // The request completed, so it has enqueued a success event. Discard the
  // event, so that the transaction can go away.
  EXPECT_EQ(1U, live_transactions->size());

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(0U, live_transactions->size());
}

TEST_F(IDBTransactionTest, ContextDestroyedWithQueuedResult) {
  V8TestingScope scope;
  const int64_t kTransactionId = 1234;
  auto database_backend = std::make_unique<MockWebIDBDatabase>();
  auto transaction_backend = std::make_unique<MockWebIDBTransaction>(
      scope.GetExecutionContext()->GetTaskRunner(TaskType::kDatabaseAccess),
      kTransactionId);
  EXPECT_CALL(*database_backend, Close()).Times(1);
  BuildTransaction(scope, std::move(database_backend),
                   std::move(transaction_backend));

  Persistent<HeapHashSet<WeakMember<IDBTransaction>>> live_transactions =
      MakeGarbageCollected<HeapHashSet<WeakMember<IDBTransaction>>>();
  ;
  live_transactions->insert(transaction_);

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(1U, live_transactions->size());

  Persistent<IDBRequest> request =
      IDBRequest::Create(scope.GetScriptState(), store_.Get(),
                         transaction_.Get(), IDBRequest::AsyncTraceState());
  DeactivateNewTransactions(scope.GetIsolate());

  request->HandleResponse(CreateIDBValueForTesting(scope.GetIsolate(), true));

  request.Clear();  // The transaction is holding onto the request.
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(1U, live_transactions->size());

  // This will generate an Abort() call to the back end which is dropped by the
  // fake proxy, so an explicit OnAbort call is made.
  scope.GetExecutionContext()->NotifyContextDestroyed();
  transaction_->OnAbort(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kAbortError, "Aborted"));
  transaction_.Clear();
  store_.Clear();

  url_loader_mock_factory_->ServeAsynchronousRequests();

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(0U, live_transactions->size());
}

TEST_F(IDBTransactionTest, ContextDestroyedWithTwoQueuedResults) {
  V8TestingScope scope;
  const int64_t kTransactionId = 1234;
  auto database_backend = std::make_unique<MockWebIDBDatabase>();
  auto transaction_backend = std::make_unique<MockWebIDBTransaction>(
      scope.GetExecutionContext()->GetTaskRunner(TaskType::kDatabaseAccess),
      kTransactionId);
  EXPECT_CALL(*database_backend, Close()).Times(1);
  BuildTransaction(scope, std::move(database_backend),
                   std::move(transaction_backend));

  Persistent<HeapHashSet<WeakMember<IDBTransaction>>> live_transactions =
      MakeGarbageCollected<HeapHashSet<WeakMember<IDBTransaction>>>();
  ;
  live_transactions->insert(transaction_);

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(1U, live_transactions->size());

  Persistent<IDBRequest> request1 =
      IDBRequest::Create(scope.GetScriptState(), store_.Get(),
                         transaction_.Get(), IDBRequest::AsyncTraceState());
  Persistent<IDBRequest> request2 =
      IDBRequest::Create(scope.GetScriptState(), store_.Get(),
                         transaction_.Get(), IDBRequest::AsyncTraceState());
  DeactivateNewTransactions(scope.GetIsolate());

  request1->HandleResponse(CreateIDBValueForTesting(scope.GetIsolate(), true));
  request2->HandleResponse(CreateIDBValueForTesting(scope.GetIsolate(), true));

  request1.Clear();  // The transaction is holding onto the requests.
  request2.Clear();
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(1U, live_transactions->size());

  // This will generate an Abort() call to the back end which is dropped by the
  // fake proxy, so an explicit OnAbort call is made.
  scope.GetExecutionContext()->NotifyContextDestroyed();
  transaction_->OnAbort(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kAbortError, "Aborted"));
  transaction_.Clear();
  store_.Clear();

  url_loader_mock_factory_->ServeAsynchronousRequests();

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(0U, live_transactions->size());
}

TEST_F(IDBTransactionTest, DocumentShutdownWithQueuedAndBlockedResults) {
  // This test covers the conditions of https://crbug.com/733642

  V8TestingScope scope;
  const int64_t kTransactionId = 1234;
  auto database_backend = std::make_unique<MockWebIDBDatabase>();
  auto transaction_backend = std::make_unique<MockWebIDBTransaction>(
      scope.GetExecutionContext()->GetTaskRunner(TaskType::kDatabaseAccess),
      kTransactionId);
  EXPECT_CALL(*database_backend, Close()).Times(1);
  BuildTransaction(scope, std::move(database_backend),
                   std::move(transaction_backend));

  Persistent<HeapHashSet<WeakMember<IDBTransaction>>> live_transactions =
      MakeGarbageCollected<HeapHashSet<WeakMember<IDBTransaction>>>();
  ;
  live_transactions->insert(transaction_);

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(1U, live_transactions->size());

  Persistent<IDBRequest> request1 =
      IDBRequest::Create(scope.GetScriptState(), store_.Get(),
                         transaction_.Get(), IDBRequest::AsyncTraceState());
  Persistent<IDBRequest> request2 =
      IDBRequest::Create(scope.GetScriptState(), store_.Get(),
                         transaction_.Get(), IDBRequest::AsyncTraceState());
  DeactivateNewTransactions(scope.GetIsolate());

  request1->HandleResponse(CreateIDBValueForTesting(scope.GetIsolate(), true));
  request2->HandleResponse(CreateIDBValueForTesting(scope.GetIsolate(), false));

  request1.Clear();  // The transaction is holding onto the requests.
  request2.Clear();
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(1U, live_transactions->size());

  // This will generate an Abort() call to the back end which is dropped by the
  // fake proxy, so an explicit OnAbort call is made.
  scope.GetDocument().Shutdown();
  transaction_->OnAbort(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kAbortError, "Aborted"));
  transaction_.Clear();
  store_.Clear();

  url_loader_mock_factory_->ServeAsynchronousRequests();

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(0U, live_transactions->size());
}

TEST_F(IDBTransactionTest, TransactionFinish) {
  V8TestingScope scope;
  const int64_t kTransactionId = 1234;
  auto database_backend = std::make_unique<MockWebIDBDatabase>();
  auto transaction_backend = std::make_unique<MockWebIDBTransaction>(
      scope.GetExecutionContext()->GetTaskRunner(TaskType::kDatabaseAccess),
      kTransactionId);
  EXPECT_CALL(*transaction_backend, Commit(0)).Times(1);
  EXPECT_CALL(*database_backend, Close()).Times(1);
  BuildTransaction(scope, std::move(database_backend),
                   std::move(transaction_backend));

  Persistent<HeapHashSet<WeakMember<IDBTransaction>>> live_transactions =
      MakeGarbageCollected<HeapHashSet<WeakMember<IDBTransaction>>>();
  ;
  live_transactions->insert(transaction_);

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(1U, live_transactions->size());

  DeactivateNewTransactions(scope.GetIsolate());

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(1U, live_transactions->size());

  transaction_.Clear();
  store_.Clear();

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(1U, live_transactions->size());

  // Stop the context, so events don't get queued (which would keep the
  // transaction alive).
  scope.GetExecutionContext()->NotifyContextDestroyed();

  // Fire an abort to make sure this doesn't free the transaction during use.
  // The test will not fail if it is, but ASAN would notice the error.
  db_->OnAbort(kTransactionId, MakeGarbageCollected<DOMException>(
                                   DOMExceptionCode::kAbortError, "Aborted"));

  // OnAbort() should have cleared the transaction's reference to the database.
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(0U, live_transactions->size());
}

}  // namespace
}  // namespace blink
