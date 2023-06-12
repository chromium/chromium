// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/smart_card/smart_card_context.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_reader_state_flags.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_reader_state_in.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_reader_state_out.h"
#include "third_party/blink/renderer/modules/smart_card/smart_card_connection.h"
#include "third_party/blink/renderer/modules/smart_card/smart_card_error.h"
#include "third_party/blink/renderer/modules/smart_card/smart_card_util.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {
namespace {
constexpr char kContextUnavailable[] = "Context unavailable.";

device::mojom::blink::SmartCardReaderStateFlagsPtr ToMojomStateFlags(
    const SmartCardReaderStateFlags& flags) {
  auto mojom_flags = device::mojom::blink::SmartCardReaderStateFlags::New();
  mojom_flags->unaware = flags.unaware();
  mojom_flags->ignore = flags.ignore();
  mojom_flags->changed = flags.changed();
  mojom_flags->unknown = flags.unknown();
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
            ToMojomStateFlags(*state_in->currentState())));
  }

  return mojom_reader_states;
}

SmartCardReaderStateFlags* ToV8ReaderStateFlags(
    const device::mojom::blink::SmartCardReaderStateFlags& mojom_state_flags) {
  auto* state_flags = SmartCardReaderStateFlags::Create();
  state_flags->setUnaware(mojom_state_flags.unaware);
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
        ToV8ReaderStateFlags(*mojom_state_out->event_state));
    state_out->setAnswerToReset(
        DOMArrayBuffer::Create(mojom_state_out->answer_to_reset.data(),
                               mojom_state_out->answer_to_reset.size()));
    reader_states.push_back(state_out);
  }

  return reader_states;
}

void RejectWithAbortionReason(ScriptPromiseResolver* resolver,
                              AbortSignal* signal) {
  CHECK(signal->aborted());

  ScriptState* script_state = resolver->GetScriptState();
  if (!IsInParallelAlgorithmRunnable(resolver->GetExecutionContext(),
                                     script_state)) {
    return;
  }

  ScriptState::Scope script_state_scope(script_state);
  resolver->Reject(signal->reason(script_state));
}

}  // anonymous namespace

class SmartCardContext::GetStatusChangeAbortAlgorithm final
    : public AbortSignal::Algorithm {
 public:
  explicit GetStatusChangeAbortAlgorithm(SmartCardContext* blink_scard_context)
      : blink_scard_context_(blink_scard_context) {}
  ~GetStatusChangeAbortAlgorithm() override = default;

  void Run() override { blink_scard_context_->AbortGetStatusChange(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(blink_scard_context_);
    Algorithm::Trace(visitor);
  }

 private:
  Member<SmartCardContext> blink_scard_context_;
};

SmartCardContext::SmartCardContext(
    mojo::PendingRemote<device::mojom::blink::SmartCardContext> pending_context,
    ExecutionContext* execution_context)
    : ExecutionContextClient(execution_context),
      scard_context_(execution_context) {
  scard_context_.Bind(
      std::move(pending_context),
      execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI));
  scard_context_.set_disconnect_handler(WTF::BindOnce(
      &SmartCardContext::CloseMojoConnection, WrapWeakPersistent(this)));
}

ScriptPromise SmartCardContext::listReaders(ScriptState* script_state,
                                            ExceptionState& exception_state) {
  if (!EnsureMojoConnection(exception_state) ||
      !EnsureNoOperationInProgress(exception_state)) {
    return ScriptPromise();
  }

  ScriptPromiseResolver* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  list_readers_request_ = resolver;
  scard_context_->ListReaders(
      WTF::BindOnce(&SmartCardContext::OnListReadersDone, WrapPersistent(this),
                    WrapPersistent(resolver)));

  return resolver->Promise();
}

ScriptPromise SmartCardContext::getStatusChange(
    ScriptState* script_state,
    const HeapVector<Member<SmartCardReaderStateIn>>& reader_states,
    AbortSignal* signal,
    ExceptionState& exception_state) {
  if (!EnsureMojoConnection(exception_state) ||
      !EnsureNoOperationInProgress(exception_state)) {
    return ScriptPromise();
  }

  if (signal->aborted()) {
    return ScriptPromise::Reject(
        script_state, get_status_change_abort_signal_->reason(script_state));
  }
  CHECK(!get_status_change_abort_signal_);
  CHECK(!get_status_change_abort_handle_);
  get_status_change_abort_signal_ = signal;
  get_status_change_abort_handle_ =
      get_status_change_abort_signal_->AddAlgorithm(
          MakeGarbageCollected<GetStatusChangeAbortAlgorithm>(this));

  ScriptPromiseResolver* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  get_status_change_request_ = resolver;
  scard_context_->GetStatusChange(
      base::TimeDelta::Max(), ToMojomReaderStatesIn(reader_states),
      WTF::BindOnce(&SmartCardContext::OnGetStatusChangeDone,
                    WrapPersistent(this), WrapPersistent(resolver)));

  return resolver->Promise();
}

ScriptPromise SmartCardContext::connect(
    ScriptState* script_state,
    const String& reader_name,
    V8SmartCardAccessMode access_mode,
    const Vector<V8SmartCardProtocol>& preferred_protocols,
    ExceptionState& exception_state) {
  if (!EnsureMojoConnection(exception_state) ||
      !EnsureNoOperationInProgress(exception_state)) {
    return ScriptPromise();
  }

  ScriptPromiseResolver* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  connect_request_ = resolver;
  scard_context_->Connect(
      reader_name, ToMojoSmartCardShareMode(access_mode),
      ToMojoSmartCardProtocols(preferred_protocols),
      WTF::BindOnce(&SmartCardContext::OnConnectDone, WrapPersistent(this),
                    WrapPersistent(resolver)));

  return resolver->Promise();
}

