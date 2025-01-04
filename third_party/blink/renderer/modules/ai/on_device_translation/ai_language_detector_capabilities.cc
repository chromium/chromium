// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/on_device_translation/ai_language_detector_capabilities.h"

namespace blink {
namespace {
using LanguageDetectionModelStatus =
    AILanguageDetectorCapabilities::LanguageDetectionModelStatus;
V8AICapabilityAvailability MapToV8Capability(
    LanguageDetectionModelStatus model_status) {
  switch (model_status) {
    case LanguageDetectionModelStatus::kReadily:
      return V8AICapabilityAvailability(
          V8AICapabilityAvailability::Enum::kReadily);
    case LanguageDetectionModelStatus::kAfterDownload:
      return V8AICapabilityAvailability(
          V8AICapabilityAvailability::Enum::kAfterDownload);
    case LanguageDetectionModelStatus::kNotAvailable:
      return V8AICapabilityAvailability(V8AICapabilityAvailability::Enum::kNo);
  }
}
}  // namespace

AILanguageDetectorCapabilities::AILanguageDetectorCapabilities(
    LanguageDetectionModelStatus model_status)
    : model_status_(model_status) {}

V8AICapabilityAvailability AILanguageDetectorCapabilities::available(
    ScriptState* script_state,
    ExceptionState& exception_state) const {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The execution context is not valid.");
    return V8AICapabilityAvailability(V8AICapabilityAvailability::Enum::kNo);
  }
  return MapToV8Capability(model_status_);
}

V8AICapabilityAvailability AILanguageDetectorCapabilities::languageAvailable(
    const WTF::String& languageTag) {
  // TODO(crbug.com/349927087): Implement actual check for availability of each
  // language.
  return MapToV8Capability(model_status_);
}

}  // namespace blink
