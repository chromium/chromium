// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/smart_card/smart_card_context.h"

#include <array>
#include <tuple>
#include <utility>

#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/device/public/mojom/smart_card.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_access_mode.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_connect_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_connect_result.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_connection_status.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_get_status_change_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_reader_state_in.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_transaction_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_transaction_options.h"
#include "third_party/blink/renderer/modules/smart_card/smart_card_connection.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

namespace blink {
namespace {

using device::mojom::blink::SmartCardConnectionState;
using device::mojom::blink::SmartCardConnectResultPtr;
using device::mojom::blink::SmartCardConnectSuccess;
using device::mojom::blink::SmartCardDisposition;
using device::mojom::blink::SmartCardListReadersResult;
using device::mojom::blink::SmartCardProtocol;
using device::mojom::blink::SmartCardProtocolsPtr;
using device::mojom::blink::SmartCardReaderStateInPtr;
using device::mojom::blink::SmartCardResult;
using device::mojom::blink::SmartCardShareMode;
using device::mojom::blink::SmartCardStatus;
using device::mojom::blink::SmartCardStatusResult;
using device::mojom::blink::SmartCardSuccess;
using device::mojom::blink::SmartCardTransactionResult;
using testing::_;
using testing::ElementsAre;
using testing::StrictMock;

class MockSmartCardContext : public device::mojom::blink::SmartCardContext {
 public:
  mojo::PendingRemote<device::mojom::blink::SmartCardContext>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void ResetReceiver() { receiver_.reset(); }

  MOCK_METHOD(void, ListReaders, (ListReadersCallback), (override));
  MOCK_METHOD(void,
              GetStatusChange,
              (base::TimeDelta,
               Vector<SmartCardReaderStateInPtr>,
               GetStatusChangeCallback),
              (override));
  MOCK_METHOD(void, Cancel, (CancelCallback), (override));
  MOCK_METHOD(
      void,
      Connect,
      (const String&,
       SmartCardShareMode,
       SmartCardProtocolsPtr,
       mojo::PendingRemote<device::mojom::blink::SmartCardConnectionWatcher>,
       ConnectCallback),
      (override));

