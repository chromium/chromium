// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/modules/ai/web_ai_features.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

// static
bool WebAIFeatures::IsPromptAPIEnabledForWebPlatform(
    v8::Local<v8::Context> v8_context) {
  return RuntimeEnabledFeatures::AIPromptAPIForWebPlatformEnabled(
      ExecutionContext::From(v8_context));
}

// static
bool WebAIFeatures::IsPromptAPIEnabledForExtension(
    v8::Local<v8::Context> v8_context) {
  return RuntimeEnabledFeatures::AIPromptAPIForExtensionEnabled(
      ExecutionContext::From(v8_context));
}

// static
bool WebAIFeatures::IsSummarizationAPIEnabled(
    v8::Local<v8::Context> v8_context) {
  return RuntimeEnabledFeatures::AISummarizationAPIEnabled(
      ExecutionContext::From(v8_context));
}

// static
bool WebAIFeatures::IsWriterAPIEnabled(v8::Local<v8::Context> v8_context) {
  return RuntimeEnabledFeatures::AIWriterAPIEnabled(
      ExecutionContext::From(v8_context));
}

// static
bool WebAIFeatures::IsRewriterAPIEnabled(v8::Local<v8::Context> v8_context) {
  return RuntimeEnabledFeatures::AIRewriterAPIEnabled(
      ExecutionContext::From(v8_context));
}

}  // namespace blink
