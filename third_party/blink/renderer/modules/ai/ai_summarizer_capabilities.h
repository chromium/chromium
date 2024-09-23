// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_SUMMARIZER_CAPABILITIES_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_SUMMARIZER_CAPABILITIES_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_capability_availability.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_summarizer_format.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_summarizer_length.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_summarizer_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"

namespace blink {

class AISummarizerCapabilities final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit AISummarizerCapabilities(
      V8AICapabilityAvailability capability_availability);

  void Trace(Visitor* visitor) const override;

  // ai_summarizer.idl implementation
  V8AICapabilityAvailability available() { return capability_availability_; }
  V8AICapabilityAvailability supportsType(V8AISummarizerType type) {
    return capability_availability_;
  }
  V8AICapabilityAvailability supportsFormat(V8AISummarizerFormat format) {
    return capability_availability_;
  }
  V8AICapabilityAvailability supportsLength(V8AISummarizerLength length) {
    return capability_availability_;
  }
  V8AICapabilityAvailability supportsInputLanguage(
      const WTF::String& language_tag);

 private:
  V8AICapabilityAvailability capability_availability_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_SUMMARIZER_CAPABILITIES_H_
