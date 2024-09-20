// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_EXCEPTION_HELPERS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_EXCEPTION_HELPERS_H_

#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

using mojom::blink::ModelStreamingResponseStatus;

extern const char kExceptionMessageSessionDestroyed[];
extern const char kExceptionMessageInvalidTemperatureAndTopKFormat[];
extern const char kExceptionMessageUnableToCreateSession[];
extern const char kExceptionMessageUnableToCloneSession[];
extern const char kExceptionMessageRequestAborted[];
extern const char kExceptionMessageSystemPromptAndInitialPromptsExist[];
extern const char kExceptionMessageSystemPromptIsNotTheFirst[];

void ThrowInvalidContextException(ExceptionState& exception_state);
void ThrowSessionDestroyedException(ExceptionState& exception_state);
void ThrowAbortedException(ExceptionState& exception_state);

void RejectPromiseWithInternalError(ScriptPromiseResolverBase* resolver);

DOMException* CreateInternalErrorException();

DOMException* ConvertModelStreamingResponseErrorToDOMException(
    ModelStreamingResponseStatus error);

WTF::String ConvertModelAvailabilityCheckResultToDebugString(
    mojom::blink::ModelAvailabilityCheckResult result);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_EXCEPTION_HELPERS_H_
