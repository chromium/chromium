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

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/testing/scoped_mock_overlay_scrollbars.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_path.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_metadata.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_object_store.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_test_helper.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value_wrapping.h"
#include "third_party/blink/renderer/modules/indexeddb/mock_idb_database.h"
#include "third_party/blink/renderer/modules/indexeddb/mock_idb_transaction.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "v8/include/v8.h"

namespace blink {
namespace {

class IDBTransactionTest : public testing::Test,
                           public ScopedMockOverlayScrollbars {
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

  test::TaskEnvironment task_environment_;
  raw_ptr<URLLoaderMockFactory> url_loader_mock_factory_;
  Persistent<IDBDatabase> db_;
  Persistent<IDBTransaction> transaction_;
  Persistent<IDBObjectStore> store_;
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;

  static constexpr int64_t kTransactionId = 1234;
  static constexpr int64_t kStoreId = 5678;
};

const int64_t IDBTransactionTest::kTransactionId;

TEST_F(IDBTransactionTest, ContextDestroyedEarlyDeath) {
  V8TestingScope scope;
  MockIDBDatabase database_backend;
  MockIDBTransaction transaction_backend;
  EXPECT_CALL(database_backend, OnDisconnect()).Times(1);
  BuildTransaction(scope, database_backend, transaction_backend);

  Persistent<HeapHashSet<WeakMember<IDBTransaction>>> live_transactions =
      MakeGarbageCollected<HeapHashSet<WeakMember<IDBTransaction>>>();
  live_transactions->insert(transaction_);

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(1u, live_transactions->size());

  Persistent<IDBRequest> request =
      IDBRequest::Create(scope.GetScriptState(), store_.Get(),
                         transaction_.Get(), IDBRequest::AsyncTraceState());

  scope.PerformMicrotaskCheckpoint();

  request.Clear();  // The transaction is holding onto the request.
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(1u, live_transactions->size());

  // This will generate an Abort() call to the back end which is dropped by the
  // fake proxy, so an explicit OnAbort call is made.
  scope.GetExecutionContext()->NotifyContextDestroyed();
  transaction_->OnAbort(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kAbortError, "Aborted"));
  transaction_->FlushForTesting();
  transaction_.Clear();
  store_.Clear();
  database_backend.Flush();

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(0U, live_transactions->size());
}

TEST_F(IDBTransactionTest, ContextDestroyedAfterDone) {
  V8TestingScope scope;
  MockIDBDatabase database_backend;
  MockIDBTransaction transaction_backend;
  EXPECT_CALL(database_backend, OnDisconnect()).Times(1);
  BuildTransaction(scope, database_backend, transaction_backend);

  Persistent<HeapHashSet<WeakMember<IDBTransaction>>> live_transactions =
      MakeGarbageCollected<HeapHashSet<WeakMember<IDBTransaction>>>();
  live_transactions->insert(transaction_);

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(1U, live_transactions->size());

  Persistent<IDBRequest> request =
      IDBRequest::Create(scope.GetScriptState(), store_.Get(),
                         transaction_.Get(), IDBRequest::AsyncTraceState());
  scope.PerformMicrotaskCheckpoint();

  request.Clear();  // The transaction is holding onto the request.
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(1U, live_transactions->size());

  // This will generate an Abort() call to the back end which is dropped by the
  // fake proxy, so an explicit OnAbort call is made.
  scope.GetExecutionContext()->NotifyContextDestroyed();
  transaction_->OnAbort(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kAbortError, "Aborted"));
  transaction_->FlushForTesting();
  transaction_.Clear();
  store_.Clear();
  database_backend.Flush();

  EXPECT_EQ(1U, live_transactions->size());

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(0U, live_transactions->size());
}

TEST_F(IDBTransactionTest, ContextDestroyedWithQueuedResult) {
  V8TestingScope scope;
  MockIDBDatabase database_backend;
  MockIDBTransaction transaction_backend;
  EXPECT_CALL(database_backend, OnDisconnect()).Times(1);
  BuildTransaction(scope, database_backend, transaction_backend);

  Persistent<HeapHashSet<WeakMember<IDBTransaction>>> live_transactions =
      MakeGarbageCollected<HeapHashSet<WeakMember<IDBTransaction>>>();
  live_transactions->insert(transaction_);

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(1U, live_transactions->size());

  Persistent<IDBRequest> request =
      IDBRequest::Create(scope.GetScriptState(), store_.Get(),
                         transaction_.Get(), IDBRequest::AsyncTraceState());
  scope.PerformMicrotaskCheckpoint();

  request->HandleResponse(CreateIDBValueForTesting(scope.GetIsolate(), true));

  request.Clear();  // The transaction is holding onto the request.
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(1U, live_transactions->size());

  // This will generate an Abort() call to the back end which is dropped by the
  // fake proxy, so an explicit OnAbort call is made.
  scope.GetExecutionContext()->NotifyContextDestroyed();
  transaction_->OnAbort(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kAbortError, "Aborted"));
  transaction_->FlushForTesting();
  transaction_.Clear();
  store_.Clear();
  database_backend.Flush();

  url_loader_mock_factory_->ServeAsynchronousRequests();

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(0U, live_transactions->size());
}

