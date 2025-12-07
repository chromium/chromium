// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_EXCEPTION_HELPERS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_EXCEPTION_HELPERS_H_

#include "third_party/blink/public/mojom/ai/ai_common.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

using mojom::blink::ModelStreamingResponseStatus;

extern const char kExceptionMessageSessionDestroyed[];
extern const char kExceptionMessageInvalidTemperatureAndTopKFormat[];
extern const char kExceptionMessageInvalidTopK[];
extern const char kExceptionMessageInvalidTemperature[];
extern const char kExceptionMessageUnableToCreateSession[];
extern const char kExceptionMessageUnableToCloneSession[];
extern const char kExceptionMessageUnableToCalculateUsage[];
extern const char kExceptionMessageInputTooLarge[];
extern const char kExceptionMessageRequestAborted[];
extern const char kExceptionMessagePromptWithSystemRoleIsNotTheFirst[];
extern const char kExceptionMessageUnsupportedLanguages[];
extern const char kExceptionMessageInvalidResponseJsonSchema[];
extern const char kExceptionMessagePermissionPolicy[];
extern const char kExceptionMessageUserActivationRequired[];

void ThrowInvalidContextException(ExceptionState& exception_state);
void ThrowDocumentNotActiveException(ExceptionState& exception_state);
void ThrowSessionDestroyedException(ExceptionState& exception_state);
void ThrowAbortedException(ExceptionState& exception_state);

MODULES_EXPORT void RejectPromiseWithInternalError(
    ScriptPromiseResolverBase* resolver);

DOMException* CreateInternalErrorException();
DOMException* CreateSessionDestroyedException();

DOMException* ConvertModelStreamingResponseErrorToDOMException(
    ModelStreamingResponseStatus error,
    mojom::blink::QuotaErrorInfoPtr quota_error_info);

String ConvertModelAvailabilityCheckResultToDebugString(
    mojom::blink::ModelAvailabilityCheckResult result);

// Throw the reason of the AbortSignal if it's aborted. If the reason is empty,
// an AbortError will be thrown.
// Returns if the signal is aborted.
bool HandleAbortSignal(AbortSignal* signal,
                       ScriptState* script_state,
                       ExceptionState& exception_state);

// Return true if fully active window or if service workers are permitted.
// Otherwise, throw an InvalidStateError and return false.
bool ValidateScriptState(ScriptState* script_state,
                         ExceptionState& exception_state,
                         bool permit_workers);

// Validates and stringifies the responseConstraint JSON schema option if
// provided. Throws an exception if an unsupported schema is detected.
String ValidateAndStringifyObject(const ScriptValue& input,
                                  ScriptState* script_state,
                                  ExceptionState& exception_state);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_EXCEPTION_HELPERS_H_
