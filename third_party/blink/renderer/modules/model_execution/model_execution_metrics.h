// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MODEL_EXECUTION_MODEL_EXECUTION_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MODEL_EXECUTION_MODEL_EXECUTION_METRICS_H_

namespace blink {

class ModelExecutionMetrics {
 public:
  // This class contains all the supported session types.
  enum class ModelExecutionSessionType {
    kGeneric = 0,
    kMaxValue = kGeneric,
  };

  // This class contains all the model execution API supported.
  enum class ModelExecutionAPI {
    kModelCanCreateSession = 0,
    kModelCreateSession = 1,
    kSessionExecute = 2,
    kSessionExecuteStreaming = 3,
    kModelDefaultGenericSessionOptions = 4,

    kMaxValue = kModelDefaultGenericSessionOptions,
  };

  static const char* GetModelExecutionAPIUsageMetricName(
      ModelExecutionSessionType session_type);
  static const char* GetModelExecutionAvailabilityMetricName(
      ModelExecutionSessionType session_type);
  static const char* GetModelExecutionSessionRequestSizeMetricName(
      ModelExecutionSessionType session_type);
  static const char* GetModelExecutionSessionResponseStatusMetricName(
      ModelExecutionSessionType session_type);
  static const char* GetModelExecutionSessionResponseSizeMetricName(
      ModelExecutionSessionType session_type);
  static const char* GetModelExecutionSessionResponseCallbackCountMetricName(
      ModelExecutionSessionType session_type);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MODEL_EXECUTION_MODEL_EXECUTION_METRICS_H_
