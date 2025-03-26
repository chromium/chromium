// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_rewriter.h"

#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/ai_writing_assistance_create_client.h"

namespace blink {

template <>
void AIWritingAssistanceCreateClient<
    mojom::blink::AIRewriter,
    mojom::blink::AIManagerCreateRewriterClient,
    AIRewriterCreateOptions,
    AIRewriter>::
    RemoteCreate(
        mojo::PendingRemote<mojom::blink::AIManagerCreateRewriterClient>
            client_remote) {
  HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
      AIInterfaceProxy::GetAIManagerRemote(GetExecutionContext());
  ai_manager_remote->CreateRewriter(std::move(client_remote),
                                    ToMojoRewriterCreateOptions(options_));
}

// static
template <>
AIMetrics::AISessionType
AIWritingAssistanceBase<AIRewriter,
                        mojom::blink::AIRewriter,
                        mojom::blink::AIManagerCreateRewriterClient,
                        AIRewriterCreateCoreOptions,
                        AIRewriterCreateOptions,
                        AIRewriterRewriteOptions>::GetSessionType() {
  return AIMetrics::AISessionType::kRewriter;
}

// static
template <>
void AIWritingAssistanceBase<AIRewriter,
                             mojom::blink::AIRewriter,
                             mojom::blink::AIManagerCreateRewriterClient,
                             AIRewriterCreateCoreOptions,
                             AIRewriterCreateOptions,
                             AIRewriterRewriteOptions>::
    RemoteCanCreate(HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote,
                    AIRewriterCreateCoreOptions* options,
                    CanCreateCallback callback) {
  ai_manager_remote->CanCreateRewriter(ToMojoRewriterCreateOptions(options),
                                       std::move(callback));
}

AIRewriter::AIRewriter(
    ExecutionContext* execution_context,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    mojo::PendingRemote<mojom::blink::AIRewriter> pending_remote,
    AIRewriterCreateOptions* options)
    : AIWritingAssistanceBase<AIRewriter,
                              mojom::blink::AIRewriter,
                              mojom::blink::AIManagerCreateRewriterClient,
                              AIRewriterCreateCoreOptions,
                              AIRewriterCreateOptions,
                              AIRewriterRewriteOptions>(
          execution_context,
          task_runner,
          std::move(pending_remote),
          std::move(options),
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
