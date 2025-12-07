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
class V8LanguageModelPrompt;

// Helper function to convert V8 prompt input into mojo objects for transport
// across IPC. Works asynchronously to allow for asynchronous fetching/decoding
// of image, audio and blobs.
void ConvertPromptInputsToMojo(
    ScriptState* script_state,
    AbortSignal* abort_signal,
    const V8LanguageModelPrompt* input,
    HashSet<mojom::blink::AILanguageModelPromptType> allowed_types,
    const String& json_schema,
    base::OnceCallback<void(Vector<mojom::blink::AILanguageModelPromptPtr>)>
        resolved_callback,
    base::OnceCallback<void(const ScriptValue& error)> rejected_callback);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_LANGUAGE_MODEL_PROMPT_BUILDER_H_
