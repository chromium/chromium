// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_METRICS_H_

#include <string>

namespace blink {

class AIMetrics {
 public:
  // This class contains all the supported session types.
  enum class AISessionType {
    kText = 0,
    kMaxValue = kText,
  };

  // This class contains all the model execution API supported.
  //
  // LINT.IfChange(AIAPI)
  enum class AIAPI {
    kCanCreateSession = 0,
    kCreateSession = 1,
    kSessionPrompt = 2,
    kSessionPromptStreaming = 3,
    kDefaultTextSessionOptions = 4,
    kSessionDestroy = 5,
    kSessionClone = 6,
    kTextModelInfo = 7,
    kSessionSummarize = 8,
    kSessionSummarizeStreaming = 9,

    kMaxValue = kSessionSummarizeStreaming,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/ai/enums.xml:AIAPI)

  static std::string GetAIAPIUsageMetricName(AISessionType session_type);
  static std::string GetAIModelAvailabilityMetricName(
      AISessionType session_type);
  static std::string GetAISessionRequestSizeMetricName(
      AISessionType session_type);
  static std::string GetAISessionResponseStatusMetricName(
      AISessionType session_type);
  static std::string GetAISessionResponseSizeMetricName(
      AISessionType session_type);
  static std::string GetAISessionResponseCallbackCountMetricName(
      AISessionType session_type);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_METRICS_H_
