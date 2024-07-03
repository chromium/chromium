// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_METRICS_H_

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

    kMaxValue = kSessionDestroy,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/ai/enums.xml:AIAPI)

  static const char* GetAIAPIUsageMetricName(AISessionType session_type);
  static const char* GetAIModelAvailabilityMetricName(
      AISessionType session_type);
  static const char* GetAISessionRequestSizeMetricName(
      AISessionType session_type);
  static const char* GetAISessionResponseStatusMetricName(
      AISessionType session_type);
  static const char* GetAISessionResponseSizeMetricName(
      AISessionType session_type);
  static const char* GetAISessionResponseCallbackCountMetricName(
      AISessionType session_type);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_METRICS_H_
