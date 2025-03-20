// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_rewriter.h"

#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_rewriter_rewrite_options.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"

namespace blink {

AIRewriter::AIRewriter(
    ExecutionContext* execution_context,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    mojo::PendingRemote<mojom::blink::AIRewriter> pending_remote,
    AIRewriterCreateOptions* options)
    : AIWritingAssistanceBase<mojom::blink::AIRewriter,
                              AIRewriterCreateOptions,
                              AIRewriterRewriteOptions>(
          execution_context,
          task_runner,
          std::move(pending_remote),
          std::move(options),
          AIMetrics::AISessionType::kRewriter,
          /*echo_whitespace_input=*/true) {}

void AIRewriter::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  AIWritingAssistanceBase::Trace(visitor);
}

void AIRewriter::remoteExecute(
    const String& input,
    const String& context,
    mojo::PendingRemote<blink::mojom::blink::ModelStreamingResponder>
        responder) {
  remote_->Rewrite(input, context, std::move(responder));
}

ScriptPromise<IDLString> AIRewriter::rewrite(
    ScriptState* script_state,
    const String& writing_task,
    const AIRewriterRewriteOptions* options,
    ExceptionState& exception_state) {
  return AIWritingAssistanceBase::execute(script_state, writing_task, options,
                                          exception_state,
                                          AIMetrics::AIAPI::kRewriterRewrite);
}

ReadableStream* AIRewriter::rewriteStreaming(
    ScriptState* script_state,
    const String& writing_task,
    const AIRewriterRewriteOptions* options,
    ExceptionState& exception_state) {
  return AIWritingAssistanceBase::executeStreaming(
      script_state, writing_task, options, exception_state,
      AIMetrics::AIAPI::kRewriterRewriteStreaming);
}

ScriptPromise<IDLDouble> AIRewriter::measureInputUsage(
    ScriptState* script_state,
    const String& writing_task,
    const AIRewriterRewriteOptions* options,
    ExceptionState& exception_state) {
  return AIWritingAssistanceBase::measureInputUsage(script_state, writing_task,
                                                    options, exception_state);
}

void AIRewriter::destroy(ScriptState* script_state,
                         ExceptionState& exception_state) {
  AIWritingAssistanceBase::destroy(script_state, exception_state);
}

}  // namespace blink
