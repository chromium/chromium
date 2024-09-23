// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/smart_card/smart_card_context.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_connect_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_connect_result.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_get_status_change_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_reader_state_flags_in.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_reader_state_flags_out.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_reader_state_in.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_reader_state_out.h"
#include "third_party/blink/renderer/modules/smart_card/smart_card_cancel_algorithm.h"
#include "third_party/blink/renderer/modules/smart_card/smart_card_connection.h"
#include "third_party/blink/renderer/modules/smart_card/smart_card_error.h"
#include "third_party/blink/renderer/modules/smart_card/smart_card_util.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {
namespace {
constexpr char kContextUnavailable[] = "Context unavailable.";
constexpr char kContextBusy[] =
    "An operation is already in progress in this smart card context.";

device::mojom::blink::SmartCardReaderStateFlagsPtr ToMojomStateFlags(
    const SmartCardReaderStateFlagsIn& flags) {
  auto mojom_flags = device::mojom::blink::SmartCardReaderStateFlags::New();
  mojom_flags->unaware = flags.unaware();
  mojom_flags->ignore = flags.ignore();
  mojom_flags->unavailable = flags.unavailable();
  mojom_flags->empty = flags.empty();
  mojom_flags->present = flags.present();
  mojom_flags->exclusive = flags.exclusive();
  mojom_flags->inuse = flags.inuse();
  mojom_flags->mute = flags.mute();
  mojom_flags->unpowered = flags.unpowered();
  return mojom_flags;
}

Vector<device::mojom::blink::SmartCardReaderStateInPtr> ToMojomReaderStatesIn(
    const HeapVector<Member<SmartCardReaderStateIn>>& reader_states) {
  Vector<device::mojom::blink::SmartCardReaderStateInPtr> mojom_reader_states;
  mojom_reader_states.reserve(reader_states.size());

  for (const Member<SmartCardReaderStateIn>& state_in : reader_states) {
    mojom_reader_states.push_back(
        device::mojom::blink::SmartCardReaderStateIn::New(
            state_in->readerName(),
            ToMojomStateFlags(*state_in->currentState()),
            state_in->getCurrentCountOr(0)));
  }

  return mojom_reader_states;
}

SmartCardReaderStateFlagsOut* ToV8ReaderStateFlagsOut(
    const device::mojom::blink::SmartCardReaderStateFlags& mojom_state_flags) {
  auto* state_flags = SmartCardReaderStateFlagsOut::Create();
  state_flags->setIgnore(mojom_state_flags.ignore);
  state_flags->setChanged(mojom_state_flags.changed);
  state_flags->setUnknown(mojom_state_flags.unknown);
  state_flags->setUnavailable(mojom_state_flags.unavailable);
  state_flags->setEmpty(mojom_state_flags.empty);
  state_flags->setPresent(mojom_state_flags.present);
  state_flags->setExclusive(mojom_state_flags.exclusive);
  state_flags->setInuse(mojom_state_flags.inuse);
  state_flags->setMute(mojom_state_flags.mute);
  state_flags->setUnpowered(mojom_state_flags.unpowered);
  return state_flags;
}

HeapVector<Member<SmartCardReaderStateOut>> ToV8ReaderStatesOut(
    Vector<device::mojom::blink::SmartCardReaderStateOutPtr>&
        mojom_reader_states) {
  HeapVector<Member<SmartCardReaderStateOut>> reader_states;
  reader_states.reserve(mojom_reader_states.size());

  for (auto& mojom_state_out : mojom_reader_states) {
    auto* state_out = SmartCardReaderStateOut::Create();
    state_out->setReaderName(mojom_state_out->reader);
    state_out->setEventState(
        ToV8ReaderStateFlagsOut(*mojom_state_out->event_state));
    state_out->setEventCount(mojom_state_out->event_count);
    state_out->setAnswerToReset(
        DOMArrayBuffer::Create(mojom_state_out->answer_to_reset));
    reader_states.push_back(state_out);
  }

  return reader_states;
}

}  // anonymous namespace

SmartCardContext::SmartCardContext(
    mojo::PendingRemote<device::mojom::blink::SmartCardContext> pending_context,
    ExecutionContext* execution_context)
    : ExecutionContextClient(execution_context),
      scard_context_(execution_context),
      feature_handle_for_scheduler_(
          execution_context->GetScheduler()->RegisterFeature(
              SchedulingPolicy::Feature::kSmartCard,
              SchedulingPolicy{SchedulingPolicy::DisableBackForwardCache()})) {
  scard_context_.Bind(
      std::move(pending_context),
      execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI));
  scard_context_.set_disconnect_handler(WTF::BindOnce(
      &SmartCardContext::CloseMojoConnection, WrapWeakPersistent(this)));
}

