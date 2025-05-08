// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_LANGUAGE_MODEL_PROMPT_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_LANGUAGE_MODEL_PROMPT_BUILDER_H_

#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

class ScriptState;
class AbortSignal;
class V8LanguageModelPromptInput;
class ExceptionState;

std::optional<WTF::Vector<mojom::blink::AILanguageModelPromptPtr>> BuildPrompts(
    const V8LanguageModelPromptInput* input,
    ScriptState* script_state,
    ExceptionState& exception_state,
    ExecutionContext* execution_context,
    WTF::HashSet<mojom::blink::AILanguageModelPromptType>& allowed_types);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_LANGUAGE_MODEL_PROMPT_BUILDER_H_
