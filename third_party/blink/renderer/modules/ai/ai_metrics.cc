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
    case AIMetrics::AISessionType::kLanguageModel:
      return "LanguageModel";
    case AIMetrics::AISessionType::kWriter:
      return "Writer";
    case AIMetrics::AISessionType::kRewriter:
      return "Rewriter";
    case AIMetrics::AISessionType::kSummarizer:
      return "Summarizer";
    case AIMetrics::AISessionType::kTranslator:
      return "Translator";
    case AIMetrics::AISessionType::kLanguageDetector:
      return "LanguageDetector";
    case AIMetrics::AISessionType::kProofreader:
      return "Proofreader";
  }
  NOTREACHED();
}

}  // namespace

// static
std::string AIMetrics::GetAIAPIUsageMetricName(AISessionType session_type) {
  return base::StrCat({"AI.", GetAISessionTypeName(session_type), ".APIUsage"});
}

// static
std::string AIMetrics::GetAvailabilityMetricName(AISessionType session_type) {
  return base::StrCat(
      {"AI.", GetAISessionTypeName(session_type), ".AvailabilityV2"});
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

// static
AIMetrics::LanguageModelInputType AIMetrics::ToLanguageModelInputType(
    mojom::blink::AILanguageModelPromptContent::Tag type) {
  using MojoEnum = mojom::blink::AILanguageModelPromptContent::Tag;
  using MetricsEnum = AIMetrics::LanguageModelInputType;
  switch (type) {
    case MojoEnum::kText:
      return MetricsEnum::kText;
    case MojoEnum::kBitmap:
      return MetricsEnum::kImage;
    case MojoEnum::kAudio:
      return MetricsEnum::kAudio;
  }
  NOTREACHED();
}

// static
AIMetrics::LanguageModelInputType AIMetrics::ToLanguageModelInputType(
    V8LanguageModelMessageType::Enum type) {
  using V8Enum = V8LanguageModelMessageType::Enum;
  using MetricsEnum = AIMetrics::LanguageModelInputType;
  switch (type) {
    case V8Enum::kText:
      return MetricsEnum::kText;
    case V8Enum::kImage:
      return MetricsEnum::kImage;
    case V8Enum::kAudio:
      return MetricsEnum::kAudio;
    case V8Enum::kToolCall:
      return MetricsEnum::kToolCall;
    case V8Enum::kToolResponse:
      return MetricsEnum::kToolResponse;
  }
  NOTREACHED();
}

// static
AIMetrics::LanguageModelInputRole AIMetrics::ToLanguageModelInputRole(
    mojom::blink::AILanguageModelPromptRole role) {
  using MojoEnum = mojom::blink::AILanguageModelPromptRole;
  using MetricsEnum = AIMetrics::LanguageModelInputRole;
  switch (role) {
    case MojoEnum::kSystem:
      return MetricsEnum::kSystem;
    case MojoEnum::kUser:
      return MetricsEnum::kUser;
    case MojoEnum::kAssistant:
      return MetricsEnum::kAssistant;
    case MojoEnum::kToolCall:
      return MetricsEnum::kToolCall;
    case MojoEnum::kToolResponse:
      return MetricsEnum::kToolResponse;
  }
  NOTREACHED();
}
}  // namespace blink
