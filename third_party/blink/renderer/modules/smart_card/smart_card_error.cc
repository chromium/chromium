// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/smart_card/smart_card_error.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_error_options.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// static
SmartCardError* SmartCardError::Create(String message,
                                       const SmartCardErrorOptions* options) {
  return MakeGarbageCollected<SmartCardError>(std::move(message),
                                              options->responseCode());
}

SmartCardError::SmartCardError(String message,
                               V8SmartCardResponseCode::Enum value)
    : SmartCardError(std::move(message), V8SmartCardResponseCode(value)) {}

SmartCardError::SmartCardError(String message,
                               V8SmartCardResponseCode response_code)
    : DOMException(DOMExceptionCode::kSmartCardError, std::move(message)),
      response_code_(std::move(response_code)) {}

SmartCardError::~SmartCardError() = default;

}  // namespace blink