ScriptPromise<IDLSequence<IDLString>> SmartCardContext::listReaders(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!EnsureMojoConnection(exception_state) ||
      !EnsureNoOperationInProgress(exception_state)) {
    return ScriptPromise<IDLSequence<IDLString>>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLSequence<IDLString>>>(
          script_state, exception_state.GetContext());

  SetOperationInProgress(resolver);
  scard_context_->ListReaders(
      WTF::BindOnce(&SmartCardContext::OnListReadersDone, WrapPersistent(this),
                    WrapPersistent(resolver)));

  return resolver->Promise();
}

ScriptPromise<IDLSequence<SmartCardReaderStateOut>>
SmartCardContext::getStatusChange(
    ScriptState* script_state,
    const HeapVector<Member<SmartCardReaderStateIn>>& reader_states,
    SmartCardGetStatusChangeOptions* options,
    ExceptionState& exception_state) {
  if (!EnsureMojoConnection(exception_state) ||
      !EnsureNoOperationInProgress(exception_state)) {
    return ScriptPromise<IDLSequence<SmartCardReaderStateOut>>();
  }

  AbortSignal* signal = options->getSignalOr(nullptr);
  if (signal && signal->aborted()) {
    return ScriptPromise<IDLSequence<SmartCardReaderStateOut>>::Reject(
        script_state, signal->reason(script_state));
  }

  base::TimeDelta timeout = base::TimeDelta::Max();
  if (options->hasTimeout()) {
    timeout = base::Milliseconds(options->timeout());
  }

  AbortSignal::AlgorithmHandle* abort_handle = nullptr;
  if (signal) {
    abort_handle = signal->AddAlgorithm(
        MakeGarbageCollected<SmartCardCancelAlgorithm>(this));
  }

  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<IDLSequence<SmartCardReaderStateOut>>>(
      script_state, exception_state.GetContext());

  SetOperationInProgress(resolver);
  scard_context_->GetStatusChange(
      timeout, ToMojomReaderStatesIn(reader_states),
      WTF::BindOnce(&SmartCardContext::OnGetStatusChangeDone,
                    WrapPersistent(this), WrapPersistent(resolver),
                    WrapPersistent(signal), WrapPersistent(abort_handle)));

  return resolver->Promise();
}

ScriptPromise<SmartCardConnectResult> SmartCardContext::connect(
    ScriptState* script_state,
    const String& reader_name,
    V8SmartCardAccessMode access_mode,
    SmartCardConnectOptions* options,
    ExceptionState& exception_state) {
  if (!EnsureMojoConnection(exception_state) ||
      !EnsureNoOperationInProgress(exception_state)) {
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<SmartCardConnectResult>>(
          script_state, exception_state.GetContext());

  Vector<V8SmartCardProtocol> preferred_protocols =
      options->getPreferredProtocolsOr(Vector<V8SmartCardProtocol>());

  SetOperationInProgress(resolver);
  scard_context_->Connect(
      reader_name, ToMojoSmartCardShareMode(access_mode),
      ToMojoSmartCardProtocols(preferred_protocols),
      WTF::BindOnce(&SmartCardContext::OnConnectDone, WrapPersistent(this),
                    WrapPersistent(resolver)));

  return resolver->Promise();
}

void SmartCardContext::Trace(Visitor* visitor) const {
  visitor->Trace(scard_context_);
  visitor->Trace(request_);
  visitor->Trace(connections_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

void SmartCardContext::Cancel() {
  if (!scard_context_.is_bound()) {
    return;
  }
  scard_context_->Cancel(
      WTF::BindOnce(&SmartCardContext::OnCancelDone, WrapPersistent(this)));
}

bool SmartCardContext::EnsureNoOperationInProgress(
    ExceptionState& exception_state) const {
  if (request_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kContextBusy);
    return false;
  }
  return true;
}

void SmartCardContext::SetConnectionOperationInProgress(
    ScriptPromiseResolverBase* resolver) {
  SetOperationInProgress(resolver);
  is_connection_request_ = true;
}

void SmartCardContext::SetOperationInProgress(
    ScriptPromiseResolverBase* resolver) {
  if (request_ == resolver) {
    // NOOP
    return;
  }

  CHECK_EQ(request_, nullptr);
  CHECK(!is_connection_request_);

  request_ = resolver;
}

void SmartCardContext::ClearConnectionOperationInProgress(
    ScriptPromiseResolverBase* resolver) {
  CHECK(is_connection_request_);
  is_connection_request_ = false;
  ClearOperationInProgress(resolver);
}

void SmartCardContext::ClearOperationInProgress(
    ScriptPromiseResolverBase* resolver) {
  CHECK_EQ(request_, resolver);
  CHECK(!is_connection_request_);
  request_ = nullptr;

  for (auto& connection : connections_) {
    connection->OnOperationInProgressCleared();
    // If that connection started a new operation, refrain from notifying the
    // others.
    if (request_) {
      break;
    }
  }
}

bool SmartCardContext::IsOperationInProgress() const {
  return request_ != nullptr;
}

void SmartCardContext::CloseMojoConnection() {
  scard_context_.reset();

  if (!request_ || is_connection_request_) {
    return;
  }

  ScriptState* script_state = request_->GetScriptState();
  if (!IsInParallelAlgorithmRunnable(request_->GetExecutionContext(),
                                     script_state)) {
    return;
  }

  ScriptState::Scope script_state_scope(script_state);
  request_->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                   kContextUnavailable);

  ClearOperationInProgress(request_);
}

bool SmartCardContext::EnsureMojoConnection(
    ExceptionState& exception_state) const {
  if (!scard_context_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kContextUnavailable);
    return false;
  }
  return true;
}

