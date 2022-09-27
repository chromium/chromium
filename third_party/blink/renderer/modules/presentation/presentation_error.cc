// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/presentation/presentation_error.h"

#include "third_party/blink/public/mojom/presentation/presentation.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

v8::Local<v8::Value> CreatePresentationError(
    v8::Isolate* isolate,
    const mojom::blink::PresentationError& error) {
  DOMExceptionCode code = DOMExceptionCode::kUnknownError;
  switch (error.error_type) {
    case mojom::blink::PresentationErrorType::NO_AVAILABLE_SCREENS:
    case mojom::blink::PresentationErrorType::NO_PRESENTATION_FOUND:
      code = DOMExceptionCode::kNotFoundError;
      break;
    case mojom::blink::PresentationErrorType::PRESENTATION_REQUEST_CANCELLED:
      code = DOMExceptionCode::kNotAllowedError;
      break;
    case mojom::blink::PresentationErrorType::PREVIOUS_START_IN_PROGRESS:
      code = DOMExceptionCode::kOperationError;
      break;
    case mojom::blink::PresentationErrorType::UNKNOWN:
      code = DOMExceptionCode::kUnknownError;
      break;
  }

  return V8ThrowDOMException::CreateOrDie(isolate, code, error.message);
}

}  // namespace blink
