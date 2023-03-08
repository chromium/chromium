// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/ndef_reader.h"

#include <memory>

#include "services/device/public/mojom/nfc.mojom-blink.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ndef_make_read_only_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ndef_scan_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ndef_write_options.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/scoped_abort_state.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/nfc/ndef_message.h"
#include "third_party/blink/renderer/modules/nfc/ndef_reading_event.h"
#include "third_party/blink/renderer/modules/nfc/nfc_proxy.h"
#include "third_party/blink/renderer/modules/nfc/nfc_type_converters.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/scheduling_policy.h"

namespace blink {

using mojom::blink::PermissionName;
using mojom::blink::PermissionService;
using mojom::blink::PermissionStatus;

namespace {

v8::Local<v8::Value> NDEFErrorTypeToDOMException(
    v8::Isolate* isolate,
    device::mojom::blink::NDEFErrorType error_type,
    const String& error_message) {
  switch (error_type) {
    case device::mojom::blink::NDEFErrorType::NOT_ALLOWED:
      return V8ThrowDOMException::CreateOrDie(
          isolate, DOMExceptionCode::kNotAllowedError, error_message);
    case device::mojom::blink::NDEFErrorType::NOT_SUPPORTED:
      return V8ThrowDOMException::CreateOrDie(
          isolate, DOMExceptionCode::kNotSupportedError, error_message);
    case device::mojom::blink::NDEFErrorType::NOT_READABLE:
      return V8ThrowDOMException::CreateOrDie(
          isolate, DOMExceptionCode::kNotReadableError, error_message);
    case device::mojom::blink::NDEFErrorType::INVALID_MESSAGE:
      return V8ThrowDOMException::CreateOrDie(
          isolate, DOMExceptionCode::kSyntaxError, error_message);
    case device::mojom::blink::NDEFErrorType::OPERATION_CANCELLED:
      return V8ThrowDOMException::CreateOrDie(
          isolate, DOMExceptionCode::kAbortError, error_message);
    case device::mojom::blink::NDEFErrorType::IO_ERROR:
      return V8ThrowDOMException::CreateOrDie(
          isolate, DOMExceptionCode::kNetworkError, error_message);
  }
  NOTREACHED();
  // Don't need to handle the case after a NOTREACHED().
  return v8::Local<v8::Value>();
}

v8::Local<v8::Value> NDEFErrorPtrToDOMException(
    v8::Isolate* isolate,
    device::mojom::blink::NDEFErrorPtr error) {
  return NDEFErrorTypeToDOMException(isolate, error->error_type,
                                     error->error_message);
}

constexpr char kNotSupportedOrPermissionDenied[] =
    "Web NFC is unavailable or permission denied.";

constexpr char kChildFrameErrorMessage[] =
    "Web NFC can only be accessed in a top-level browsing context.";

}  // namespace

class NDEFReader::ReadAbortAlgorithm final : public AbortSignal::Algorithm {
 public:
  ReadAbortAlgorithm(NDEFReader* ndef_reader, AbortSignal* signal)
      : ndef_reader_(ndef_reader), abort_signal_(signal) {}
  ~ReadAbortAlgorithm() override = default;

  void Run() override { ndef_reader_->ReadAbort(abort_signal_); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(ndef_reader_);
    visitor->Trace(abort_signal_);
    Algorithm::Trace(visitor);
  }

 private:
  Member<NDEFReader> ndef_reader_;
  Member<AbortSignal> abort_signal_;
};

class NDEFReader::WriteAbortAlgorithm final : public AbortSignal::Algorithm {
 public:
  WriteAbortAlgorithm(NDEFReader* ndef_reader, AbortSignal* signal)
      : ndef_reader_(ndef_reader), abort_signal_(signal) {}
  ~WriteAbortAlgorithm() override = default;

  void Run() override { ndef_reader_->WriteAbort(abort_signal_); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(ndef_reader_);
    visitor->Trace(abort_signal_);
    Algorithm::Trace(visitor);
  }

