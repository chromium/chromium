// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/smart_card/smart_card_connection.h"

#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_connection_status.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_disposition.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_protocol.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_transaction_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_transaction_options.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/smart_card/smart_card_cancel_algorithm.h"
#include "third_party/blink/renderer/modules/smart_card/smart_card_context.h"
#include "third_party/blink/renderer/modules/smart_card/smart_card_error.h"
#include "third_party/blink/renderer/modules/smart_card/smart_card_util.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_remote.h"

namespace blink {
namespace {
constexpr char kOperationInProgress[] = "An operation is in progress.";
constexpr char kDisconnected[] = "Is disconnected.";
constexpr char kTransactionAlreadyExists[] =
    "This connection already has an active transaction.";
constexpr char kTransactionEndedWithPendingOperation[] =
    "Transaction callback returned while an operation was still in progress.";

using device::mojom::blink::SmartCardConnectionState;
using device::mojom::blink::SmartCardDisposition;
using device::mojom::blink::SmartCardProtocol;
using device::mojom::blink::SmartCardStatusPtr;

SmartCardDisposition ToMojomDisposition(
    const V8SmartCardDisposition& disposition) {
  switch (disposition.AsEnum()) {
    case V8SmartCardDisposition::Enum::kLeave:
      return SmartCardDisposition::kLeave;
    case V8SmartCardDisposition::Enum::kReset:
      return SmartCardDisposition::kReset;
    case V8SmartCardDisposition::Enum::kUnpower:
      return SmartCardDisposition::kUnpower;
    case V8SmartCardDisposition::Enum::kEject:
      return SmartCardDisposition::kEject;
  }
}

absl::optional<V8SmartCardConnectionState::Enum> ToV8ConnectionState(
    SmartCardConnectionState state,
    SmartCardProtocol protocol) {
  switch (state) {
    case SmartCardConnectionState::kAbsent:
      return V8SmartCardConnectionState::Enum::kAbsent;
    case SmartCardConnectionState::kPresent:
      return V8SmartCardConnectionState::Enum::kPresent;
    case SmartCardConnectionState::kSwallowed:
      return V8SmartCardConnectionState::Enum::kSwallowed;
    case SmartCardConnectionState::kPowered:
      return V8SmartCardConnectionState::Enum::kPowered;
    case SmartCardConnectionState::kNegotiable:
      return V8SmartCardConnectionState::Enum::kNegotiable;
    case SmartCardConnectionState::kSpecific:
      switch (protocol) {
        case SmartCardProtocol::kUndefined:
          LOG(ERROR)
              << "Invalid Status result: (state=specific, protocol=undefined)";
          return absl::nullopt;
        case SmartCardProtocol::kT0:
          return V8SmartCardConnectionState::Enum::kT0;
        case SmartCardProtocol::kT1:
          return V8SmartCardConnectionState::Enum::kT1;
        case SmartCardProtocol::kRaw:
          return V8SmartCardConnectionState::Enum::kRaw;
      }
  }
}

class TransactionFulfilledFunction : public ScriptFunction::Callable {
 public:
  explicit TransactionFulfilledFunction(SmartCardConnection* connection)
      : connection_(connection) {}

  ScriptValue Call(ScriptState* script_state, ScriptValue value) override {
    ExceptionState exception_state(v8::Isolate::GetCurrent(),
                                   ExceptionState::kExecutionContext,
                                   "SmartCardConnection", "startTransaction");

    if (value.IsUndefined()) {
      connection_->OnTransactionCallbackDone(SmartCardDisposition::kLeave);
      return ScriptValue();
    }

    V8SmartCardDisposition v8_disposition =
        NativeValueTraits<V8SmartCardDisposition>::NativeValue(
            script_state->GetIsolate(), value.V8Value(), exception_state);

    if (exception_state.HadException()) {
      ScriptValue exception_value(script_state->GetIsolate(),
                                  exception_state.GetException());
      connection_->OnTransactionCallbackFailed(exception_value);
      return ScriptValue();
    }

    connection_->OnTransactionCallbackDone(ToMojomDisposition(v8_disposition));

    return ScriptValue();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(connection_);
    ScriptFunction::Callable::Trace(visitor);
  }

