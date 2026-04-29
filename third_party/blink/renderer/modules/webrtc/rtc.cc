// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webrtc/rtc.h"

#include "base/notreached.h"
#include "third_party/blink/public/common/webrtc/rtc_logging_utils.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_diagnostic_logging_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_finish_diagnostic_logging_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_start_diagnostic_logging_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

namespace {

template <typename OptionsType, typename T>
bool ValidateMetadata(OptionsType* options,
                      ScriptPromiseResolver<T>* resolver) {
  switch (RTCMetadataValidator::Validate(options->metadata())) {
    case RTCMetadataValidationError::kNone:
      return true;
    case RTCMetadataValidationError::kTooManyEntries:
      resolver->RejectWithRangeError("Too many metadata entries.");
      return false;
    case RTCMetadataValidationError::kEntryTooLong:
      resolver->RejectWithRangeError("Metadata entry too long.");
      return false;
  }
  NOTREACHED();
}

void OnDiagnosticLoggingResult(ScriptPromiseResolver<IDLString>* resolver,
                               const String& uuid) {
  if (resolver) {
    resolver->Resolve(uuid);
  }
}

void OnDiagnosticLoggingVoidResult(
    ScriptPromiseResolver<IDLUndefined>* resolver) {
  if (!resolver) {
    return;
  }
  resolver->Resolve();
}

}  // namespace

const char RTC::kSupplementName[] = "RTC";

RTC* RTC::rtc(Navigator& navigator) {
  RTC* supplement = Supplement<Navigator>::From<RTC>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<RTC>(navigator);
    ProvideTo(navigator, supplement);
  }
  return supplement;
}

RTC::RTC(Navigator& navigator)
    : Supplement<Navigator>(navigator),
      diagnostic_logging_dispatcher_(
          navigator.DomWindow()->GetExecutionContext()) {}

ScriptPromise<IDLString> RTC::startDiagnosticLogging(
    ScriptState* script_state,
    RTCStartDiagnosticLoggingOptions* options) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(script_state);
  auto promise = resolver->Promise();
  if (!ValidateMetadata(options, resolver)) {
    return promise;
  }

  HashMap<String, String> metadata;
  for (const auto& pair : options->metadata()) {
    metadata.Set(pair.first, pair.second);
  }

  GetDiagnosticLoggingDispatcher().StartDiagnosticLogging(
      options->allowUpload(), std::move(metadata),
      BindOnce(&OnDiagnosticLoggingResult, WrapPersistent(resolver)));

  return promise;
}

ScriptPromise<IDLUndefined> RTC::finishDiagnosticLogging(
    ScriptState* script_state,
    RTCFinishDiagnosticLoggingOptions* options) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();
  if (!ValidateMetadata(options, resolver)) {
    return promise;
  }

  HashMap<String, String> metadata;
  for (const auto& pair : options->metadata()) {
    metadata.Set(pair.first, pair.second);
  }

  GetDiagnosticLoggingDispatcher().FinishDiagnosticLogging(
      std::move(metadata),
      BindOnce(&OnDiagnosticLoggingVoidResult, WrapPersistent(resolver)));

  return promise;
}

ScriptPromise<IDLUndefined> RTC::cancelDiagnosticLogging(
    ScriptState* script_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();

  GetDiagnosticLoggingDispatcher().CancelDiagnosticLogging(
      BindOnce(&OnDiagnosticLoggingVoidResult, WrapPersistent(resolver)));

  return promise;
}

void RTC::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  Supplement<Navigator>::Trace(visitor);
  visitor->Trace(diagnostic_logging_dispatcher_);
}

mojom::blink::RTCLoggingDispatcher& RTC::GetDiagnosticLoggingDispatcher() {
  if (!diagnostic_logging_dispatcher_.is_bound()) {
    // TODO(guidou): Add disconnect handler.
    auto* window = GetSupplementable()->DomWindow();
    window->GetBrowserInterfaceBroker().GetInterface(
        diagnostic_logging_dispatcher_.BindNewPipeAndPassReceiver(
            window->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }
  return *diagnostic_logging_dispatcher_.get();
}

}  // namespace blink