 private:
  Member<NDEFReader> ndef_reader_;
  Member<AbortSignal> abort_signal_;
};

class NDEFReader::MakeReadOnlyAbortAlgorithm final
    : public AbortSignal::Algorithm {
 public:
  MakeReadOnlyAbortAlgorithm(NDEFReader* ndef_reader, AbortSignal* signal)
      : ndef_reader_(ndef_reader), abort_signal_(signal) {}
  ~MakeReadOnlyAbortAlgorithm() override = default;

  void Run() override { ndef_reader_->MakeReadOnlyAbort(abort_signal_); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(ndef_reader_);
    visitor->Trace(abort_signal_);
    Algorithm::Trace(visitor);
  }

 private:
  Member<NDEFReader> ndef_reader_;
  Member<AbortSignal> abort_signal_;
};

// static
NDEFReader* NDEFReader::Create(ExecutionContext* context) {
  context->GetScheduler()->RegisterStickyFeature(
      SchedulingPolicy::Feature::kWebNfc,
      {SchedulingPolicy::DisableBackForwardCache()});
  return MakeGarbageCollected<NDEFReader>(context);
}

NDEFReader::NDEFReader(ExecutionContext* context)
    : ExecutionContextLifecycleObserver(context), permission_service_(context) {
  // Call GetNFCProxy to create a proxy. This guarantees no allocation will
  // be needed when calling HasPendingActivity later during gc tracing.
  GetNfcProxy();
}

NDEFReader::~NDEFReader() = default;

const AtomicString& NDEFReader::InterfaceName() const {
  return event_target_names::kNDEFReader;
}

ExecutionContext* NDEFReader::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

bool NDEFReader::HasPendingActivity() const {
  return GetExecutionContext() && GetNfcProxy()->IsReading(this) &&
         HasEventListeners();
}

// https://w3c.github.io/web-nfc/#the-scan-method
ScriptPromise NDEFReader::scan(ScriptState* script_state,
                               const NDEFScanOptions* options,
                               ExceptionState& exception_state) {
  // https://w3c.github.io/web-nfc/#security-policies
  // WebNFC API must be only accessible from top level browsing context.
  if (!DomWindow() || !DomWindow()->GetFrame()->IsMainFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kChildFrameErrorMessage);
    return ScriptPromise();
  }

  if (scan_signal_ && scan_abort_handle_) {
    scan_signal_->RemoveAlgorithm(scan_abort_handle_);
    scan_abort_handle_.Clear();
  }
  scan_signal_ = options->getSignalOr(nullptr);
  if (scan_signal_) {
    if (scan_signal_->aborted()) {
      return ScriptPromise::Reject(script_state,
                                   scan_signal_->reason(script_state));
    }
    scan_abort_handle_ = scan_signal_->AddAlgorithm(
        MakeGarbageCollected<ReadAbortAlgorithm>(this, scan_signal_));
  }

  // Reject promise when there's already an ongoing scan.
  if (scan_resolver_ || GetNfcProxy()->IsReading(this)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "A scan() operation is ongoing.");
    return ScriptPromise();
  }

  scan_resolver_ = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  GetPermissionService()->RequestPermission(
      CreatePermissionDescriptor(PermissionName::NFC),
      LocalFrame::HasTransientUserActivation(DomWindow()->GetFrame()),
      WTF::BindOnce(&NDEFReader::ReadOnRequestPermission, WrapPersistent(this),
                    WrapPersistent(options)));
  return scan_resolver_->Promise();
}

void NDEFReader::ReadOnRequestPermission(const NDEFScanOptions* options,
                                         PermissionStatus status) {
  if (!scan_resolver_)
    return;

  ScriptState* script_state = scan_resolver_->GetScriptState();

  if (!IsInParallelAlgorithmRunnable(scan_resolver_->GetExecutionContext(),
                                     script_state)) {
    scan_resolver_.Clear();
    return;
  }

  ScriptState::Scope script_state_scope(script_state);

  if (status != PermissionStatus::GRANTED) {
    scan_resolver_->Reject(V8ThrowDOMException::CreateOrDie(
        script_state->GetIsolate(), DOMExceptionCode::kNotAllowedError,
        "NFC permission request denied."));
    scan_resolver_.Clear();
    return;
  }

  DCHECK(!scan_signal_ || !scan_signal_->aborted());

  GetNfcProxy()->StartReading(
      this,
      WTF::BindOnce(&NDEFReader::ReadOnRequestCompleted, WrapPersistent(this)));
}

