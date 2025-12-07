// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_METRICS_H_

#include <optional>
#include <string>

#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_message_content.h"

namespace blink {

class AIMetrics {
 public:
  // This class contains all the supported session types.
  // LINT.IfChange(AISessionType)
  enum class AISessionType {
    kLanguageModel = 0,
    kWriter = 1,
    kRewriter = 2,
    kSummarizer = 3,
    kTranslator = 4,
    kLanguageDetector = 5,
    kProofreader = 6,
    kMaxValue = kProofreader,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/ai/histograms.xml:SessionType)

  // LINT.IfChange(LanguageModelInputType)
  enum class LanguageModelInputType {
    kText = 0,
    kImage = 1,
    kAudio = 2,
    kToolCall = 3,
    kToolResponse = 4,
    kMaxValue = kToolResponse,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/ai/enums.xml:LanguageModelInputType)

  // LINT.IfChange(LanguageModelInputRole)
  enum class LanguageModelInputRole {
    kSystem = 0,
    kUser = 1,
    kAssistant = 2,
    kToolCall = 3,
    kToolResponse = 4,
    kMaxValue = kToolResponse,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/ai/enums.xml:LanguageModelInputRole)

  static std::string GetAIAPIUsageMetricName(AISessionType session_type);
  static std::string GetAvailabilityMetricName(AISessionType session_type);
  static std::string GetAISessionRequestSizeMetricName(
      AISessionType session_type);
  static std::string GetAISessionResponseStatusMetricName(
      AISessionType session_type);
  static std::string GetAISessionResponseSizeMetricName(
      AISessionType session_type);
  static std::string GetAISessionResponseCallbackCountMetricName(
      AISessionType session_type);

  // Enum mappings from mojo/V8 enums to metric enums. Returns nullopt if the
  // enum is not mapped.
  static LanguageModelInputType ToLanguageModelInputType(
      mojom::blink::AILanguageModelPromptContent::Tag type);
  static LanguageModelInputType ToLanguageModelInputType(
      V8LanguageModelMessageType::Enum type);
  static LanguageModelInputRole ToLanguageModelInputRole(
      mojom::blink::AILanguageModelPromptRole role);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_METRICS_H_
