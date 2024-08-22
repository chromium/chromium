// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_metrics.h"

#include <string_view>

#include "base/notreached.h"
#include "base/strings/strcat.h"

namespace blink {
namespace {

std::string_view GetAISessionTypeName(AIMetrics::AISessionType session_type) {
  switch (session_type) {
    case AIMetrics::AISessionType::kAssistant:
      return "Assistant";
    case AIMetrics::AISessionType::kWriter:
      return "Writer";
    case AIMetrics::AISessionType::kRewriter:
      return "Rewriter";
    case AIMetrics::AISessionType::kSummarizer:
      return "Summarizer";
  }
  NOTREACHED_IN_MIGRATION();
}

}  // namespace

// static
std::string AIMetrics::GetAIAPIUsageMetricName(AISessionType session_type) {
  return base::StrCat({"AI.", GetAISessionTypeName(session_type), ".APIUsage"});
}

// static
std::string AIMetrics::GetAICapabilityAvailabilityMetricName(
    AISessionType session_type) {
  return base::StrCat(
      {"AI.", GetAISessionTypeName(session_type), ".Availability"});
}

// static
std::string AIMetrics::GetAISessionRequestSizeMetricName(
    AISessionType session_type) {
  return base::StrCat({"AI.Session.", GetAISessionTypeName(session_type),
                       ".PromptRequestSize"});
}

// static
std::string AIMetrics::GetAISessionResponseStatusMetricName(
    AISessionType session_type) {
  return base::StrCat({"AI.Session.", GetAISessionTypeName(session_type),
                       ".PromptResponseStatus"});
}

// static
std::string AIMetrics::GetAISessionResponseSizeMetricName(
    AISessionType session_type) {
  return base::StrCat({"AI.Session.", GetAISessionTypeName(session_type),
                       ".PromptResponseSize"});
}

// static
std::string AIMetrics::GetAISessionResponseCallbackCountMetricName(
    AISessionType session_type) {
  return base::StrCat({"AI.Session.", GetAISessionTypeName(session_type),
                       ".PromptResponseCallbackCount"});
}
}  // namespace blink
