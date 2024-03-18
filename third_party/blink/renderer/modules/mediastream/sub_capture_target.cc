// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/sub_capture_target.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/mediastream/media_devices.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

MediaDevices* SubCaptureTarget::GetMediaDevices(
    ScriptState* script_state,
    Element* element,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

#if BUILDFLAG(IS_ANDROID)
  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Unsupported.");
  return nullptr;
#else
  if (!script_state || !script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid state.");
    return nullptr;
  }

  if (!element) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Invalid state.");
    return nullptr;
  }

  ExecutionContext* const context = ExecutionContext::From(script_state);

  if (!context || !context->IsWindow() || context->IsContextDestroyed() ||
      element->GetExecutionContext() != context) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid state.");
    return nullptr;
  }

  LocalDOMWindow* const window = To<LocalDOMWindow>(context);
  if (!window || !window->GetFrame() || !window->isSecureContext()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid state.");
    return nullptr;
  }

  Navigator* const navigator = window->navigator();
  if (!navigator) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid state.");
    return nullptr;
  }

  MediaDevices* const media_devices = MediaDevices::mediaDevices(*navigator);
  if (!media_devices) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid state.");
    return nullptr;
  }

  return media_devices;
#endif
}

SubCaptureTarget::SubCaptureTarget(Type type, String id)
    : type_(type), id_(std::move(id)) {
  CHECK(!id_.empty());
}

}  // namespace blink