 private:
  Member<SmartCardConnection> connection_;
};

class TransactionRejectedFunction : public ScriptFunction::Callable {
 public:
  explicit TransactionRejectedFunction(SmartCardConnection* connection)
      : connection_(connection) {}

  ScriptValue Call(ScriptState*, ScriptValue value) override {
    connection_->OnTransactionCallbackFailed(value);
    return ScriptValue();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(connection_);
    ScriptFunction::Callable::Trace(visitor);
  }

 private:
  Member<SmartCardConnection> connection_;
};

}  // anonymous namespace

/////
// SmartCardConnection::TransactionState

class SmartCardConnection::TransactionState final
    : public GarbageCollected<TransactionState> {
 public:
  TransactionState(
      ScriptPromiseResolver* start_transaction_request,
      mojo::PendingAssociatedRemote<device::mojom::blink::SmartCardTransaction>
          pending_remote,
      ExecutionContext* execution_context);
  ~TransactionState();
  void Trace(Visitor*) const;
  void SetCallbackException(DOMExceptionCode, const String& message);
  void SetCallbackException(const ScriptValue& exception);
  void SettleStartTransaction(
      device::mojom::blink::SmartCardResultPtr end_transaction_result);
  void RejectStartTransaction(DOMExceptionCode exception_code,
                              const String& message);
  bool HasPendingEnd() const;
  void SetPendingEnd(device::mojom::blink::SmartCardDisposition);
  device::mojom::blink::SmartCardDisposition TakePendingEnd();

  ScriptPromiseResolver* EndTransaction(
      device::mojom::blink::SmartCardDisposition,
      base::OnceCallback<void(device::mojom::blink::SmartCardResultPtr)>);

 private:
  Member<ScriptPromiseResolver> start_transaction_request_;
  HeapMojoAssociatedRemote<device::mojom::blink::SmartCardTransaction>
      transaction_;
  ScriptValue callback_exception_;
  absl::optional<device::mojom::blink::SmartCardDisposition> pending_end_;
};

SmartCardConnection::TransactionState::~TransactionState() = default;

SmartCardConnection::TransactionState::TransactionState(
    ScriptPromiseResolver* start_transaction_request,
    mojo::PendingAssociatedRemote<device::mojom::blink::SmartCardTransaction>
        pending_remote,
    ExecutionContext* execution_context)
    : start_transaction_request_(start_transaction_request),
      transaction_(execution_context) {
  transaction_.Bind(std::move(pending_remote), execution_context->GetTaskRunner(
                                                   TaskType::kMiscPlatformAPI));
}

void SmartCardConnection::TransactionState::Trace(Visitor* visitor) const {
  visitor->Trace(start_transaction_request_);
  visitor->Trace(transaction_);
  visitor->Trace(callback_exception_);
}

void SmartCardConnection::TransactionState::SetCallbackException(
    DOMExceptionCode code,
    const String& message) {
  ScriptState* script_state = start_transaction_request_->GetScriptState();
  v8::Isolate* isolate = script_state->GetIsolate();

  callback_exception_ = ScriptValue::From(
      script_state, V8ThrowDOMException::CreateOrEmpty(isolate, code, message));
}

void SmartCardConnection::TransactionState::SetCallbackException(
    const ScriptValue& exception) {
  callback_exception_ = exception;
}

void SmartCardConnection::TransactionState::SettleStartTransaction(
    device::mojom::blink::SmartCardResultPtr end_transaction_result) {
  CHECK(!pending_end_);

  if (!callback_exception_.IsEmpty()) {
    start_transaction_request_->Reject(callback_exception_);
  } else if (end_transaction_result->is_error()) {
    start_transaction_request_->Reject(
        SmartCardError::Create(end_transaction_result->get_error()));
  } else {
    start_transaction_request_->Resolve();
  }
}

void SmartCardConnection::TransactionState::RejectStartTransaction(
    DOMExceptionCode exception_code,
    const String& message) {
  start_transaction_request_->RejectWithDOMException(exception_code, message);
}

bool SmartCardConnection::TransactionState::HasPendingEnd() const {
  return pending_end_.has_value();
}

void SmartCardConnection::TransactionState::SetPendingEnd(
    SmartCardDisposition disposition) {
  CHECK(!pending_end_);
  pending_end_ = disposition;
}

SmartCardDisposition SmartCardConnection::TransactionState::TakePendingEnd() {
  CHECK(pending_end_);
  SmartCardDisposition disposition = *pending_end_;
  pending_end_.reset();
  return disposition;
}

ScriptPromiseResolver* SmartCardConnection::TransactionState::EndTransaction(
    SmartCardDisposition disposition,
    base::OnceCallback<void(device::mojom::blink::SmartCardResultPtr)>
        callback) {
  CHECK(!pending_end_);
  transaction_->EndTransaction(disposition, std::move(callback));
  return start_transaction_request_.Get();
}

/////
// SmartCardConnection

SmartCardConnection::SmartCardConnection(
    mojo::PendingRemote<device::mojom::blink::SmartCardConnection>
        pending_connection,
    device::mojom::blink::SmartCardProtocol active_protocol,
    SmartCardContext* smart_card_context,
    ExecutionContext* execution_context)
    : ExecutionContextClient(execution_context),
      connection_(execution_context),
      active_protocol_(active_protocol),
      smart_card_context_(smart_card_context) {
  connection_.Bind(
      std::move(pending_connection),
      execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI));
  connection_.set_disconnect_handler(WTF::BindOnce(
      &SmartCardConnection::CloseMojoConnection, WrapWeakPersistent(this)));
}

