// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/ndef_reader.h"

#include <utility>

#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/device/public/mojom/nfc.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/string_or_array_buffer_or_array_buffer_view_or_ndef_message_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ndef_scan_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ndef_write_options.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/nfc/ndef_message.h"
#include "third_party/blink/renderer/modules/nfc/ndef_reading_event.h"
#include "third_party/blink/renderer/modules/nfc/nfc_proxy.h"
#include "third_party/blink/renderer/modules/nfc/nfc_type_converters.h"
#include "third_party/blink/renderer/modules/nfc/nfc_utils.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/scheduling_policy.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

using mojom::blink::PermissionName;
using mojom::blink::PermissionService;
using mojom::blink::PermissionStatus;

namespace {

constexpr char kNotSupportedOrPermissionDenied[] =
    "Web NFC is unavailable or permission denied.";

constexpr char kChildFrameErrorMessage[] =
    "Web NFC can only be accessed in a top-level browsing context.";

constexpr char kInvalidStateErrorMessage[] = "A scan() operation is ongoing.";
}  // namespace

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

  // 7. If reader.[[Signal]]'s aborted flag is set, then reject p with a
  // "AbortError" DOMException and return p.
  if (options->hasSignal() && options->signal()->aborted()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kAbortError,
                                      "The NFC scan operation was cancelled.");
    return ScriptPromise();
  }

  if (has_pending_scan_request_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInvalidStateErrorMessage);
    return ScriptPromise();
  }
  has_pending_scan_request_ = true;

  // https://github.com/w3c/web-nfc/issues/592
  // reject scan promise when there's already an ongoing scan.
  if (GetNfcProxy()->IsReading(this)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInvalidStateErrorMessage);
    return ScriptPromise();
  }

  scan_resolver_ = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  // 8. If |signal| is not null, then add the following abort steps
  // to |signal|:
  if (options->hasSignal()) {
    options->signal()->AddAlgorithm(
        WTF::Bind(&NDEFReader::ReadAbort, WrapPersistent(this)));
  }

  GetPermissionService()->RequestPermission(
      CreatePermissionDescriptor(PermissionName::NFC),
      LocalFrame::HasTransientUserActivation(DomWindow()->GetFrame()),
      WTF::Bind(&NDEFReader::ReadOnRequestPermission, WrapPersistent(this),
                WrapPersistent(options)));
  return scan_resolver_->Promise();
}

void NDEFReader::ReadOnRequestPermission(const NDEFScanOptions* options,
                                         PermissionStatus status) {
  if (!scan_resolver_) {
    has_pending_scan_request_ = false;
    return;
  }

  if (status != PermissionStatus::GRANTED) {
    has_pending_scan_request_ = false;
    scan_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError, "NFC permission request denied."));
    scan_resolver_.Clear();
    return;
  }
  if (options->hasSignal() && options->signal()->aborted()) {
    has_pending_scan_request_ = false;
    scan_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kAbortError,
        "The NFC scan operation was cancelled."));
    scan_resolver_.Clear();
    return;
  }

  GetNfcProxy()->StartReading(
      this,
      WTF::Bind(&NDEFReader::ReadOnRequestCompleted, WrapPersistent(this)));
}

void NDEFReader::ReadOnRequestCompleted(
    device::mojom::blink::NDEFErrorPtr error) {
  has_pending_scan_request_ = false;
  if (!scan_resolver_)
    return;

  if (error) {
    scan_resolver_->Reject(
        NDEFErrorTypeToDOMException(error->error_type, error->error_message));
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
  DispatchEvent(*Event::Create(event_type_names::kReadingerror));
  GetExecutionContext()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kJavaScript,
      mojom::blink::ConsoleMessageLevel::kInfo, message));
}

void NDEFReader::ContextDestroyed() {
  GetNfcProxy()->StopReading(this);
}

void NDEFReader::ReadAbort() {
  if (scan_resolver_) {
    scan_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kAbortError,
        "The NFC scan operation was cancelled."));
    scan_resolver_.Clear();
  }

  GetNfcProxy()->StopReading(this);
}

