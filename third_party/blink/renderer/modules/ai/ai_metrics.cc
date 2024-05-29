// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_metrics.h"

#include "base/notreached.h"

namespace blink {

// static
const char* AIMetrics::GetAIAPIUsageMetricName(AISessionType session_type) {
  switch (session_type) {
    case AISessionType::kText:
      return "AI.Text.APIUsage";
  }
  NOTREACHED_IN_MIGRATION();
}

// static
const char* AIMetrics::GetAIModelAvailabilityMetricName(
    AISessionType session_type) {
  switch (session_type) {
    case AISessionType::kText:
      return "AI.Text.Availability";
  }
  NOTREACHED_IN_MIGRATION();
}

// static
const char* AIMetrics::GetAISessionRequestSizeMetricName(
    AISessionType session_type) {
  switch (session_type) {
    case AISessionType::kText:
      return "AI.Session.Text.PromptRequestSize";
  }
  NOTREACHED_IN_MIGRATION();
}

// static
const char* AIMetrics::GetAISessionResponseStatusMetricName(
    AISessionType session_type) {
  switch (session_type) {
    case AISessionType::kText:
      return "AI.Session.Text.PromptResponseStatus";
  }
  NOTREACHED_IN_MIGRATION();
}

// static
const char* AIMetrics::GetAISessionResponseSizeMetricName(
    AISessionType session_type) {
  switch (session_type) {
    case AISessionType::kText:
      return "AI.Session.Text.PromptResponseSize";
  }
  NOTREACHED_IN_MIGRATION();
}

// static
const char* AIMetrics::GetAISessionResponseCallbackCountMetricName(
    AISessionType session_type) {
  switch (session_type) {
    case AISessionType::kText:
      return "AI.Session.Text.PromptResponseCallbackCount";
  }
  NOTREACHED_IN_MIGRATION();
}
}  // namespace blink