void NDEFReader::ReadOnRequestCompleted(
    device::mojom::blink::NDEFErrorPtr error) {
  if (!scan_resolver_)
    return;

  ScriptState* script_state = scan_resolver_->GetScriptState();

  if (!IsInParallelAlgorithmRunnable(scan_resolver_->GetExecutionContext(),
                                     script_state)) {
    scan_resolver_.Clear();
    return;
  }

  ScriptState::Scope script_state_scope(script_state);

  if (error) {
    scan_resolver_->Reject(NDEFErrorPtrToDOMException(
        script_state->GetIsolate(), std::move(error)));
  } else {
    scan_resolver_->Resolve();
  }

  scan_resolver_.Clear();
}

void NDEFReader::OnReading(const String& serial_number,
                           const device::mojom::blink::NDEFMessage& message) {
  DCHECK(GetNfcProxy()->IsReading(this));
  DispatchEvent(*MakeGarbageCollected<NDEFReadingEvent>(
      event_type_names::kReading, serial_number,
      MakeGarbageCollected<NDEFMessage>(message)));
}

void NDEFReader::OnReadingError(const String& message) {
  GetExecutionContext()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kJavaScript,
      mojom::blink::ConsoleMessageLevel::kInfo, message));

  // Dispatch the event as the final step in this method as it may cause script
  // to run that destroys the execution context.
  DispatchEvent(*Event::Create(event_type_names::kReadingerror));
}

void NDEFReader::ContextDestroyed() {
  GetNfcProxy()->StopReading(this);
  scan_abort_handle_.Clear();
}

void NDEFReader::ReadAbort(AbortSignal* signal) {
  if (!base::FeatureList::IsEnabled(features::kAbortSignalHandleBasedRemoval)) {
    // There is no RemoveAlgorithm() method on AbortSignal so compare the signal
    // bound to this callback to the one last passed to scan().
    if (scan_signal_ != signal)
      return;
  }

  GetNfcProxy()->StopReading(this);
  scan_abort_handle_.Clear();

  if (!scan_resolver_)
    return;

  ScriptState* script_state = scan_resolver_->GetScriptState();

  if (!IsInParallelAlgorithmRunnable(scan_resolver_->GetExecutionContext(),
                                     script_state)) {
    scan_resolver_.Clear();
    return;
  }

  ScriptState::Scope script_state_scope(script_state);

  scan_resolver_->Reject(scan_signal_->reason(script_state));
  scan_resolver_.Clear();
}

// https://w3c.github.io/web-nfc/#writing-content
// https://w3c.github.io/web-nfc/#the-write-method
ScriptPromise NDEFReader::write(ScriptState* script_state,
                                const V8NDEFMessageSource* write_message,
                                const NDEFWriteOptions* options,
                                ExceptionState& exception_state) {
  // https://w3c.github.io/web-nfc/#security-policies
  // WebNFC API must be only accessible from top level browsing context.
  if (!DomWindow() || !DomWindow()->GetFrame()->IsMainFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kChildFrameErrorMessage);
    return ScriptPromise();
  }

  write_signal_ = options->getSignalOr(nullptr);
  std::unique_ptr<ScopedAbortState> scoped_abort_state = nullptr;
  if (write_signal_) {
    if (write_signal_->aborted()) {
      return ScriptPromise::Reject(script_state,
                                   write_signal_->reason(script_state));
    }
    auto* handle = write_signal_->AddAlgorithm(
        MakeGarbageCollected<WriteAbortAlgorithm>(this, write_signal_));
    scoped_abort_state =
        std::make_unique<ScopedAbortState>(write_signal_, handle);
  }

  // Step 11.2: Run "create NDEF message", if this throws an exception,
  // reject p with that exception and abort these steps.
  NDEFMessage* ndef_message =
      NDEFMessage::Create(script_state, write_message, exception_state);
  if (exception_state.HadException()) {
    return ScriptPromise();
  }

  auto message = device::mojom::blink::NDEFMessage::From(ndef_message);
  DCHECK(message);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  write_requests_.insert(resolver);

  // Add the writer to proxy's writer list for Mojo connection error
  // notification.
  GetNfcProxy()->AddWriter(this);

  GetPermissionService()->RequestPermission(
      CreatePermissionDescriptor(PermissionName::NFC),
      LocalFrame::HasTransientUserActivation(DomWindow()->GetFrame()),
      WTF::BindOnce(&NDEFReader::WriteOnRequestPermission, WrapPersistent(this),
                    WrapPersistent(resolver), std::move(scoped_abort_state),
                    WrapPersistent(options), std::move(message)));

  return resolver->Promise();
}