 private:
  mojo::Receiver<device::mojom::blink::SmartCardContext> receiver_{this};
};

class MockSmartCardConnection
    : public device::mojom::blink::SmartCardConnection {
 public:
  mojo::PendingRemote<device::mojom::blink::SmartCardConnection>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(void,
              Disconnect,
              (SmartCardDisposition, DisconnectCallback),
              (override));
  MOCK_METHOD(void,
              Transmit,
              (SmartCardProtocol, const Vector<uint8_t>&, TransmitCallback),
              (override));
  MOCK_METHOD(void,
              Control,
              (uint32_t, const Vector<uint8_t>&, ControlCallback),
              (override));
  MOCK_METHOD(void, GetAttrib, (uint32_t, GetAttribCallback), (override));
  MOCK_METHOD(void,
              SetAttrib,
              (uint32_t, const Vector<uint8_t>&, SetAttribCallback),
              (override));
  MOCK_METHOD(void, Status, (StatusCallback), (override));
  MOCK_METHOD(void, BeginTransaction, (BeginTransactionCallback), (override));

 private:
  mojo::Receiver<device::mojom::blink::SmartCardConnection> receiver_{this};
};

class MockSmartCardTransaction
    : public device::mojom::blink::SmartCardTransaction {
 public:
  mojo::PendingAssociatedRemote<device::mojom::blink::SmartCardTransaction>
  BindNewEndpointAndPassRemote() {
    return receiver_.BindNewEndpointAndPassRemote();
  }

  MOCK_METHOD(void,
              EndTransaction,
              (SmartCardDisposition, EndTransactionCallback),
              (override));

 private:
  mojo::AssociatedReceiver<device::mojom::blink::SmartCardTransaction>
      receiver_{this};
};

class ConnectResolved final
    : public ThenCallable<SmartCardConnectResult, ConnectResolved> {
 public:
  void React(ScriptState*, SmartCardConnectResult* result) {
    connection_ = result->connection();
  }

  SmartCardConnection* connection() const { return connection_.Get(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(connection_);
    ThenCallable<SmartCardConnectResult, ConnectResolved>::Trace(visitor);
  }

 private:
  Member<SmartCardConnection> connection_;
};

void ReturnDataCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  info.GetReturnValue().Set(info.DataV2().As<v8::Value>());
}

void ReturnPendingPromiseCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Promise::Resolver> resolver =
      v8::Promise::Resolver::New(info.GetIsolate()->GetCurrentContext())
          .ToLocalChecked();
  info.GetReturnValue().Set(resolver->GetPromise());
}

class SmartCardContextTest : public testing::Test {
 protected:
  SmartCardConnection* Connect(V8TestingScope& scope,
                               SmartCardContext* context,
                               MockSmartCardContext& mock_context,
                               MockSmartCardConnection& mock_connection,
                               const String& reader_name) {
    EXPECT_CALL(mock_context, Connect(reader_name, _, _, _, _))
        .WillOnce([&mock_connection](
                      const String&, SmartCardShareMode, SmartCardProtocolsPtr,
                      mojo::PendingRemote<
                          device::mojom::blink::SmartCardConnectionWatcher>,
                      MockSmartCardContext::ConnectCallback callback) {
          std::move(callback).Run(
              device::mojom::blink::SmartCardConnectResult::NewSuccess(
                  SmartCardConnectSuccess::New(
                      mock_connection.BindNewPipeAndPassRemote(),
                      SmartCardProtocol::kT1)));
        });

    auto* resolved = MakeGarbageCollected<ConnectResolved>();
    ScriptPromise<SmartCardConnectResult> promise = context->connect(
        scope.GetScriptState(), reader_name,
        V8SmartCardAccessMode(V8SmartCardAccessMode::Enum::kShared),
        SmartCardConnectOptions::Create(), ASSERT_NO_EXCEPTION);
    promise.Then(scope.GetScriptState(), resolved);

    ScriptPromiseTester tester(scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsFulfilled());
    return resolved->connection();
  }

