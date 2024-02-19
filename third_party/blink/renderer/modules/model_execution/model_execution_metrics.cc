// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/model_execution/model_execution_metrics.h"
#include "base/notreached.h"

namespace blink {

// static
const char* ModelExecutionMetrics::GetModelExecutionAPIUsageMetricName(
    ModelExecutionSessionType session_type) {
  switch (session_type) {
    case ModelExecutionSessionType::kGeneric:
      return "ModelExecution.Generic.APIUsage";
  }
  NOTREACHED();
}

// static
const char* ModelExecutionMetrics::GetModelExecutionAvailabilityMetricName(
    ModelExecutionSessionType session_type) {
  switch (session_type) {
    case ModelExecutionSessionType::kGeneric:
      return "ModelExecution.Generic.Availability";
  }
  NOTREACHED();
}

// static
const char*
ModelExecutionMetrics::GetModelExecutionSessionRequestSizeMetricName(
    ModelExecutionSessionType session_type) {
  switch (session_type) {
    case ModelExecutionSessionType::kGeneric:
      return "ModelExecution.Session.Generic.ExecutionRequestSize";
  }
  NOTREACHED();
}

// static
const char*
ModelExecutionMetrics::GetModelExecutionSessionResponseStatusMetricName(
    ModelExecutionSessionType session_type) {
  switch (session_type) {
    case ModelExecutionSessionType::kGeneric:
      return "ModelExecution.Session.Generic.ExecutionResponseStatus";
  }
  NOTREACHED();
}

// static
const char*
ModelExecutionMetrics::GetModelExecutionSessionResponseSizeMetricName(
    ModelExecutionSessionType session_type) {
  switch (session_type) {
    case ModelExecutionSessionType::kGeneric:
      return "ModelExecution.Session.Generic.ExecutionResponseSize";
  }
  NOTREACHED();
}

// static
const char*
ModelExecutionMetrics::GetModelExecutionSessionResponseCallbackCountMetricName(
    ModelExecutionSessionType session_type) {
  switch (session_type) {
    case ModelExecutionSessionType::kGeneric:
      return "ModelExecution.Session.Generic.ExecutionResponseCallbackCount";
  }
  NOTREACHED();
}
}  // namespace blink