ScriptPromise SmartCardConnection::disconnect(ScriptState* script_state,
                                              ExceptionState& exception_state) {
  return disconnect(
      script_state,
      V8SmartCardDisposition(V8SmartCardDisposition::Enum::kLeave),
      exception_state);
}

ScriptPromise SmartCardConnection::disconnect(
    ScriptState* script_state,
    const V8SmartCardDisposition& disposition,
    ExceptionState& exception_state) {
  if (!EnsureNoOperationInProgress(exception_state) ||
      !EnsureConnection(exception_state)) {
    return ScriptPromise();
  }

  ongoing_request_ = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  connection_->Disconnect(
      ToMojomDisposition(disposition),
      WTF::BindOnce(&SmartCardConnection::OnDisconnectDone,
                    WrapPersistent(this),
                    WrapPersistent(ongoing_request_.Get())));

  return ongoing_request_->Promise();
}

ScriptPromise SmartCardConnection::transmit(ScriptState* script_state,
                                            const DOMArrayPiece& send_buffer,
                                            ExceptionState& exception_state) {
  if (!EnsureNoOperationInProgress(exception_state) ||
      !EnsureConnection(exception_state)) {
    return ScriptPromise();
  }

  if (send_buffer.IsDetached() || send_buffer.IsNull()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid send buffer.");
    return ScriptPromise();
  }

  ongoing_request_ = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  Vector<uint8_t> send_vector;
  send_vector.Append(send_buffer.Bytes(),
                     static_cast<wtf_size_t>(send_buffer.ByteLength()));

  connection_->Transmit(
      active_protocol_, send_vector,
      WTF::BindOnce(&SmartCardConnection::OnDataResult, WrapPersistent(this),
                    WrapPersistent(ongoing_request_.Get())));

  return ongoing_request_->Promise();
}

ScriptPromise SmartCardConnection::status(ScriptState* script_state,
                                          ExceptionState& exception_state) {
  if (!EnsureNoOperationInProgress(exception_state) ||
      !EnsureConnection(exception_state)) {
    return ScriptPromise();
  }

  ongoing_request_ = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  connection_->Status(WTF::BindOnce(&SmartCardConnection::OnStatusDone,
                                    WrapPersistent(this),
                                    WrapPersistent(ongoing_request_.Get())));

  return ongoing_request_->Promise();
}