 private:
  test::TaskEnvironment task_environment_;
};

TEST_F(SmartCardContextTest,
       OperationInProgressCheckedBeforeContextAvailability) {
  V8TestingScope scope;
  StrictMock<MockSmartCardContext> mock_context;
  auto* context = MakeGarbageCollected<SmartCardContext>(
      mock_context.BindNewPipeAndPassRemote(), scope.GetExecutionContext());

  StrictMock<MockSmartCardConnection> mock_connection;
  SmartCardConnection* connection =
      Connect(scope, context, mock_context, mock_connection, "Reader");
  ASSERT_TRUE(connection);

  MockSmartCardConnection::StatusCallback status_callback;
  EXPECT_CALL(mock_connection, Status(_))
      .WillOnce(
          [&status_callback](MockSmartCardConnection::StatusCallback callback) {
            status_callback = std::move(callback);
          });

  ScriptPromise<SmartCardConnectionStatus> status_promise =
      connection->status(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  ScriptPromiseTester status_tester(scope.GetScriptState(), status_promise);
  test::RunPendingTasks();
  ASSERT_TRUE(status_callback);

  // Closing the context pipe while a connection operation is in flight leaves
  // the context both unavailable (`!scard_context_.is_bound()`) and busy
  // (`IsOperationInProgress()`).
  mock_context.ResetReceiver();
  test::RunPendingTasks();

  constexpr char kExpectedBusyMessage[] =
      "An operation is already in progress in this smart card context.";

  {
    DummyExceptionStateForTesting exception_state;
    context->listReaders(scope.GetScriptState(), exception_state);
    EXPECT_TRUE(exception_state.HadException());
    EXPECT_EQ(exception_state.Code(),
              static_cast<int>(DOMExceptionCode::kInvalidStateError));
    EXPECT_EQ(exception_state.Message(), kExpectedBusyMessage);
  }

  {
    DummyExceptionStateForTesting exception_state;
    context->getStatusChange(
        scope.GetScriptState(), HeapVector<Member<SmartCardReaderStateIn>>(),
        SmartCardGetStatusChangeOptions::Create(), exception_state);
    EXPECT_TRUE(exception_state.HadException());
    EXPECT_EQ(exception_state.Code(),
              static_cast<int>(DOMExceptionCode::kInvalidStateError));
    EXPECT_EQ(exception_state.Message(), kExpectedBusyMessage);
  }

  {
    DummyExceptionStateForTesting exception_state;
    context->connect(
        scope.GetScriptState(), "Reader",
        V8SmartCardAccessMode(V8SmartCardAccessMode::Enum::kShared),
        SmartCardConnectOptions::Create(), exception_state);
    EXPECT_TRUE(exception_state.HadException());
    EXPECT_EQ(exception_state.Code(),
              static_cast<int>(DOMExceptionCode::kInvalidStateError));
    EXPECT_EQ(exception_state.Message(), kExpectedBusyMessage);
  }

  // Once the connection operation finishes, the context reports that it is
  // unavailable.
  std::move(status_callback)
      .Run(SmartCardStatusResult::NewStatus(
          SmartCardStatus::New("Reader", SmartCardConnectionState::kSpecific,
                               SmartCardProtocol::kT1, Vector<uint8_t>())));
  status_tester.WaitUntilSettled();
  EXPECT_TRUE(status_tester.IsFulfilled());

  {
    DummyExceptionStateForTesting exception_state;
    context->listReaders(scope.GetScriptState(), exception_state);
    EXPECT_TRUE(exception_state.HadException());
    EXPECT_EQ(exception_state.Code(),
              static_cast<int>(DOMExceptionCode::kInvalidStateError));
    EXPECT_EQ(exception_state.Message(), "Context unavailable.");
  }
}

TEST_F(SmartCardContextTest, SettledTransactionsEndInConnectionCreationOrder) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();
  v8::Local<v8::Context> v8_context = scope.GetContext();

  StrictMock<MockSmartCardContext> mock_context;
  auto* context = MakeGarbageCollected<SmartCardContext>(
      mock_context.BindNewPipeAndPassRemote(), scope.GetExecutionContext());

  constexpr size_t kNumConnections = 8;
  std::array<StrictMock<MockSmartCardConnection>, kNumConnections>
      mock_connections;
  std::array<StrictMock<MockSmartCardTransaction>, kNumConnections>
      mock_transactions;
  std::array<v8::Global<v8::Promise::Resolver>, kNumConnections>
      callback_resolvers;
  std::array<v8::Global<v8::Promise>, kNumConnections> transaction_promises;

  for (size_t i = 0; i < kNumConnections; ++i) {
    SmartCardConnection* connection =
        Connect(scope, context, mock_context, mock_connections[i],
                String("Reader ") + String::Number(i));
    ASSERT_TRUE(connection);

    v8::Local<v8::Promise::Resolver> resolver =
        v8::Promise::Resolver::New(v8_context).ToLocalChecked();
    callback_resolvers[i].Reset(isolate, resolver);

    v8::Local<v8::Function> callback_fn =
        v8::Function::New(v8_context, &ReturnDataCallback,
                          resolver->GetPromise())
            .ToLocalChecked();

    EXPECT_CALL(mock_connections[i], BeginTransaction(_))
        .WillOnce([&mock_transaction = mock_transactions[i]](
                      MockSmartCardConnection::BeginTransactionCallback cb) {
          std::move(cb).Run(SmartCardTransactionResult::NewTransaction(
              mock_transaction.BindNewEndpointAndPassRemote()));
        });

    ScriptPromise<IDLUndefined> tx_promise = connection->startTransaction(
        scope.GetScriptState(),
        V8SmartCardTransactionCallback::Create(callback_fn),
        SmartCardTransactionOptions::Create(), ASSERT_NO_EXCEPTION);
    transaction_promises[i].Reset(isolate, tx_promise.V8Promise());

    test::RunPendingTasks();
    scope.PerformMicrotaskCheckpoint();
    EXPECT_FALSE(context->IsOperationInProgress());
  }

  MockSmartCardContext::ListReadersCallback list_readers_callback;
  EXPECT_CALL(mock_context, ListReaders(_))
      .WillOnce([&list_readers_callback](
                    MockSmartCardContext::ListReadersCallback callback) {
        list_readers_callback = std::move(callback);
      });

  ScriptPromise<IDLSequence<IDLString>> list_promise =
      context->listReaders(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  ScriptPromiseTester list_tester(scope.GetScriptState(), list_promise);
  test::RunPendingTasks();
  ASSERT_TRUE(list_readers_callback);

  // Settle the transaction callbacks in reverse order while listReaders() is in
  // progress, so only `connections_` creation order can yield 0..7.
  for (size_t i = kNumConnections; i > 0; --i) {
    callback_resolvers[i - 1]
        .Get(isolate)
        ->Resolve(v8_context, V8String(isolate, "leave"))
        .Check();
  }
  scope.PerformMicrotaskCheckpoint();

  Vector<size_t> end_order;
  for (size_t i = 0; i < kNumConnections; ++i) {
    EXPECT_CALL(mock_transactions[i],
                EndTransaction(SmartCardDisposition::kLeave, _))
        .WillOnce([&end_order, i](
                      SmartCardDisposition,
                      MockSmartCardTransaction::EndTransactionCallback cb) {
          end_order.push_back(i);
          std::move(cb).Run(SmartCardResult::NewSuccess(SmartCardSuccess::kOk));
        });
  }

  std::move(list_readers_callback)
      .Run(SmartCardListReadersResult::NewReaders(Vector<String>()));
  list_tester.WaitUntilSettled();
  EXPECT_TRUE(list_tester.IsFulfilled());

  for (size_t i = 0; i < kNumConnections; ++i) {
    ScriptPromiseTester tx_tester(
        scope.GetScriptState(),
        ScriptPromise<IDLUndefined>::FromV8Promise(
            isolate, transaction_promises[i].Get(isolate)));
    tx_tester.WaitUntilSettled();
    // A startTransaction() promise is rejected if its callback settles while
    // another operation is in progress.
    EXPECT_TRUE(tx_tester.IsRejected());
  }

  EXPECT_THAT(end_order, ElementsAre(0, 1, 2, 3, 4, 5, 6, 7));
}

TEST_F(SmartCardContextTest,
       NeverSettlingTransactionCallbackCollectedWithoutCrash) {
  V8TestingScope scope;
  StrictMock<MockSmartCardContext> mock_context;
  StrictMock<MockSmartCardConnection> mock_connection;
  StrictMock<MockSmartCardTransaction> mock_transaction;

  Persistent<SmartCardContext> context = MakeGarbageCollected<SmartCardContext>(
      mock_context.BindNewPipeAndPassRemote(), scope.GetExecutionContext());
  WeakPersistent<SmartCardConnection> weak_connection;

  {
    v8::HandleScope handle_scope(scope.GetIsolate());
    SmartCardConnection* connection =
        Connect(scope, context, mock_context, mock_connection, "Reader");
    ASSERT_TRUE(connection);
    weak_connection = connection;

    v8::Local<v8::Function> callback_fn =
        v8::Function::New(scope.GetContext(), &ReturnPendingPromiseCallback)
            .ToLocalChecked();

    EXPECT_CALL(mock_connection, BeginTransaction(_))
        .WillOnce([&mock_transaction](
                      MockSmartCardConnection::BeginTransactionCallback cb) {
          std::move(cb).Run(SmartCardTransactionResult::NewTransaction(
              mock_transaction.BindNewEndpointAndPassRemote()));
        });

    std::ignore = connection->startTransaction(
        scope.GetScriptState(),
        V8SmartCardTransactionCallback::Create(callback_fn),
        SmartCardTransactionOptions::Create(), ASSERT_NO_EXCEPTION);

    test::RunPendingTasks();
    scope.PerformMicrotaskCheckpoint();
    EXPECT_FALSE(context->IsOperationInProgress());
  }

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_FALSE(weak_connection);
}

}  // namespace
}  // namespace blink
