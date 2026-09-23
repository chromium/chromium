// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/diagnostic_logging/rtc_diagnostic_logging.h"

#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/uuid.h"
#include "third_party/blink/public/common/webrtc/rtc_logging_utils.h"
#include "third_party/blink/public/mojom/webrtc/rtc_logging.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_diagnostic_logging_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

namespace {

bool ValidateMetadata(const RTCDiagnosticLoggingOptions* options,
                      ExceptionState& exception_state) {
  switch (RTCMetadataValidator::Validate(options->metadata())) {
    case RTCMetadataValidationError::kNone:
      return true;
    case RTCMetadataValidationError::kTooManyEntries:
      exception_state.ThrowTypeError("Too many metadata entries.");
      return false;
    case RTCMetadataValidationError::kEntryTooLong:
      exception_state.ThrowTypeError("Metadata entry too long.");
      return false;
  }
  NOTREACHED();
}

LocalDOMWindow* ValidateWindow(ScriptState* script_state,
                               ExceptionState& exception_state) {
  if (!script_state || !script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "No local DOM window; is this a detached window?");
    return nullptr;
  }
  auto* window =
      DynamicTo<LocalDOMWindow>(ExecutionContext::From(script_state));
  if (!window || !window->GetFrame() || window->IsContextDestroyed()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "No local DOM window; is this a detached window?");
    return nullptr;
  }
  return window;
}

class RTCDiagnosticLoggingHost final
    : public GarbageCollected<RTCDiagnosticLoggingHost>,
      public Supplement<LocalDOMWindow> {
 public:
  static const char kSupplementName[];

  static RTCDiagnosticLoggingHost& From(LocalDOMWindow& window) {
    RTCDiagnosticLoggingHost* host =
        Supplement<LocalDOMWindow>::From<RTCDiagnosticLoggingHost>(window);
    if (!host) {
      host = MakeGarbageCollected<RTCDiagnosticLoggingHost>(window);
      ProvideTo(window, host);
    }
    return *host;
  }

  explicit RTCDiagnosticLoggingHost(LocalDOMWindow& window)
      : Supplement<LocalDOMWindow>(window),
        diagnostic_logging_dispatcher_(&window) {}

  String StartDiagnosticLogging(const RTCDiagnosticLoggingOptions* options,
                                ExceptionState& exception_state) {
    if (is_logging_active_) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "A diagnostic logging session is already active.");
      return String();
    }

    is_logging_active_ = true;
    base::Uuid uuid = base::Uuid::GenerateRandomV4();

    HashMap<String, String> metadata;
    for (const auto& pair : options->metadata()) {
      metadata.Set(pair.first, pair.second);
    }

    GetDispatcher().StartDiagnosticLogging(
        uuid, /*upload=*/true, std::move(metadata), base::DoNothing());
    return String(uuid.AsLowercaseString());
  }

  void StopDiagnosticLogging(ExceptionState& exception_state) {
    if (!is_logging_active_) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "No active diagnostic logging session.");
      return;
    }

    is_logging_active_ = false;
    GetDispatcher().FinishDiagnosticLogging(/*metadata=*/{}, base::DoNothing());
  }

  void CancelDiagnosticLogging(ExceptionState& exception_state) {
    if (!is_logging_active_) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "No active diagnostic logging session.");
      return;
    }

    is_logging_active_ = false;
    GetDispatcher().CancelDiagnosticLogging(base::DoNothing());
  }

  void Trace(Visitor* visitor) const override {
    Supplement<LocalDOMWindow>::Trace(visitor);
    visitor->Trace(diagnostic_logging_dispatcher_);
  }

 private:
  mojom::blink::RTCLoggingDispatcher& GetDispatcher() {
    if (!diagnostic_logging_dispatcher_.is_bound()) {
      LocalDOMWindow* window = GetSupplementable();
      window->GetBrowserInterfaceBroker().GetInterface(
          diagnostic_logging_dispatcher_.BindNewPipeAndPassReceiver(
              window->GetTaskRunner(TaskType::kMiscPlatformAPI)));
    }
    return *diagnostic_logging_dispatcher_.get();
  }

  HeapMojoRemote<mojom::blink::RTCLoggingDispatcher>
      diagnostic_logging_dispatcher_;
  // Tracks whether the [[RTCDiagnosticLoggingSessionId]] internal slot on the
  // global object is non-null.
  bool is_logging_active_ = false;
};

const char RTCDiagnosticLoggingHost::kSupplementName[] =
    "RTCDiagnosticLoggingHost";

}  // namespace

String RTCDiagnosticLogging::startDiagnosticLogging(
    ScriptState* script_state,
    const RTCDiagnosticLoggingOptions* options,
    ExceptionState& exception_state) {
  if (!ValidateMetadata(options, exception_state)) {
    return String();
  }
  auto* window = ValidateWindow(script_state, exception_state);
  if (!window) {
    return String();
  }
  return RTCDiagnosticLoggingHost::From(*window).StartDiagnosticLogging(
      options, exception_state);
}

void RTCDiagnosticLogging::stopDiagnosticLogging(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto* window = ValidateWindow(script_state, exception_state);
  if (!window) {
    return;
  }
  RTCDiagnosticLoggingHost::From(*window).StopDiagnosticLogging(
      exception_state);
}

void RTCDiagnosticLogging::cancelDiagnosticLogging(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto* window = ValidateWindow(script_state, exception_state);
  if (!window) {
    return;
  }
  RTCDiagnosticLoggingHost::From(*window).CancelDiagnosticLogging(
      exception_state);
}

}  // namespace blink