ScriptPromise SmartCardConnection::control(ScriptState* script_state,
                                           uint32_t control_code,
                                           const DOMArrayPiece& data,
                                           ExceptionState& exception_state) {
  if (!EnsureNoOperationInProgress(exception_state) ||
      !EnsureConnection(exception_state)) {
    return ScriptPromise();
  }

  if (data.IsDetached() || data.IsNull()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid data.");
    return ScriptPromise();
  }

  ongoing_request_ = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  Vector<uint8_t> data_vector;
  data_vector.Append(data.Bytes(), static_cast<wtf_size_t>(data.ByteLength()));

  connection_->Control(
      control_code, data_vector,
      WTF::BindOnce(&SmartCardConnection::OnDataResult, WrapPersistent(this),
                    WrapPersistent(ongoing_request_.Get())));

  return ongoing_request_->Promise();
}

ScriptPromise SmartCardConnection::getAttribute(
    ScriptState* script_state,
    uint32_t tag,
    ExceptionState& exception_state) {
  if (!EnsureNoOperationInProgress(exception_state) ||
      !EnsureConnection(exception_state)) {
    return ScriptPromise();
  }

  ongoing_request_ = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  connection_->GetAttrib(
      tag,
      WTF::BindOnce(&SmartCardConnection::OnDataResult, WrapPersistent(this),
                    WrapPersistent(ongoing_request_.Get())));

  return ongoing_request_->Promise();
}

ScriptPromise SmartCardConnection::setAttribute(
    ScriptState* script_state,
    uint32_t tag,
    const DOMArrayPiece& data,
    ExceptionState& exception_state) {
  if (!EnsureNoOperationInProgress(exception_state) ||
      !EnsureConnection(exception_state)) {
    return ScriptPromise();
  }

  if (data.IsDetached() || data.IsNull()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid data.");
    return ScriptPromise();
  }

  ongoing_request_ = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  Vector<uint8_t> data_vector;
  data_vector.Append(data.Bytes(), static_cast<wtf_size_t>(data.ByteLength()));

  connection_->SetAttrib(
      tag, data_vector,
      WTF::BindOnce(&SmartCardConnection::OnPlainResult, WrapPersistent(this),
                    WrapPersistent(ongoing_request_.Get())));

  return ongoing_request_->Promise();
}

ScriptPromise SmartCardConnection::startTransaction(
    ScriptState* script_state,
    V8SmartCardTransactionCallback* transaction_callback,
    SmartCardTransactionOptions* options,
    ExceptionState& exception_state) {
  if (!EnsureNoOperationInProgress(exception_state) ||
      !EnsureConnection(exception_state)) {
    return ScriptPromise();
  }

  if (transaction_state_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kTransactionAlreadyExists);
    return ScriptPromise();
  }

  AbortSignal* signal = options->getSignalOr(nullptr);
  if (signal && signal->aborted()) {
    return ScriptPromise::Reject(script_state, signal->reason(script_state));
  }

  ongoing_request_ = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  AbortSignal::AlgorithmHandle* abort_handle = nullptr;
  if (signal) {
    abort_handle = signal->AddAlgorithm(
        MakeGarbageCollected<SmartCardCancelAlgorithm>(smart_card_context_));
  }

  connection_->BeginTransaction(WTF::BindOnce(
      &SmartCardConnection::OnBeginTransactionDone, WrapPersistent(this),
      WrapPersistent(ongoing_request_.Get()),
      WrapPersistent(transaction_callback), WrapPersistent(signal),
      WrapPersistent(abort_handle)));

  return ongoing_request_->Promise();
}

void SmartCardConnection::OnTransactionCallbackDone(
    SmartCardDisposition disposition) {
  CHECK(transaction_state_);

  if (ongoing_request_) {
    transaction_state_->SetCallbackException(
        DOMExceptionCode::kInvalidStateError,
        kTransactionEndedWithPendingOperation);
    transaction_state_->SetPendingEnd(disposition);
  } else {
    EndTransaction(disposition);
  }
}

