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
#include <optional>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_idb_request_ready_state.h"
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
#include "third_party/blink/renderer/modules/indexeddb/idb_record_array.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_request_queue_item.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_test_helper.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_transaction.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value_wrapping.h"
#include "third_party/blink/renderer/modules/indexeddb/mock_idb_database.h"
#include "third_party/blink/renderer/modules/indexeddb/mock_idb_transaction.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/blob/testing/fake_blob_registry.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

namespace blink {
namespace {

// Creates a new `ArrayBuffer` initialized to `array_buffer_bytes`.
v8::Local<v8::ArrayBuffer> CreateArrayBuffer(
    v8::Isolate* isolate,
    base::span<const uint8_t> array_buffer_bytes) {
  v8::Local<v8::ArrayBuffer> array_buffer =
      v8::ArrayBuffer::New(isolate, array_buffer_bytes.size());
  UNSAFE_TODO(std::memcpy(array_buffer->GetBackingStore()->Data(),
                          array_buffer_bytes.data(),
                          array_buffer_bytes.size()));
  return array_buffer;
}

// Creates a `IDBReturnValuePtr` for `v8_value` wrapped in a `Blob`.
mojom::blink::IDBReturnValuePtr CreateIDBReturnValuePtrWithBlob(
    v8::Isolate* isolate,
    v8::Local<v8::Value> v8_value) {
  NonThrowableExceptionState non_throwable_exception_state;
  IDBValueWrapper wrapper(
      isolate, v8_value, SerializedScriptValue::SerializeOptions::kSerialize,
      non_throwable_exception_state, /*backend_uses_sqlite=*/false);
  wrapper.set_wrapping_threshold_for_test(0);
  wrapper.DoneCloning();

  mojom::blink::IDBReturnValuePtr idb_return_value =
      mojom::blink::IDBReturnValue::New();
  idb_return_value->value = std::move(wrapper).Build();

  idb_return_value->primary_key = IDBKey::CreateNone();
  return idb_return_value;
}

// Creates an `IDBValue` for `v8_value` serialized.
std::unique_ptr<IDBValue> CreateIDBValueWithV8Value(
    v8::Isolate* isolate,
    v8::Local<v8::Value> v8_value) {
  NonThrowableExceptionState non_throwable_exception_state;
  IDBValueWrapper wrapper(
      isolate, v8_value, SerializedScriptValue::SerializeOptions::kSerialize,
      non_throwable_exception_state, /*backend_uses_sqlite=*/false);
  wrapper.DoneCloning();
  return std::move(wrapper).Build();
}

// Generates a batch of records to stream for a get all request.  Use the bool
// parameters to configure which parts of the record should remain empty.  For
// example, `getAll()` should only generate values, leaving the primary keys and
// index keys empty.
Vector<mojom::blink::IDBRecordPtr> GenerateGetAllResults(
    wtf_size_t result_count,
    bool generate_primary_key,
    bool generate_value,
    bool generate_index_key) {
  static int next_id = 0;

  Vector<mojom::blink::IDBRecordPtr> result_records;
  for (wtf_size_t i = 0; i < result_count; ++i) {
    ++next_id;

    std::optional<std::unique_ptr<IDBKey>> primary_key;
    if (generate_primary_key) {
      primary_key =
          IDBKey::CreateString(u"primary_key_" + String::Number(next_id));
    }

    mojom::blink::IDBReturnValuePtr return_value;
    if (generate_value) {
      String value_string = u"value_" + String::Number(next_id);
      Vector<char> value_bytes(value_string.Utf8());
      return_value = mojom::blink::IDBReturnValue::New();
      (return_value->value = std::make_unique<IDBValue>())
          ->SetData(std::move(value_bytes));
      return_value->primary_key = IDBKey::CreateNone();
    }

    std::optional<std::unique_ptr<IDBKey>> index_key;
    if (generate_index_key) {
      index_key = IDBKey::CreateString(u"index_key_" + String::Number(next_id));
    }

    result_records.push_back(mojom::blink::IDBRecord::New(
        std::move(primary_key), std::move(return_value), std::move(index_key)));
  }
  return result_records;
}

// Converts batches of get all results to an `IDBRecordArray`.  Test may use
// this to generate the expected results for a get all request.
IDBRecordArray ToIDBRecordArray(
    const Vector<Vector<mojom::blink::IDBRecordPtr>>& get_all_results) {
  IDBRecordArray record_array;
  for (const Vector<mojom::blink::IDBRecordPtr>& records : get_all_results) {
    for (const mojom::blink::IDBRecordPtr& record : records) {
      if (record->primary_key) {
        record_array.primary_keys.push_back(
            IDBKey::Clone(record->primary_key->get()));
      }

      if (record->return_value) {
        mojom::blink::IDBReturnValuePtr& idb_return_value =
            record->return_value;
        Vector<char> expected_value_bytes(idb_return_value->value->Data());
        std::unique_ptr<IDBValue> expected_value = std::make_unique<IDBValue>();
        expected_value->SetData(std::move(expected_value_bytes));

        // Copy the injected primary key when it exists.
        if (idb_return_value->primary_key->IsValid()) {
          expected_value->SetInjectedPrimaryKey(
              IDBKey::Clone(idb_return_value->primary_key.get()),
              idb_return_value->key_path);
        }

        record_array.values.push_back(std::move(expected_value));
      }

      if (record->index_key) {
        record_array.index_keys.push_back(
            IDBKey::Clone(record->index_key->get()));
      }
    }
  }
  return record_array;
}

void ExpectEqualsIDBKeyArray(
    const Vector<std::unique_ptr<IDBKey>>& actual_keys,
    const Vector<std::unique_ptr<IDBKey>>& expected_keys) {
  ASSERT_EQ(actual_keys.size(), expected_keys.size());

  for (size_t i = 0; i < expected_keys.size(); ++i) {
    ASSERT_NE(actual_keys[i], nullptr);
    EXPECT_TRUE(actual_keys[i]->IsEqual(expected_keys[i].get()));
  }
}

void ExpectEqualsIDBValueArray(
    const Vector<std::unique_ptr<IDBValue>>& actual_values,
    const Vector<std::unique_ptr<IDBValue>>& expected_values) {
  ASSERT_EQ(actual_values.size(), expected_values.size());

  for (size_t i = 0; i < expected_values.size(); ++i) {
    ASSERT_NE(actual_values[i], nullptr);
    EXPECT_EQ(actual_values[i]->Data(), expected_values[i]->Data());

    // Compare the injected primary key when it exists.
    if (expected_values[i]->PrimaryKey()) {
      ASSERT_NE(actual_values[i]->PrimaryKey(), nullptr);
      EXPECT_TRUE(actual_values[i]->PrimaryKey()->IsEqual(
          expected_values[i]->PrimaryKey()));
    } else {
      EXPECT_EQ(actual_values[i]->PrimaryKey(), nullptr);
    }
    EXPECT_EQ(actual_values[i]->KeyPath(), expected_values[i]->KeyPath());
  }
}

void ExpectEqualsGetAllResults(const IDBAny* actual_result,
                               mojom::blink::IDBGetAllResultType expected_type,
                               IDBRecordArray expected_records) {
  ASSERT_NE(actual_result, nullptr);

  switch (expected_type) {
    // Verify `getAllKeys()`.
    case mojom::blink::IDBGetAllResultType::Keys: {
      ASSERT_EQ(actual_result->GetType(), IDBAny::Type::kKeyType);
      const IDBKey* actual_key_result = actual_result->Key();

      ASSERT_NE(actual_key_result, nullptr);
      ASSERT_EQ(actual_key_result->GetType(), mojom::blink::IDBKeyType::Array);

      const Vector<std::unique_ptr<IDBKey>>& actual_keys =
          actual_key_result->Array();

      ASSERT_NO_FATAL_FAILURE(
          ExpectEqualsIDBKeyArray(actual_keys, expected_records.primary_keys));
      break;
    }

    // Verify `getAll()`.
    case mojom::blink::IDBGetAllResultType::Values: {
      ASSERT_EQ(actual_result->GetType(), IDBAny::Type::kIDBValueArrayType);
      const Vector<std::unique_ptr<IDBValue>>& actual_values =
          actual_result->Values();

      ASSERT_NO_FATAL_FAILURE(
          ExpectEqualsIDBValueArray(actual_values, expected_records.values));
      break;
    }

    // Verify `getAllRecords()`.
    case mojom::blink::IDBGetAllResultType::Records: {
      ASSERT_EQ(actual_result->GetType(), IDBAny::Type::kIDBRecordArrayType);
      const IDBRecordArray& actual_records = actual_result->Records();

      ASSERT_NO_FATAL_FAILURE(ExpectEqualsIDBKeyArray(
          actual_records.primary_keys, expected_records.primary_keys));

      ASSERT_NO_FATAL_FAILURE(ExpectEqualsIDBValueArray(
          actual_records.values, expected_records.values));

      ASSERT_NO_FATAL_FAILURE(ExpectEqualsIDBKeyArray(
          actual_records.index_keys, expected_records.index_keys));
      break;
    }
  }
}

}  // namespace

class BackendDatabaseWithMockedClose
    : public testing::StrictMock<mojom::blink::IDBDatabase> {
 public:
  explicit BackendDatabaseWithMockedClose(
      mojo::PendingAssociatedReceiver<mojom::blink::IDBDatabase>
          pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {
    receiver_.set_disconnect_handler(
        BindOnce(&BackendDatabaseWithMockedClose::DatabaseDestroyed,
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
    BlobDataHandle::SetBlobRegistryForTesting(blob_registry_remote_.get());

    url_loader_mock_factory_ = URLLoaderMockFactory::GetSingletonInstance();
    WebURLResponse response;
    response.SetCurrentRequestUrl(KURL("blob:"));
    url_loader_mock_factory_->RegisterURLProtocol(WebString("blob"), response,
                                                  "");
  }

  void TearDown() override {
    url_loader_mock_factory_->UnregisterAllURLsAndClearMemoryCache();
    BlobDataHandle::SetBlobRegistryForTesting(nullptr);
  }

  void BuildTransaction(V8TestingScope& scope,
                        MockIDBDatabase& mock_database,
                        MockIDBTransaction& mock_transaction_remote) {
    auto* execution_context = scope.GetExecutionContext();

    db_ = MakeGarbageCollected<IDBDatabase>(
        execution_context, mojo::NullAssociatedReceiver(),
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
        new IDBObjectStoreMetadata("store", kStoreId, store_key_path, true));
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

  // Creates a new get all `IDBRequest` using `get_all_type` and
  // `get_all_results`. Verifies the `IDBRequest` completes with
  // `expected_results`.
  void TestGetAll(V8TestingScope& scope,
                  mojom::blink::IDBGetAllResultType get_all_type,
                  Vector<Vector<mojom::blink::IDBRecordPtr>> get_all_results,
                  IDBRecordArray expected_results) {
    // Set up the transaction.
    MockIDBDatabase database_backend;
    MockIDBTransaction transaction_backend;
    EXPECT_CALL(transaction_backend, Commit(0)).Times(1);
    BuildTransaction(scope, database_backend, transaction_backend);
    ASSERT_TRUE(!scope.GetExceptionState().HadException());
    ASSERT_TRUE(transaction_);

    // Create the get all request.
    IDBRequest* request =
        IDBRequest::Create(scope.GetScriptState(), store_.Get(),
                           transaction_.Get(), IDBRequest::AsyncTraceState());
    mojo::AssociatedRemote<mojom::blink::IDBDatabaseGetAllResultSink>
        get_all_sink_remote;
    request->OnGetAll(
        get_all_type,
        get_all_sink_remote.BindNewEndpointAndPassDedicatedReceiver());

    // Stream batches of results to the request.
    for (Vector<mojom::blink::IDBRecordPtr>& records : get_all_results) {
      get_all_sink_remote->ReceiveResults(std::move(records),
                                          /*done=*/false);
    }
    get_all_sink_remote->ReceiveResults(/*records=*/{}, /*done=*/true);
    scope.PerformMicrotaskCheckpoint();
    platform_->RunUntilIdle();

    // Verify the request completes with the expected results.
    EXPECT_EQ(request->readyState(), V8IDBRequestReadyState::Enum::kDone);
    IDBAny* actual_result = request->ResultAsAny();
    ASSERT_NO_FATAL_FAILURE(ExpectEqualsGetAllResults(
        actual_result, get_all_type, std::move(expected_results)));
  }

  test::TaskEnvironment task_environment_;
  raw_ptr<URLLoaderMockFactory> url_loader_mock_factory_;
  Persistent<IDBDatabase> db_;
  Persistent<IDBTransaction> transaction_;
  Persistent<IDBObjectStore> store_;
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;

  // Setup the blob registry to test get all requests that include results with
  // IDB values wrapped in a blob.
  FakeBlobRegistry fake_blob_registry_{/*support_binary_blob_bodies=*/true};
  mojo::Remote<mojom::blink::BlobRegistry> blob_registry_remote_;
  mojo::Receiver<mojom::blink::BlobRegistry> blob_registry_receiver_{
      &fake_blob_registry_, blob_registry_remote_.BindNewPipeAndPassReceiver()};

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

  EXPECT_EQ(request->readyState(), V8IDBRequestReadyState::Enum::kPending);
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
  EXPECT_EQ(request->readyState(), V8IDBRequestReadyState::Enum::kPending);
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
  EXPECT_EQ(request1->readyState(), V8IDBRequestReadyState::Enum::kPending);
  EXPECT_EQ(request2->readyState(), V8IDBRequestReadyState::Enum::kPending);
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
        IDBRequest::AsyncTraceState());
    EXPECT_EQ(request->readyState(), V8IDBRequestReadyState::Enum::kPending);
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
        IDBRequest::AsyncTraceState());
    EXPECT_EQ(request->readyState(), V8IDBRequestReadyState::Enum::kPending);
    std::unique_ptr<IDBFactoryClient> factory_client =
        request->CreateFactoryClient();

    scope.GetExecutionContext()->NotifyContextDestroyed();
    factory_client->OpenSuccess(remote.Unbind(), metadata);
    platform_->RunUntilIdle();
  }
}

TEST_F(IDBRequestTest, GetAllKeysEmpty) {
  V8TestingScope scope;
  ASSERT_NO_FATAL_FAILURE(TestGetAll(scope,
                                     mojom::blink::IDBGetAllResultType::Keys,
                                     /*get_all_results=*/{},
                                     /*expected_results=*/{}));
}

TEST_F(IDBRequestTest, GetAllValuesEmpty) {
  V8TestingScope scope;
  ASSERT_NO_FATAL_FAILURE(TestGetAll(scope,
                                     mojom::blink::IDBGetAllResultType::Values,
                                     /*get_all_results=*/{},
                                     /*expected_results=*/{}));
}

TEST_F(IDBRequestTest, GetAllRecordsEmpty) {
  V8TestingScope scope;
  ASSERT_NO_FATAL_FAILURE(TestGetAll(scope,
                                     mojom::blink::IDBGetAllResultType::Records,
                                     /*get_all_results=*/{},
                                     /*expected_results=*/{}));
}

TEST_F(IDBRequestTest, GetAllKeys) {
  Vector<Vector<mojom::blink::IDBRecordPtr>> get_all_results;
  get_all_results.push_back(GenerateGetAllResults(
      /*result_count=*/3, /*generate_primary_key=*/true,
      /*generate_value=*/false, /*generate_index_key=*/false));
  get_all_results.push_back(GenerateGetAllResults(
      /*result_count=*/2, /*generate_primary_key=*/true,
      /*generate_value=*/false, /*generate_index_key=*/false));

  V8TestingScope scope;
  IDBRecordArray expected_results = ToIDBRecordArray(get_all_results);
  ASSERT_NO_FATAL_FAILURE(
      TestGetAll(scope, mojom::blink::IDBGetAllResultType::Keys,
                 std::move(get_all_results), std::move(expected_results)));
}

TEST_F(IDBRequestTest, GetAllValues) {
  Vector<Vector<mojom::blink::IDBRecordPtr>> get_all_results;
  get_all_results.push_back(GenerateGetAllResults(
      /*result_count=*/5, /*generate_primary_key=*/false,
      /*generate_value=*/true, /*generate_index_key=*/false));
  get_all_results.push_back(GenerateGetAllResults(
      /*result_count=*/7, /*generate_primary_key=*/false,
      /*generate_value=*/true, /*generate_index_key=*/false));

  V8TestingScope scope;
  IDBRecordArray expected_results = ToIDBRecordArray(get_all_results);
  ASSERT_NO_FATAL_FAILURE(
      TestGetAll(scope, mojom::blink::IDBGetAllResultType::Values,
                 std::move(get_all_results), std::move(expected_results)));
}

TEST_F(IDBRequestTest, GetAllValuesWithBlob) {
  V8TestingScope scope;

  Vector<Vector<mojom::blink::IDBRecordPtr>> get_all_results;
  get_all_results.push_back(GenerateGetAllResults(
      /*result_count=*/11, /*generate_primary_key=*/false,
      /*generate_value=*/true, /*generate_index_key=*/false));

  IDBRecordArray expected_results = ToIDBRecordArray(get_all_results);

  // Append a blob value to the results.
  const uint8_t array_buffer_bytes[] = {1, 2, 3, 4, 5, 6, 7, 8};
  v8::Local<v8::ArrayBuffer> array_buffer =
      CreateArrayBuffer(scope.GetIsolate(), array_buffer_bytes);

  mojom::blink::IDBReturnValuePtr blob_return_value =
      CreateIDBReturnValuePtrWithBlob(scope.GetIsolate(), array_buffer);

  get_all_results.back().emplace_back(mojom::blink::IDBRecord::New(
      /*primary_key=*/std::nullopt, std::move(blob_return_value),
      /*index_key=*/std::nullopt));

  // Append the unwrapped blob value to the expected results.
  std::unique_ptr<IDBValue> blob_expected_value =
      CreateIDBValueWithV8Value(scope.GetIsolate(), array_buffer);

  expected_results.values.emplace_back(std::move(blob_expected_value));

  ASSERT_NO_FATAL_FAILURE(
      TestGetAll(scope, mojom::blink::IDBGetAllResultType::Values,
                 std::move(get_all_results), std::move(expected_results)));
}

TEST_F(IDBRequestTest, GetAllValuesWithInjectedKey) {
  Vector<Vector<mojom::blink::IDBRecordPtr>> get_all_results;
  get_all_results.push_back(GenerateGetAllResults(
      /*result_count=*/6, /*generate_primary_key=*/false,
      /*generate_value=*/true, /*generate_index_key=*/false));

  std::unique_ptr<IDBKey> injected_primary_key =
      IDBKey::CreateString(u"injected_primary_key");
  IDBKeyPath injected_key_path("injected_key_path");

  // Add an injected key to the last result.
  mojom::blink::IDBReturnValuePtr& last_value =
      get_all_results.back().back()->return_value;
  last_value->primary_key = IDBKey::Clone(injected_primary_key);
  last_value->key_path = injected_key_path;

  IDBRecordArray expected_results = ToIDBRecordArray(get_all_results);

  V8TestingScope scope;
  ASSERT_NO_FATAL_FAILURE(
      TestGetAll(scope, mojom::blink::IDBGetAllResultType::Values,
                 std::move(get_all_results), std::move(expected_results)));
}

TEST_F(IDBRequestTest, GetAllRecords) {
  Vector<Vector<mojom::blink::IDBRecordPtr>> get_all_results;
  get_all_results.push_back(GenerateGetAllResults(
      /*result_count=*/10, /*generate_primary_key=*/true,
      /*generate_value=*/true, /*generate_index_key=*/true));
  get_all_results.push_back(GenerateGetAllResults(
      /*result_count=*/9, /*generate_primary_key=*/true,
      /*generate_value=*/true, /*generate_index_key=*/true));

  V8TestingScope scope;
  IDBRecordArray expected_results = ToIDBRecordArray(get_all_results);
  ASSERT_NO_FATAL_FAILURE(
      TestGetAll(scope, mojom::blink::IDBGetAllResultType::Records,
                 std::move(get_all_results), std::move(expected_results)));
}

TEST_F(IDBRequestTest, GetAllRecordsWithoutIndexKeys) {
  Vector<Vector<mojom::blink::IDBRecordPtr>> get_all_results;
  get_all_results.push_back(GenerateGetAllResults(
      /*result_count=*/10, /*generate_primary_key=*/true,
      /*generate_value=*/true, /*generate_index_key=*/false));
  get_all_results.push_back(GenerateGetAllResults(
      /*result_count=*/9, /*generate_primary_key=*/true,
      /*generate_value=*/true, /*generate_index_key=*/false));

  V8TestingScope scope;
  IDBRecordArray expected_results = ToIDBRecordArray(get_all_results);
  ASSERT_NO_FATAL_FAILURE(
      TestGetAll(scope, mojom::blink::IDBGetAllResultType::Records,
                 std::move(get_all_results), std::move(expected_results)));
}

TEST_F(IDBRequestTest, GetAllRecordsWithBlob) {
  V8TestingScope scope;

  Vector<Vector<mojom::blink::IDBRecordPtr>> get_all_results;
  get_all_results.push_back(GenerateGetAllResults(
      /*result_count=*/5, /*generate_primary_key=*/true,
      /*generate_value=*/true, /*generate_index_key=*/true));

  IDBRecordArray expected_results = ToIDBRecordArray(get_all_results);

  // Append a blob value to the results.
  const uint8_t array_buffer_bytes[] = {100, 101, 102, 103, 104, 105,
                                        106, 107, 108, 109, 110, 111,
                                        112, 113, 114, 115, 116};
  v8::Local<v8::ArrayBuffer> array_buffer =
      CreateArrayBuffer(scope.GetIsolate(), array_buffer_bytes);

  mojom::blink::IDBReturnValuePtr blob_return_value =
      CreateIDBReturnValuePtrWithBlob(scope.GetIsolate(), array_buffer);

  std::unique_ptr<IDBKey> blob_primary_key =
      IDBKey::CreateString(u"blob_primary_key");
  std::unique_ptr<IDBKey> blob_index_key =
      IDBKey::CreateString(u"blob_index_key");

  get_all_results.back().emplace_back(mojom::blink::IDBRecord::New(
      IDBKey::Clone(blob_primary_key), std::move(blob_return_value),
      IDBKey::Clone(blob_index_key)));

  // Append the unwrapped blob value to the expected results.
  std::unique_ptr<IDBValue> blob_expected_value =
      CreateIDBValueWithV8Value(scope.GetIsolate(), array_buffer);

  expected_results.primary_keys.emplace_back(std::move(blob_primary_key));
  expected_results.values.emplace_back(std::move(blob_expected_value));
  expected_results.index_keys.emplace_back(std::move(blob_index_key));

  ASSERT_NO_FATAL_FAILURE(
      TestGetAll(scope, mojom::blink::IDBGetAllResultType::Records,
                 std::move(get_all_results), std::move(expected_results)));
}

TEST_F(IDBRequestTest, GetAllRecordsWithInjectedKey) {
  Vector<Vector<mojom::blink::IDBRecordPtr>> get_all_results;
  get_all_results.push_back(GenerateGetAllResults(
      /*result_count=*/6, /*generate_primary_key=*/true,
      /*generate_value=*/true, /*generate_index_key=*/true));

  std::unique_ptr<IDBKey> injected_primary_key =
      IDBKey::CreateString(u"injected_primary_key");
  IDBKeyPath injected_key_path("injected_key_path");

  // Add an injected key to the last result.
  mojom::blink::IDBReturnValuePtr& last_value =
      get_all_results.back().back()->return_value;
  last_value->primary_key = IDBKey::Clone(injected_primary_key);
  last_value->key_path = injected_key_path;

  IDBRecordArray expected_results = ToIDBRecordArray(get_all_results);

  V8TestingScope scope;
  ASSERT_NO_FATAL_FAILURE(
      TestGetAll(scope, mojom::blink::IDBGetAllResultType::Records,
                 std::move(get_all_results), std::move(expected_results)));
}

TEST_F(IDBRequestTest, AbortGetAll) {
  V8TestingScope scope;

  // Set up the transaction.
  MockIDBDatabase database_backend;
  MockIDBTransaction transaction_backend;
  BuildTransaction(scope, database_backend, transaction_backend);
  ASSERT_TRUE(!scope.GetExceptionState().HadException());
  ASSERT_TRUE(transaction_);

  // Create the get all request.
  IDBRequest* request =
      IDBRequest::Create(scope.GetScriptState(), store_.Get(),
                         transaction_.Get(), IDBRequest::AsyncTraceState());
  mojo::AssociatedRemote<mojom::blink::IDBDatabaseGetAllResultSink>
      get_all_sink_remote;
  request->OnGetAll(
      mojom::blink::IDBGetAllResultType::Values,
      get_all_sink_remote.BindNewEndpointAndPassDedicatedReceiver());

  // Abort the request and verify the results.
  request->Abort(/*queue_dispatch=*/true);
  scope.PerformMicrotaskCheckpoint();
  platform_->RunUntilIdle();

  EXPECT_EQ(request->readyState(), V8IDBRequestReadyState::Enum::kDone);

  IDBAny* actual_result = request->ResultAsAny();
  ASSERT_NE(actual_result, nullptr);
  EXPECT_EQ(actual_result->GetType(), IDBAny::Type::kUndefinedType);

  NonThrowableExceptionState exception_state;
  DOMException* error = request->error(exception_state);
  ASSERT_NE(error, nullptr);
  EXPECT_EQ(error->name(), "AbortError");
}

TEST_F(IDBRequestTest, AbortGetAllWhileReceivingValues) {
  V8TestingScope scope;

  // Set up the transaction.
  MockIDBDatabase database_backend;
  MockIDBTransaction transaction_backend;
  BuildTransaction(scope, database_backend, transaction_backend);
  ASSERT_TRUE(!scope.GetExceptionState().HadException());
  ASSERT_TRUE(transaction_);

  // Create the get all request.
  IDBRequest* request =
      IDBRequest::Create(scope.GetScriptState(), store_.Get(),
                         transaction_.Get(), IDBRequest::AsyncTraceState());
  mojo::AssociatedRemote<mojom::blink::IDBDatabaseGetAllResultSink>
      get_all_sink_remote;
  request->OnGetAll(
      mojom::blink::IDBGetAllResultType::Values,
      get_all_sink_remote.BindNewEndpointAndPassDedicatedReceiver());

  // Stream a batch of results to the request.
  Vector<mojom::blink::IDBRecordPtr> records = GenerateGetAllResults(
      /*result_count=*/3, /*generate_primary_key=*/false,
      /*generate_value=*/true, /*generate_index_key=*/false);
  get_all_sink_remote->ReceiveResults(std::move(records),
                                      /*done=*/false);

  // Wait for the request to process the result records before aborting.
  platform_->RunUntilIdle();

  // Abort the request and verify the results.
  request->Abort(/*queue_dispatch=*/true);
  scope.PerformMicrotaskCheckpoint();
  platform_->RunUntilIdle();

  EXPECT_EQ(request->readyState(), V8IDBRequestReadyState::Enum::kDone);

  IDBAny* actual_result = request->ResultAsAny();
  ASSERT_NE(actual_result, nullptr);
  EXPECT_EQ(actual_result->GetType(), IDBAny::Type::kUndefinedType);

  NonThrowableExceptionState exception_state;
  DOMException* error = request->error(exception_state);
  ASSERT_NE(error, nullptr);
  EXPECT_EQ(error->name(), "AbortError");
}

TEST_F(IDBRequestTest, AbortGetAllWhileLoadingValues) {
  V8TestingScope scope;

  // Remove the blob registry to force timeouts when loading blob values. This
  // gives the test a chance to abort the request while waiting for the blob to
  // load.
  BlobDataHandle::SetBlobRegistryForTesting(nullptr);

  // Set up the transaction.
  MockIDBDatabase database_backend;
  MockIDBTransaction transaction_backend;
  BuildTransaction(scope, database_backend, transaction_backend);
  ASSERT_TRUE(!scope.GetExceptionState().HadException());
  ASSERT_TRUE(transaction_);

  // Create the get all request.
  IDBRequest* request =
      IDBRequest::Create(scope.GetScriptState(), store_.Get(),
                         transaction_.Get(), IDBRequest::AsyncTraceState());
  mojo::AssociatedRemote<mojom::blink::IDBDatabaseGetAllResultSink>
      get_all_sink_remote;
  request->OnGetAll(
      mojom::blink::IDBGetAllResultType::Values,
      get_all_sink_remote.BindNewEndpointAndPassDedicatedReceiver());

  // Stream a batch of results to the request.  Include a blob value that
  // requires loading.
  const uint8_t array_buffer_bytes[] = {200, 201};
  v8::Local<v8::ArrayBuffer> array_buffer =
      CreateArrayBuffer(scope.GetIsolate(), array_buffer_bytes);

  mojom::blink::IDBReturnValuePtr blob_return_value =
      CreateIDBReturnValuePtrWithBlob(scope.GetIsolate(), array_buffer);

  Vector<mojom::blink::IDBRecordPtr> records = GenerateGetAllResults(
      /*result_count=*/3, /*generate_primary_key=*/false,
      /*generate_value=*/true, /*generate_index_key=*/false);
  records.emplace_back(mojom::blink::IDBRecord::New(
      /*primary_key=*/std::nullopt, std::move(blob_return_value),
      /*index_key=*/std::nullopt));

  get_all_sink_remote->ReceiveResults(std::move(records),
                                      /*done=*/true);

  // Wait for the request to load the result values before aborting.
  platform_->RunUntilIdle();

  // Abort the request and verify the results.
  request->Abort(/*queue_dispatch=*/true);
  scope.PerformMicrotaskCheckpoint();
  platform_->RunUntilIdle();

  EXPECT_EQ(request->readyState(), V8IDBRequestReadyState::Enum::kDone);

  IDBAny* actual_result = request->ResultAsAny();
  ASSERT_NE(actual_result, nullptr);
  EXPECT_EQ(actual_result->GetType(), IDBAny::Type::kUndefinedType);

  NonThrowableExceptionState exception_state;
  DOMException* error = request->error(exception_state);
  ASSERT_NE(error, nullptr);
  EXPECT_EQ(error->name(), "AbortError");
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