void NDEFReader::WriteOnRequestPermission(
    ScriptPromiseResolver* resolver,
    std::unique_ptr<ScopedAbortState> scoped_abort_state,
    const NDEFWriteOptions* options,
    device::mojom::blink::NDEFMessagePtr message,
    PermissionStatus status) {
  DCHECK(resolver);

  ScriptState* script_state = resolver->GetScriptState();

  if (!IsInParallelAlgorithmRunnable(resolver->GetExecutionContext(),
                                     script_state)) {
    return;
  }

  ScriptState::Scope script_state_scope(script_state);

  if (status != PermissionStatus::GRANTED) {
    resolver->Reject(V8ThrowDOMException::CreateOrDie(
        script_state->GetIsolate(), DOMExceptionCode::kNotAllowedError,
        "NFC permission request denied."));
    return;
  }

  if (write_signal_ && write_signal_->aborted()) {
    resolver->Reject(write_signal_->reason(script_state));
    return;
  }

  auto callback =
      WTF::BindOnce(&NDEFReader::WriteOnRequestCompleted, WrapPersistent(this),
                    WrapPersistent(resolver), std::move(scoped_abort_state));
  GetNfcProxy()->Push(std::move(message),
                      device::mojom::blink::NDEFWriteOptions::From(options),
                      std::move(callback));
}

void NDEFReader::WriteOnRequestCompleted(
    ScriptPromiseResolver* resolver,
    std::unique_ptr<ScopedAbortState> scoped_abort_state,
    device::mojom::blink::NDEFErrorPtr error) {
  DCHECK(write_requests_.Contains(resolver));

  write_requests_.erase(resolver);

  ScriptState* script_state = resolver->GetScriptState();

  if (!IsInParallelAlgorithmRunnable(resolver->GetExecutionContext(),
                                     script_state)) {
    return;
  }

  AbortSignal* signal =
      scoped_abort_state ? scoped_abort_state->Signal() : nullptr;

  ScriptState::Scope script_state_scope(script_state);

  if (error.is_null()) {
    resolver->Resolve();
  } else if (signal && signal->aborted()) {
    resolver->Reject(signal->reason(script_state));
  } else {
    resolver->Reject(NDEFErrorPtrToDOMException(script_state->GetIsolate(),
                                                std::move(error)));
  }
}

void NDEFReader::WriteAbort(AbortSignal* signal) {
  if (!base::FeatureList::IsEnabled(features::kAbortSignalHandleBasedRemoval)) {
    // There is no RemoveAlgorithm() method on AbortSignal so compare the signal
    // bound to this callback to the one last passed to write().
    if (write_signal_ != signal)
      return;
  }

  // WriteOnRequestCompleted() should always be called whether the push
  // operation is cancelled successfully or not.
  GetNfcProxy()->CancelPush();
}