// https://w3c.github.io/web-nfc/#writing-content
// https://w3c.github.io/web-nfc/#the-write-method
ScriptPromise NDEFReader::write(ScriptState* script_state,
                                const NDEFMessageSource& write_message,
                                const NDEFWriteOptions* options,
                                ExceptionState& exception_state) {
  // https://w3c.github.io/web-nfc/#security-policies
  // WebNFC API must be only accessible from top level browsing context.
  if (!DomWindow() || !DomWindow()->GetFrame()->IsMainFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kChildFrameErrorMessage);
    return ScriptPromise();
  }

  if (options->hasSignal() && options->signal()->aborted()) {
    // If signal’s aborted flag is set, then reject p with an "AbortError"
    // DOMException and return p.
    exception_state.ThrowDOMException(DOMExceptionCode::kAbortError,
                                      "The NFC write operation was cancelled.");
    return ScriptPromise();
  }

  // Step 11.2: Run "create NDEF message", if this throws an exception,
  // reject p with that exception and abort these steps.
  NDEFMessage* ndef_message =
      NDEFMessage::Create(DomWindow(), write_message, exception_state);
  if (exception_state.HadException()) {
    return ScriptPromise();
  }

  auto message = device::mojom::blink::NDEFMessage::From(ndef_message);
  DCHECK(message);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  write_requests_.insert(resolver);

  // Add the writer to proxy's writer list for Mojo connection error
  // notification.
  GetNfcProxy()->AddWriter(this);

  GetPermissionService()->RequestPermission(
      CreatePermissionDescriptor(PermissionName::NFC),
      LocalFrame::HasTransientUserActivation(DomWindow()->GetFrame()),
      WTF::Bind(&NDEFReader::WriteOnRequestPermission, WrapPersistent(this),
                WrapPersistent(resolver), WrapPersistent(options),
                std::move(message)));

  return resolver->Promise();
}

void NDEFReader::WriteOnRequestPermission(
    ScriptPromiseResolver* resolver,
    const NDEFWriteOptions* options,
    device::mojom::blink::NDEFMessagePtr message,
    PermissionStatus status) {
  if (status != PermissionStatus::GRANTED) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError, "NFC permission request denied."));
    return;
  }

  if (options->hasSignal() && options->signal()->aborted()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kAbortError,
        "The NFC write operation was cancelled."));
    return;
  }

  // If signal is not null, then add the abort steps to signal.
  if (options->hasSignal() && !options->signal()->aborted()) {
    options->signal()->AddAlgorithm(WTF::Bind(&NDEFReader::WriteAbort,
                                              WrapPersistent(this),
                                              WrapPersistent(resolver)));
  }

  auto callback = WTF::Bind(&NDEFReader::WriteOnRequestCompleted,
                            WrapPersistent(this), WrapPersistent(resolver));
  GetNfcProxy()->Push(std::move(message),
                      device::mojom::blink::NDEFWriteOptions::From(options),
                      std::move(callback));
}

void NDEFReader::WriteOnRequestCompleted(
    ScriptPromiseResolver* resolver,
    device::mojom::blink::NDEFErrorPtr error) {
  DCHECK(write_requests_.Contains(resolver));

  write_requests_.erase(resolver);

  if (error.is_null()) {
    resolver->Resolve();
  } else {
    resolver->Reject(
        NDEFErrorTypeToDOMException(error->error_type, error->error_message));
  }
}

void NDEFReader::WriteAbort(ScriptPromiseResolver* resolver) {
  // WriteOnRequestCompleted() should always be called whether the push
  // operation is cancelled successfully or not.
  GetNfcProxy()->CancelPush();
}

NFCProxy* NDEFReader::GetNfcProxy() const {
  DCHECK(DomWindow());
  return NFCProxy::From(*DomWindow());
}

void NDEFReader::Trace(Visitor* visitor) const {
  visitor->Trace(permission_service_);
  visitor->Trace(scan_resolver_);
  visitor->Trace(write_requests_);
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
  if (scan_resolver_) {
    scan_resolver_->Reject(NDEFErrorTypeToDOMException(
        device::mojom::blink::NDEFErrorType::NOT_SUPPORTED,
        kNotSupportedOrPermissionDenied));
    scan_resolver_.Clear();
  }
}

void NDEFReader::WriteOnMojoConnectionError() {
  // If the mojo connection breaks, All push requests will be rejected with a
  // default error.

  // Script may execute during a call to Resolve(). Swap these sets to prevent
  // concurrent modification.
  HeapHashSet<Member<ScriptPromiseResolver>> write_requests;
  write_requests_.swap(write_requests);
  write_requests_.clear();
  for (ScriptPromiseResolver* resolver : write_requests) {
    resolver->Reject(NDEFErrorTypeToDOMException(
        device::mojom::blink::NDEFErrorType::NOT_SUPPORTED,
        kNotSupportedOrPermissionDenied));
  }
}

}  // namespace blink
