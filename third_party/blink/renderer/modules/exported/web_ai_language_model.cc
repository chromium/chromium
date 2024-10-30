// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/modules/ai/web_ai_language_model.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/ai/ai.h"
#include "third_party/blink/renderer/modules/ai/ai_language_model_factory.h"
#include "third_party/blink/renderer/modules/ai/dom_ai.h"

namespace blink {

// static
v8::Local<v8::Value> WebAILanguageModel::GetAILanguageModelFactory(
    v8::Local<v8::Context> v8_context,
    v8::Isolate* isolate) {
  ExecutionContext* execution_context = ExecutionContext::From(v8_context);
  AILanguageModelFactory* language_model =
      DOMAI::ai(*execution_context)->languageModel();
  return language_model->ToV8(ScriptState::From(isolate, v8_context));
}

}  // namespace blink
