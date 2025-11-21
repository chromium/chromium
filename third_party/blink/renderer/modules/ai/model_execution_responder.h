// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_MODEL_EXECUTION_RESPONDER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_MODEL_EXECUTION_RESPONDER_H_

#include <tuple>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace blink {

class ReadableStream;
class ScriptState;

// Creates a ModelStreamingResponder that handles the streaming output of a
// model execution. The responder streams results into the returned
// ReadableStream.
MODULES_EXPORT std::tuple<
    ReadableStream*,
    mojo::PendingRemote<blink::mojom::blink::ModelStreamingResponder>>
CreateModelExecutionStreamingResponder(
    ScriptState* script_state,
    AbortSignal* signal,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    AIMetrics::AISessionType session_type,
    base::OnceCallback<void(mojom::blink::ModelExecutionContextInfoPtr)>
        complete_callback,
    base::RepeatingClosure overflow_callback);

// Creates a ModelStreamingResponder that handles the streaming output of the
// model execution. Callbacks are run on completion, error, or abort.
MODULES_EXPORT
mojo::PendingRemote<blink::mojom::blink::ModelStreamingResponder>
CreateModelExecutionResponder(
    ScriptState* script_state,
    AbortSignal* signal,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    AIMetrics::AISessionType session_type,
    base::OnceCallback<void(const String&,
                            mojom::blink::ModelExecutionContextInfoPtr)>
        complete_callback,
    base::RepeatingClosure overflow_callback,
    base::OnceCallback<void(DOMException*)> error_callback,
    base::OnceCallback<void()> abort_callback);

// Creates a closed ReadableStream without any chunk.
MODULES_EXPORT
ReadableStream* CreateEmptyReadableStream(
    ScriptState* script_state,
    AIMetrics::AISessionType session_type);

// Resolves API execution promise directly with the full response on model
// execution completion.
template <typename T>
void ResolvePromiseOnCompletion(
    ScriptPromiseResolver<T>* resolver,
    const String& response,
    mojom::blink::ModelExecutionContextInfoPtr context_info) {
  resolver->Resolve(response);
}

// Rejects API execution promise when model execution is aborted.
template <typename T>
void RejectPromiseOnAbort(ScriptPromiseResolver<T>* resolver,
                          AbortSignal* signal,
                          ScriptState* script_state) {
  if (signal) {
    resolver->Reject(signal->reason(script_state));
  } else {
    RejectPromiseWithInternalError(resolver);
  }
}

// Rejects API execution promise on error during model execution.
template <typename T>
void RejectPromiseOnError(ScriptPromiseResolver<T>* resolver,
                          DOMException* exception) {
  resolver->Reject(exception);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_MODEL_EXECUTION_RESPONDER_H_
