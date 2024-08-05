// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_summarizer_capabilities.h"

namespace blink {

AISummarizerCapabilities::AISummarizerCapabilities(
    V8AIModelAvailability model_availability)
    : model_availability_(model_availability) {}

void AISummarizerCapabilities::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