TEST_F(IDBTransactionTest, ContextDestroyedWithTwoQueuedResults) {
  V8TestingScope scope;
  MockIDBDatabase database_backend;
  MockIDBTransaction transaction_backend;
  EXPECT_CALL(database_backend, OnDisconnect()).Times(1);
  BuildTransaction(scope, database_backend, transaction_backend);

  Persistent<HeapHashSet<WeakMember<IDBTransaction>>> live_transactions =
      MakeGarbageCollected<HeapHashSet<WeakMember<IDBTransaction>>>();
  live_transactions->insert(transaction_);

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(1U, live_transactions->size());

  Persistent<IDBRequest> request1 =
      IDBRequest::Create(scope.GetScriptState(), store_.Get(),
                         transaction_.Get(), IDBRequest::AsyncTraceState());
  Persistent<IDBRequest> request2 =
      IDBRequest::Create(scope.GetScriptState(), store_.Get(),
                         transaction_.Get(), IDBRequest::AsyncTraceState());
  scope.PerformMicrotaskCheckpoint();

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
  transaction_->FlushForTesting();
  transaction_.Clear();
  store_.Clear();
  database_backend.Flush();

  url_loader_mock_factory_->ServeAsynchronousRequests();

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(0U, live_transactions->size());
}

TEST_F(IDBTransactionTest, DocumentShutdownWithQueuedAndBlockedResults) {
  // This test covers the conditions of https://crbug.com/733642

  MockIDBDatabase database_backend;
  MockIDBTransaction transaction_backend;
  EXPECT_CALL(database_backend, OnDisconnect()).Times(1);
  {
    // The database isn't actually closed until `scope` is destroyed, so create
    // this object in a nested scope to allow mock expectations to be verified.
    V8TestingScope scope;

    BuildTransaction(scope, database_backend, transaction_backend);

    Persistent<HeapHashSet<WeakMember<IDBTransaction>>> live_transactions =
        MakeGarbageCollected<HeapHashSet<WeakMember<IDBTransaction>>>();
    live_transactions->insert(transaction_);

    ThreadState::Current()->CollectAllGarbageForTesting();
    EXPECT_EQ(1U, live_transactions->size());

    Persistent<IDBRequest> request1 =
        IDBRequest::Create(scope.GetScriptState(), store_.Get(),
                           transaction_.Get(), IDBRequest::AsyncTraceState());
    Persistent<IDBRequest> request2 =
        IDBRequest::Create(scope.GetScriptState(), store_.Get(),
                           transaction_.Get(), IDBRequest::AsyncTraceState());
    scope.PerformMicrotaskCheckpoint();

    request1->HandleResponse(
        CreateIDBValueForTesting(scope.GetIsolate(), true));
    request2->HandleResponse(
        CreateIDBValueForTesting(scope.GetIsolate(), false));

    request1.Clear();  // The transaction is holding onto the requests.
    request2.Clear();
    ThreadState::Current()->CollectAllGarbageForTesting();
    EXPECT_EQ(1U, live_transactions->size());

    // This will generate an Abort() call to the back end which is dropped by
    // the fake proxy, so an explicit OnAbort call is made.
    scope.GetDocument().Shutdown();
    transaction_->OnAbort(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kAbortError, "Aborted"));
    transaction_->FlushForTesting();
    transaction_.Clear();
    store_.Clear();

    url_loader_mock_factory_->ServeAsynchronousRequests();

    ThreadState::Current()->CollectAllGarbageForTesting();
    EXPECT_EQ(0U, live_transactions->size());
  }
  database_backend.Flush();
}

TEST_F(IDBTransactionTest, TransactionFinish) {
  V8TestingScope scope;
  MockIDBDatabase database_backend;
  MockIDBTransaction transaction_backend;
  EXPECT_CALL(transaction_backend, Commit(0)).Times(1);
  EXPECT_CALL(database_backend, OnDisconnect()).Times(1);
  BuildTransaction(scope, database_backend, transaction_backend);

  Persistent<HeapHashSet<WeakMember<IDBTransaction>>> live_transactions =
      MakeGarbageCollected<HeapHashSet<WeakMember<IDBTransaction>>>();
  live_transactions->insert(transaction_);

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(1U, live_transactions->size());

  scope.PerformMicrotaskCheckpoint();

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(1U, live_transactions->size());

  transaction_->FlushForTesting();
  transaction_.Clear();
  store_.Clear();

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(1U, live_transactions->size());

  // Stop the context, so events don't get queued (which would keep the
  // transaction alive).
  scope.GetExecutionContext()->NotifyContextDestroyed();

  // Fire an abort to make sure this doesn't free the transaction during use.
  // The test will not fail if it is, but ASAN would notice the error.
  db_->Abort(kTransactionId, mojom::blink::IDBException::kAbortError,
             "Aborted");

  database_backend.Flush();

  // OnAbort() should have cleared the transaction's reference to the database.
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(0U, live_transactions->size());
}

