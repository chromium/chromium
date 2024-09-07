// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_assistant_capabilities.h"

namespace blink {

AIAssistantCapabilities::AIAssistantCapabilities(
    V8AICapabilityAvailability capability_availability)
    : capability_availability_(capability_availability) {}

void AIAssistantCapabilities::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