ScriptPromise NDEFReader::makeReadOnly(ScriptState* script_state,
                                       const NDEFMakeReadOnlyOptions* options,
                                       ExceptionState& exception_state) {
  // https://w3c.github.io/web-nfc/#security-policies
  // WebNFC API must be only accessible from top level browsing context.
  if (!DomWindow() || !DomWindow()->GetFrame()->IsMainFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kChildFrameErrorMessage);
    return ScriptPromise();
  }

  make_read_only_signal_ = options->getSignalOr(nullptr);
  std::unique_ptr<ScopedAbortState> scoped_abort_state = nullptr;
  if (make_read_only_signal_) {
    if (make_read_only_signal_->aborted()) {
      return ScriptPromise::Reject(
          script_state, make_read_only_signal_->reason(script_state));
    }
    auto* handle = make_read_only_signal_->AddAlgorithm(
        MakeGarbageCollected<MakeReadOnlyAbortAlgorithm>(
            this, make_read_only_signal_));
    scoped_abort_state =
        std::make_unique<ScopedAbortState>(make_read_only_signal_, handle);
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  make_read_only_requests_.insert(resolver);

  // Add the writer to proxy's writer list for Mojo connection error
  // notification.
  GetNfcProxy()->AddWriter(this);

  GetPermissionService()->RequestPermission(
      CreatePermissionDescriptor(PermissionName::NFC),
      LocalFrame::HasTransientUserActivation(DomWindow()->GetFrame()),
      WTF::BindOnce(&NDEFReader::MakeReadOnlyOnRequestPermission,
                    WrapPersistent(this), WrapPersistent(resolver),
                    std::move(scoped_abort_state), WrapPersistent(options)));

  return resolver->Promise();
}

void NDEFReader::MakeReadOnlyOnRequestPermission(
    ScriptPromiseResolver* resolver,
    std::unique_ptr<ScopedAbortState> scoped_abort_state,
    const NDEFMakeReadOnlyOptions* options,
    PermissionStatus status) {
  DCHECK(resolver);

  ScriptState* script_state = resolver->GetScriptState();

  if (!IsInParallelAlgorithmRunnable(resolver->GetExecutionContext(),
                                     script_state)) {
    return;
  }

  ScriptState::Scope script_state_scope(resolver->GetScriptState());

  if (status != PermissionStatus::GRANTED) {
    resolver->Reject(V8ThrowDOMException::CreateOrDie(
        script_state->GetIsolate(), DOMExceptionCode::kNotAllowedError,
        "NFC permission request denied."));
    return;
  }

  if (make_read_only_signal_ && make_read_only_signal_->aborted()) {
    resolver->Reject(make_read_only_signal_->reason(script_state));
    return;
  }

  auto callback = WTF::BindOnce(&NDEFReader::MakeReadOnlyOnRequestCompleted,
                                WrapPersistent(this), WrapPersistent(resolver),
                                std::move(scoped_abort_state));
  GetNfcProxy()->MakeReadOnly(std::move(callback));
}

void NDEFReader::MakeReadOnlyOnRequestCompleted(
    ScriptPromiseResolver* resolver,
    std::unique_ptr<ScopedAbortState> scoped_abort_state,
    device::mojom::blink::NDEFErrorPtr error) {
  DCHECK(make_read_only_requests_.Contains(resolver));

  make_read_only_requests_.erase(resolver);

  ScriptState* script_state = resolver->GetScriptState();

  if (!IsInParallelAlgorithmRunnable(resolver->GetExecutionContext(),
                                     script_state)) {
    return;
  }

  AbortSignal* signal =
      scoped_abort_state ? scoped_abort_state->Signal() : nullptr;

  ScriptState::Scope script_state_scope(script_state);

  if (error.is_null()) {
    resolver->Resolve();
  } else if (signal && signal->aborted()) {
    resolver->Reject(signal->reason(script_state));
  } else {
    resolver->Reject(NDEFErrorPtrToDOMException(script_state->GetIsolate(),
                                                std::move(error)));
  }
}