void SmartCardConnection::OnTransactionCallbackFailed(
    const ScriptValue& exception) {
  CHECK(transaction_state_);

  transaction_state_->SetCallbackException(exception);

  if (ongoing_request_) {
    transaction_state_->SetPendingEnd(SmartCardDisposition::kLeave);
  } else {
    EndTransaction(SmartCardDisposition::kLeave);
  }
}

void SmartCardConnection::Trace(Visitor* visitor) const {
  visitor->Trace(connection_);
  visitor->Trace(ongoing_request_);
  visitor->Trace(smart_card_context_);
  visitor->Trace(transaction_state_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

bool SmartCardConnection::EnsureNoOperationInProgress(
    ExceptionState& exception_state) const {
  if (ongoing_request_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kOperationInProgress);
    return false;
  }
  return true;
}

bool SmartCardConnection::EnsureConnection(
    ExceptionState& exception_state) const {
  if (!connection_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kDisconnected);
    return false;
  }
  return true;
}

void SmartCardConnection::OnDisconnectDone(
    ScriptPromiseResolver* resolver,
    device::mojom::blink::SmartCardResultPtr result) {
  CHECK_EQ(ongoing_request_, resolver);
  ongoing_request_ = nullptr;

  if (result->is_error()) {
    auto* error = SmartCardError::Create(result->get_error());
    resolver->Reject(error);
    return;
  }

  CHECK(connection_.is_bound());
  connection_.reset();

  resolver->Resolve();

  MaybeEndTransaction();
}

void SmartCardConnection::OnPlainResult(
    ScriptPromiseResolver* resolver,
    device::mojom::blink::SmartCardResultPtr result) {
  CHECK_EQ(ongoing_request_, resolver);
  ongoing_request_ = nullptr;

  if (result->is_error()) {
    resolver->Reject(SmartCardError::Create(result->get_error()));
    return;
  }

  resolver->Resolve();

  MaybeEndTransaction();
}

void SmartCardConnection::OnDataResult(
    ScriptPromiseResolver* resolver,
    device::mojom::blink::SmartCardDataResultPtr result) {
  CHECK_EQ(ongoing_request_, resolver);
  ongoing_request_ = nullptr;

  if (result->is_error()) {
    auto* error = SmartCardError::Create(result->get_error());
    resolver->Reject(error);
    return;
  }

  const Vector<uint8_t>& data = result->get_data();

  resolver->Resolve(DOMArrayBuffer::Create(data.data(), data.size()));

  MaybeEndTransaction();
}

void SmartCardConnection::OnStatusDone(
    ScriptPromiseResolver* resolver,
    device::mojom::blink::SmartCardStatusResultPtr result) {
  CHECK_EQ(ongoing_request_, resolver);
  ongoing_request_ = nullptr;

  if (result->is_error()) {
    resolver->Reject(SmartCardError::Create(result->get_error()));
    return;
  }

  const SmartCardStatusPtr& mojo_status = result->get_status();

  absl::optional<V8SmartCardConnectionState::Enum> connection_state =
      ToV8ConnectionState(mojo_status->state, mojo_status->protocol);

  if (!connection_state.has_value()) {
    auto* error = SmartCardError::Create(
        device::mojom::blink::SmartCardError::kInternalError);
    resolver->Reject(error);
    return;
  }

  auto* status = SmartCardConnectionStatus::Create();
  status->setReaderName(mojo_status->reader_name);
  status->setState(connection_state.value());
  if (!mojo_status->answer_to_reset.empty()) {
    status->setAnswerToReset(
        DOMArrayBuffer::Create(mojo_status->answer_to_reset.data(),
                               mojo_status->answer_to_reset.size()));
  }
  resolver->Resolve(status);

  MaybeEndTransaction();
}