void SmartCardContext::OnListReadersDone(
    ScriptPromiseResolver<IDLSequence<IDLString>>* resolver,
    device::mojom::blink::SmartCardListReadersResultPtr result) {
  ClearOperationInProgress(resolver);

  if (result->is_error()) {
    auto mojom_error = result->get_error();
    // If there are no readers available, PCSC API returns a kNoReadersAvailable
    // error. In web API we want to return an empty list of readers instead.
    if (mojom_error ==
        device::mojom::blink::SmartCardError::kNoReadersAvailable) {
      resolver->Resolve(Vector<String>());
      return;
    }

    SmartCardError::MaybeReject(resolver, mojom_error);
    return;
  }

  resolver->Resolve(std::move(result->get_readers()));
}

void SmartCardContext::OnGetStatusChangeDone(
    ScriptPromiseResolver<IDLSequence<SmartCardReaderStateOut>>* resolver,
    AbortSignal* signal,
    AbortSignal::AlgorithmHandle* abort_handle,
    device::mojom::blink::SmartCardStatusChangeResultPtr result) {
  ClearOperationInProgress(resolver);

  if (signal && abort_handle) {
    signal->RemoveAlgorithm(abort_handle);
  }

  if (result->is_error()) {
    if (signal && signal->aborted() &&
        result->get_error() ==
            device::mojom::blink::SmartCardError::kCancelled) {
      RejectWithAbortionReason(resolver, signal);
    } else {
      SmartCardError::MaybeReject(resolver, result->get_error());
    }
    return;
  }

  resolver->Resolve(ToV8ReaderStatesOut(result->get_reader_states()));
}

void SmartCardContext::OnCancelDone(
    device::mojom::blink::SmartCardResultPtr result) {
  if (result->is_error()) {
    LOG(WARNING) << "Cancel operation failed: " << result->get_error();
  }
}

void SmartCardContext::OnConnectDone(
    ScriptPromiseResolver<SmartCardConnectResult>* resolver,
    device::mojom::blink::SmartCardConnectResultPtr result) {
  ClearOperationInProgress(resolver);

  if (result->is_error()) {
    SmartCardError::MaybeReject(resolver, result->get_error());
    return;
  }

  device::mojom::blink::SmartCardConnectSuccessPtr& success =
      result->get_success();

  auto* connection = MakeGarbageCollected<SmartCardConnection>(
      std::move(success->connection), success->active_protocol, this,
      GetExecutionContext());
  // Being a weak member, it will be automatically removed from the set when
  // garbage-collected.
  connections_.insert(connection);

  auto* blink_result = SmartCardConnectResult::Create();
  blink_result->setConnection(connection);

  switch (success->active_protocol) {
    case device::mojom::blink::SmartCardProtocol::kUndefined:
      // NOOP: Do not set an activeProtocol.
      break;
    case device::mojom::blink::SmartCardProtocol::kT0:
      blink_result->setActiveProtocol(V8SmartCardProtocol::Enum::kT0);
      break;
    case device::mojom::blink::SmartCardProtocol::kT1:
      blink_result->setActiveProtocol(V8SmartCardProtocol::Enum::kT1);
      break;
    case device::mojom::blink::SmartCardProtocol::kRaw:
      blink_result->setActiveProtocol(V8SmartCardProtocol::Enum::kRaw);
      break;
  }

  resolver->Resolve(blink_result);
}

}  // namespace blink