void NDEFReader::MakeReadOnlyAbort(AbortSignal* signal) {
  if (!base::FeatureList::IsEnabled(features::kAbortSignalHandleBasedRemoval)) {
    // There is no RemoveAlgorithm() method on AbortSignal so compare the signal
    // bound to this callback to the one last passed to makeReadOnly().
    if (make_read_only_signal_ != signal)
      return;
  }

  // MakeReadOnlyOnRequestCompleted() should always be called whether the
  // makeReadOnly operation is cancelled successfully or not.
  GetNfcProxy()->CancelMakeReadOnly();
}

NFCProxy* NDEFReader::GetNfcProxy() const {
  DCHECK(DomWindow());
  return NFCProxy::From(*DomWindow());
}

void NDEFReader::Trace(Visitor* visitor) const {
  visitor->Trace(permission_service_);
  visitor->Trace(scan_resolver_);
  visitor->Trace(scan_signal_);
  visitor->Trace(scan_abort_handle_);
  visitor->Trace(write_requests_);
  visitor->Trace(write_signal_);
  visitor->Trace(make_read_only_requests_);
  visitor->Trace(make_read_only_signal_);
  EventTargetWithInlineData::Trace(visitor);
  ActiveScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

PermissionService* NDEFReader::GetPermissionService() {
  if (!permission_service_.is_bound()) {
    ConnectToPermissionService(
        GetExecutionContext(),
        permission_service_.BindNewPipeAndPassReceiver(
            GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }
  return permission_service_.get();
}

void NDEFReader::ReadOnMojoConnectionError() {
  // If |scan_resolver_| has already settled this rejection is silently ignored.
  if (!scan_resolver_)
    return;

  ScriptState* script_state = scan_resolver_->GetScriptState();

  if (!IsInParallelAlgorithmRunnable(scan_resolver_->GetExecutionContext(),
                                     script_state)) {
    scan_resolver_.Clear();
    return;
  }

  ScriptState::Scope script_state_scope(script_state);

  scan_resolver_->Reject(NDEFErrorTypeToDOMException(
      script_state->GetIsolate(),
      device::mojom::blink::NDEFErrorType::NOT_SUPPORTED,
      kNotSupportedOrPermissionDenied));
  scan_resolver_.Clear();
}

void NDEFReader::WriteOnMojoConnectionError() {
  // If the mojo connection breaks, All push requests will be rejected with a
  // default error.

  // Script may execute during a call to Reject(). Swap these sets to prevent
  // concurrent modification.
  HeapHashSet<Member<ScriptPromiseResolver>> write_requests;
  write_requests_.swap(write_requests);
  for (ScriptPromiseResolver* resolver : write_requests) {
    DCHECK(resolver);

    ScriptState* script_state = resolver->GetScriptState();

    if (!IsInParallelAlgorithmRunnable(resolver->GetExecutionContext(),
                                       script_state)) {
      continue;
    }

    ScriptState::Scope script_state_scope(script_state);

    resolver->Reject(NDEFErrorTypeToDOMException(
        script_state->GetIsolate(),
        device::mojom::blink::NDEFErrorType::NOT_SUPPORTED,
        kNotSupportedOrPermissionDenied));
  }
}

void NDEFReader::MakeReadOnlyOnMojoConnectionError() {
  // If the mojo connection breaks, All makeReadOnly requests will be rejected
  // with a default error.

  // Script may execute during a call to Reject(). Swap these sets to prevent
  // concurrent modification.
  HeapHashSet<Member<ScriptPromiseResolver>> make_read_only_requests;
  make_read_only_requests_.swap(make_read_only_requests);
  for (ScriptPromiseResolver* resolver : make_read_only_requests) {
    DCHECK(resolver);

    ScriptState* script_state = resolver->GetScriptState();

    if (!IsInParallelAlgorithmRunnable(resolver->GetExecutionContext(),
                                       script_state)) {
      continue;
    }

    ScriptState::Scope script_state_scope(script_state);

    resolver->Reject(NDEFErrorTypeToDOMException(
        script_state->GetIsolate(),
        device::mojom::blink::NDEFErrorType::NOT_SUPPORTED,
        kNotSupportedOrPermissionDenied));
  }
}

}  // namespace blink
