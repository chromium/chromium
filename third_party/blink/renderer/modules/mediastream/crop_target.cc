// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/crop_target.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/mediastream/media_devices.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

ScriptPromise CropTarget::fromElement(ScriptState* script_state,
                                      Element* element,
                                      ExceptionState& exception_state) {
  DCHECK(IsMainThread());

#if BUILDFLAG(IS_ANDROID)
  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Unsupported.");
  return ScriptPromise();
#else
  if (!script_state || !script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid state.");
    return ScriptPromise();
  }

  if (!element || !element->IsSupportedByRegionCapture()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Invalid state.");
    return ScriptPromise();
  }

  ExecutionContext* const context = ExecutionContext::From(script_state);

  if (!context || !context->IsWindow() || context->IsContextDestroyed() ||
      element->GetExecutionContext() != context) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid state.");
    return ScriptPromise();
  }

  LocalDOMWindow* const window = To<LocalDOMWindow>(context);
  if (!window || !window->GetFrame() || !window->isSecureContext()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid state.");
    return ScriptPromise();
  }

  Navigator* const navigator = window->navigator();
  if (!navigator) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid state.");
    return ScriptPromise();
  }

  MediaDevices* const media_devices = MediaDevices::mediaDevices(*navigator);
  if (!media_devices) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid state.");
    return ScriptPromise();
  }

  // TODO(crbug.com/1332628): Perform the following clean-up steps:
  // 1. Stop Web-exposing produceCropId.
  // 2. Move some of the error-testing here, leaving MediaDevices with DCHECKs.
  return media_devices->ProduceCropTarget(script_state, element,
                                          exception_state);
#endif
}

CropTarget::CropTarget(String crop_id) : crop_id_(std::move(crop_id)) {
  DCHECK(!crop_id_.empty());
}

}  // namespace blink