TEST_F(IDBTransactionTest, ValueSizeTest) {
  // For testing use a much smaller maximum size to prevent allocating >100 MB
  // of memory, which crashes on memory-constrained systems.
  const size_t kMaxValueSizeForTesting = 10 * 1024 * 1024;  // 10 MB

  const Vector<char> value_data(kMaxValueSizeForTesting + 1);
  const Vector<WebBlobInfo> blob_info;
  auto value = std::make_unique<IDBValue>(Vector<char>(value_data), blob_info);
  std::unique_ptr<IDBKey> key = IDBKey::CreateNumber(0);
  const int64_t object_store_id = 2;

  ASSERT_GT(value_data.size() + key->SizeEstimate(), kMaxValueSizeForTesting);
  ThreadState::Current()->CollectAllGarbageForTesting();

  bool got_error = false;
  auto callback = WTF::BindOnce(
      [](bool* got_error, mojom::blink::IDBTransactionPutResultPtr result) {
        *got_error = result->is_error_result();
      },
      WTF::Unretained(&got_error));

  V8TestingScope scope;
  MockIDBDatabase database_backend;
  MockIDBTransaction transaction_backend;
  BuildTransaction(scope, database_backend, transaction_backend);

  transaction_->set_max_put_value_size_for_testing(kMaxValueSizeForTesting);
  transaction_->Put(object_store_id, std::move(value), std::move(key),
                    mojom::IDBPutMode::AddOrUpdate, Vector<IDBIndexKeys>(),
                    std::move(callback));
  EXPECT_TRUE(got_error);
}

TEST_F(IDBTransactionTest, KeyAndValueSizeTest) {
  // For testing use a much smaller maximum size to prevent allocating >100 MB
  // of memory, which crashes on memory-constrained systems.
  const size_t kMaxValueSizeForTesting = 10 * 1024 * 1024;  // 10 MB
  const size_t kKeySize = 1024 * 1024;

  const Vector<char> value_data(kMaxValueSizeForTesting - kKeySize);
  const Vector<WebBlobInfo> blob_info;
  auto value = std::make_unique<IDBValue>(Vector<char>(value_data), blob_info);
  const int64_t object_store_id = 2;

  // For this test, we want IDBKey::SizeEstimate() minus kKeySize to be the
  // smallest value > 0.  An IDBKey with a string has a size_estimate_ equal to
  // kOverheadSize (~16) + (string.length * sizeof(UChar)).  Create
  // |kKeySize / sizeof(UChar)| characters in String.
  const unsigned int number_of_chars = kKeySize / sizeof(UChar);
  Vector<UChar> key_string_vector;
  key_string_vector.ReserveInitialCapacity(number_of_chars);
  key_string_vector.Fill(u'0', number_of_chars);
  String key_string(key_string_vector);
  DCHECK_EQ(key_string.length(), number_of_chars);

  std::unique_ptr<IDBKey> key = IDBKey::CreateString(key_string);
  DCHECK_EQ(value_data.size(), kMaxValueSizeForTesting - kKeySize);
  DCHECK_GT(key->SizeEstimate() - kKeySize, static_cast<size_t>(0));
  DCHECK_GT(value_data.size() + key->SizeEstimate(), kMaxValueSizeForTesting);

  ThreadState::Current()->CollectAllGarbageForTesting();

  bool got_error = false;
  auto callback = WTF::BindOnce(
      [](bool* got_error, mojom::blink::IDBTransactionPutResultPtr result) {
        *got_error = result->is_error_result();
      },
      WTF::Unretained(&got_error));

  V8TestingScope scope;
  MockIDBDatabase database_backend;
  MockIDBTransaction transaction_backend;
  BuildTransaction(scope, database_backend, transaction_backend);

  transaction_->set_max_put_value_size_for_testing(kMaxValueSizeForTesting);
  transaction_->Put(object_store_id, std::move(value), std::move(key),
                    mojom::IDBPutMode::AddOrUpdate, Vector<IDBIndexKeys>(),
                    std::move(callback));
  EXPECT_TRUE(got_error);
}

}  // namespace
}  // namespace blink