ScriptPromise SmartCardContext::connect(ScriptState* script_state,
                                        const String& reader_name,
                                        V8SmartCardAccessMode access_mode,
                                        ExceptionState& exception_state) {
  return connect(script_state, reader_name, access_mode,
                 Vector<V8SmartCardProtocol>(), exception_state);
}

void SmartCardContext::Trace(Visitor* visitor) const {
  visitor->Trace(scard_context_);
  visitor->Trace(list_readers_request_);
  visitor->Trace(connect_request_);
  visitor->Trace(get_status_change_request_);
  visitor->Trace(get_status_change_abort_signal_);
  visitor->Trace(get_status_change_abort_handle_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

void SmartCardContext::CloseMojoConnection() {
  scard_context_.reset();

  auto reject = [](ScriptPromiseResolver* resolver) {
    if (!resolver) {
      return;
    }
    ScriptState* script_state = resolver->GetScriptState();
    if (!IsInParallelAlgorithmRunnable(resolver->GetExecutionContext(),
                                       script_state)) {
      return;
    }
    ScriptState::Scope script_state_scope(script_state);
    resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                     kContextUnavailable);
  };

  reject(list_readers_request_.Release());
  reject(connect_request_.Release());

  ResetAbortSignal();
  reject(get_status_change_request_.Release());
}

void SmartCardContext::ResetAbortSignal() {
  if (get_status_change_abort_handle_) {
    CHECK(get_status_change_abort_signal_);
    get_status_change_abort_signal_->RemoveAlgorithm(
        get_status_change_abort_handle_);
    get_status_change_abort_handle_ = nullptr;
  }
  get_status_change_abort_signal_ = nullptr;
}

bool SmartCardContext::EnsureNoOperationInProgress(
    ExceptionState& exception_state) const {
  if (list_readers_request_ || connect_request_ || get_status_change_request_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "An operation is in progress.");
    return false;
  }
  return true;
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
    ScriptPromiseResolver* resolver,
    device::mojom::blink::SmartCardListReadersResultPtr result) {
  CHECK_EQ(list_readers_request_, resolver);
  list_readers_request_ = nullptr;

  if (result->is_error()) {
    auto mojom_error = result->get_error();
    // If there are no readers available, PCSC API returns a kNoReadersAvailable
    // error. In web API we want to return an empty list of readers instead.
    if (mojom_error ==
        device::mojom::blink::SmartCardError::kNoReadersAvailable) {
      resolver->Resolve(Vector<String>());
      return;
    }

    auto* error = SmartCardError::Create(mojom_error);
    resolver->Reject(error);
    return;
  }

  resolver->Resolve(std::move(result->get_readers()));
}

void SmartCardContext::OnGetStatusChangeDone(
    ScriptPromiseResolver* resolver,
    device::mojom::blink::SmartCardStatusChangeResultPtr result) {
  CHECK(get_status_change_abort_signal_);
  CHECK_EQ(get_status_change_request_, resolver);
  get_status_change_request_ = nullptr;

  if (result->is_error()) {
    if (get_status_change_abort_signal_->aborted() &&
        result->get_error() ==
            device::mojom::blink::SmartCardError::kCancelled) {
      CHECK(!get_status_change_abort_handle_);
      RejectWithAbortionReason(resolver, get_status_change_abort_signal_);
    } else {
      resolver->Reject(SmartCardError::Create(result->get_error()));
    }
    ResetAbortSignal();
    return;
  }

  ResetAbortSignal();

  resolver->Resolve(ToV8ReaderStatesOut(result->get_reader_states()));
}

void SmartCardContext::OnCancelDone(
    device::mojom::blink::SmartCardResultPtr result) {
  if (result->is_error()) {
    LOG(WARNING) << "Cancel operation failed: " << result->get_error();
  }
}

void SmartCardContext::OnConnectDone(
    ScriptPromiseResolver* resolver,
    device::mojom::blink::SmartCardConnectResultPtr result) {
  CHECK_EQ(connect_request_, resolver);
  connect_request_ = nullptr;

  if (result->is_error()) {
    auto* error = SmartCardError::Create(result->get_error());
    resolver->Reject(error);
    return;
  }

  device::mojom::blink::SmartCardConnectSuccessPtr& success =
      result->get_success();

  auto* connection = MakeGarbageCollected<SmartCardConnection>(
      std::move(success->connection), success->active_protocol,
      GetExecutionContext());

  resolver->Resolve(connection);
}

void SmartCardContext::AbortGetStatusChange() {
  CHECK(get_status_change_abort_signal_);
  CHECK(get_status_change_abort_handle_);
  // Aborting shouldn't be possible if there's no ongoing getStatusChange()
  // request in the first place.
  CHECK(get_status_change_request_);

  // You can only abort once.
  get_status_change_abort_signal_->RemoveAlgorithm(
      get_status_change_abort_handle_);
  get_status_change_abort_handle_ = nullptr;

  scard_context_->Cancel(
      WTF::BindOnce(&SmartCardContext::OnCancelDone, WrapPersistent(this)));
}

}  // namespace blink
