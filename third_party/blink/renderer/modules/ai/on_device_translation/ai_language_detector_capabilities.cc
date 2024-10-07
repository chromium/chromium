// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/on_device_translation/ai_language_detector_capabilities.h"

namespace blink {

V8AICapabilityAvailability AILanguageDetectorCapabilities::available(
    ScriptState* script_state,
    ExceptionState& exception_state) const {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The execution context is not valid.");
    return V8AICapabilityAvailability(V8AICapabilityAvailability::Enum::kNo);
  }

  // TODO(crbug.com/349927087): Implement actual check for availability.
  return V8AICapabilityAvailability(V8AICapabilityAvailability::Enum::kReadily);
}

V8AICapabilityAvailability AILanguageDetectorCapabilities::canDetect(
    const WTF::String& languageTag) {
  // TODO(crbug.com/349927087): Implement actual check for availability.
  return V8AICapabilityAvailability(V8AICapabilityAvailability::Enum::kReadily);
}

}  // namespace blink
