// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_assistant_capabilities.h"

#include "third_party/blink/renderer/modules/ai/ai_capability_availability.h"

namespace blink {

AIAssistantCapabilities::AIAssistantCapabilities(
    V8AICapabilityAvailability capability_availability)
    : capability_availability_(capability_availability) {}

void AIAssistantCapabilities::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

V8AICapabilityAvailability AIAssistantCapabilities::languageAvailable(
    WTF::String languageTag) const {
  if (languageTag == kAILanguageTagEn) {
    return V8AICapabilityAvailability(
        V8AICapabilityAvailability::Enum::kReadily);
  }
  return V8AICapabilityAvailability(V8AICapabilityAvailability::Enum::kNo);
}

}  // namespace blink
