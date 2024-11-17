// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_summarizer_capabilities.h"

#include "third_party/blink/renderer/modules/ai/ai_capability_availability.h"

namespace blink {

AISummarizerCapabilities::AISummarizerCapabilities(
    V8AICapabilityAvailability capability_availability)
    : capability_availability_(capability_availability) {}

void AISummarizerCapabilities::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

V8AICapabilityAvailability AISummarizerCapabilities::languageAvailable(
    const WTF::String& language_tag) {
  if (language_tag == kAILanguageTagEn) {
    return V8AICapabilityAvailability(
        V8AICapabilityAvailability::Enum::kReadily);
  }
  return V8AICapabilityAvailability(V8AICapabilityAvailability::Enum::kNo);
}

}  // namespace blink
