// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/quota_exceeded_error.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_quota_exceeded_error_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

// static
QuotaExceededError* QuotaExceededError::Create(
    const String& message,
    const QuotaExceededErrorOptions* options) {
  // Not doing AttachStackProperty is consistent with `new DOMException()`
  // but should be updated to do so to match spec (crbug.com/40815519).
  return MakeGarbageCollected<QuotaExceededError>(message, options);
}

// static
v8::Local<v8::Value> QuotaExceededError::Create(
    v8::Isolate* isolate,
    const String& message,
    std::optional<double> quota,
    std::optional<double> requested) {
  auto* options = QuotaExceededErrorOptions::Create(isolate);
  if (quota) {
    options->setQuota(quota.value());
  }
  if (requested) {
    options->setRequested(requested.value());
  }
  auto* exception = MakeGarbageCollected<QuotaExceededError>(message, options);
  return V8ThrowDOMException::AttachStackProperty(isolate, exception);
}

// static
void QuotaExceededError::Throw(ExceptionState& exception_state,
                               const String& message,
                               std::optional<double> quota,
                               std::optional<double> requested) {
  if (RuntimeEnabledFeatures::QuotaExceededErrorUpdateEnabled()) {
    v8::Isolate* isolate = exception_state.GetIsolate();
    if (!isolate) {
      return;
    }
    v8::Local<v8::Value> exception = Create(isolate, message, quota, requested);
    exception_state.ThrowDOMException(
        exception, DOMExceptionCode::kQuotaExceededError, message);
  } else {
    exception_state.ThrowDOMException(DOMExceptionCode::kQuotaExceededError,
                                      message);
  }
}

// static
void QuotaExceededError::Reject(ScriptPromiseResolverBase* resolver,
                                const String& message,
                                std::optional<double> quota,
                                std::optional<double> requested) {
  if (RuntimeEnabledFeatures::QuotaExceededErrorUpdateEnabled()) {
    ScriptState* script_state = resolver->GetScriptState();
    ScriptState::Scope scope(script_state);
    v8::Isolate* isolate = script_state->GetIsolate();
    resolver->Reject(Create(isolate, message, quota, requested));
  } else {
    resolver->RejectWithDOMException(DOMExceptionCode::kQuotaExceededError,
                                     message);
  }
}

QuotaExceededError::QuotaExceededError(const String& message,
                                       const QuotaExceededErrorOptions* options)
    : DOMException(DOMExceptionCode::kQuotaExceededError, message),
      quota_(options->hasQuota() ? std::make_optional(options->quota())
                                 : std::nullopt),
      requested_(options->hasRequested()
                     ? std::make_optional(options->requested())
                     : std::nullopt) {}

QuotaExceededError::QuotaExceededError(const String& message,
                                       std::optional<double> quota,
                                       std::optional<double> requested)
    : DOMException(DOMExceptionCode::kQuotaExceededError, message),
      quota_(quota),
      requested_(requested) {}

QuotaExceededError::QuotaExceededError(const String& message)
    : DOMException(DOMExceptionCode::kQuotaExceededError, message) {}

}  // namespace blink
