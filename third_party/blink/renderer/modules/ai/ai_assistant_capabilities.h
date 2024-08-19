// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_ASSISTANT_CAPABILITIES_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_ASSISTANT_CAPABILITIES_H_

#include <optional>

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_capability_availability.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"

namespace blink {

class AIAssistantCapabilities final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit AIAssistantCapabilities(
      V8AICapabilityAvailability capability_availability);

  void Trace(Visitor* visitor) const override;

  // ai_assistant_factory.idl implementation
  V8AICapabilityAvailability available() const {
    return capability_availability_;
  }
  std::optional<uint64_t> defaultTopK() const { return default_top_k_; }
  std::optional<uint64_t> maxTopK() const { return max_top_k_; }
  std::optional<float> defaultTemperature() const {
    return default_temperature_;
  }

  void SetDefaultTopK(uint64_t default_top_k) {
    default_top_k_ = default_top_k;
  }
  void SetMaxTopK(uint64_t max_top_k) { max_top_k_ = max_top_k; }
  void SetDefaultTemperature(float default_temperature) {
    default_temperature_ = default_temperature;
  }

 private:
  V8AICapabilityAvailability capability_availability_;
  std::optional<uint64_t> default_top_k_;
  std::optional<uint64_t> max_top_k_;
  std::optional<float> default_temperature_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_ASSISTANT_CAPABILITIES_H_