void SmartCardConnection::OnBeginTransactionDone(
    ScriptPromiseResolver* resolver,
    V8SmartCardTransactionCallback* transaction_callback,
    AbortSignal* signal,
    AbortSignal::AlgorithmHandle* abort_handle,
    device::mojom::blink::SmartCardTransactionResultPtr result) {
  CHECK(!transaction_state_);
  CHECK_EQ(ongoing_request_, resolver);
  ongoing_request_ = nullptr;

  if (signal && abort_handle) {
    signal->RemoveAlgorithm(abort_handle);
  }

  if (result->is_error()) {
    if (signal && signal->aborted() &&
        result->get_error() ==
            device::mojom::blink::SmartCardError::kCancelled) {
      RejectWithAbortionReason(resolver, signal);
    } else {
      resolver->Reject(SmartCardError::Create(result->get_error()));
    }
    return;
  }

  transaction_state_ = MakeGarbageCollected<TransactionState>(
      resolver, std::move(result->get_transaction()), GetExecutionContext());

  ScriptState* script_state = resolver->GetScriptState();
  if (!IsInParallelAlgorithmRunnable(resolver->GetExecutionContext(),
                                     script_state)) {
    // Can't run the transaction callback function.
    EndTransaction(SmartCardDisposition::kLeave);
    return;
  }

  ScriptState::Scope scope(script_state);
  v8::TryCatch try_catch(script_state->GetIsolate());
  v8::Maybe<ScriptPromise> transaction_result =
      transaction_callback->Invoke(nullptr);

  if (transaction_result.IsNothing()) {
    if (try_catch.HasCaught()) {
      transaction_state_->SetCallbackException(
          ScriptValue(script_state->GetIsolate(), try_catch.Exception()));
    } else {
      // Shouldn't happen, but technically possible.
      transaction_state_->SetCallbackException(
          DOMExceptionCode::kOperationError,
          "Could not run the given transaction callback function.");
    }
    EndTransaction(SmartCardDisposition::kLeave);
    return;
  }

  ScriptPromise promise = transaction_result.FromJust();
  promise.Then(MakeGarbageCollected<ScriptFunction>(
                   script_state,
                   MakeGarbageCollected<TransactionFulfilledFunction>(this)),
               MakeGarbageCollected<ScriptFunction>(
                   script_state,
                   MakeGarbageCollected<TransactionRejectedFunction>(this)));
}

void SmartCardConnection::OnEndTransactionDone(
    device::mojom::blink::SmartCardResultPtr end_transaction_result) {
  CHECK(transaction_state_);
  ongoing_request_ = nullptr;

  transaction_state_->SettleStartTransaction(std::move(end_transaction_result));
  transaction_state_ = nullptr;
}

void SmartCardConnection::CloseMojoConnection() {
  connection_.reset();

  if (!ongoing_request_) {
    return;
  }

  ScriptState* script_state = ongoing_request_->GetScriptState();
  if (IsInParallelAlgorithmRunnable(ongoing_request_->GetExecutionContext(),
                                    script_state)) {
    ScriptState::Scope script_state_scope(script_state);
    ongoing_request_->RejectWithDOMException(
        DOMExceptionCode::kInvalidStateError, kDisconnected);
  }
  ongoing_request_ = nullptr;
}

void SmartCardConnection::EndTransaction(SmartCardDisposition disposition) {
  CHECK(!ongoing_request_);
  CHECK(transaction_state_);

  if (!connection_.is_bound()) {
    transaction_state_->RejectStartTransaction(
        DOMExceptionCode::kInvalidStateError,
        "Cannot end transaction with an invalid connection.");
    transaction_state_ = nullptr;
    return;
  }

  ongoing_request_ = transaction_state_->EndTransaction(
      disposition, WTF::BindOnce(&SmartCardConnection::OnEndTransactionDone,
                                 WrapPersistent(this)));
}

void SmartCardConnection::MaybeEndTransaction() {
  if (!transaction_state_ || !transaction_state_->HasPendingEnd()) {
    return;
  }

  EndTransaction(transaction_state_->TakePendingEnd());
}

}  // namespace blink
