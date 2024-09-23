// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_summarizer_capabilities.h"

namespace blink {

AISummarizerCapabilities::AISummarizerCapabilities(
    V8AICapabilityAvailability capability_availability)
    : capability_availability_(capability_availability) {}

void AISummarizerCapabilities::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

V8AICapabilityAvailability AISummarizerCapabilities::supportsInputLanguage(
    const WTF::String& language_tag) {
  constexpr char kLanguageTagEn[] = "en";
  if (language_tag == kLanguageTagEn) {
    return V8AICapabilityAvailability(
        V8AICapabilityAvailability::Enum::kReadily);
  }
  return V8AICapabilityAvailability(V8AICapabilityAvailability::Enum::kNo);
}

}  // namespace blink
