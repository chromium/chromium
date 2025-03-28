// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/rewriter.h"

#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/ai_writing_assistance_create_client.h"

namespace blink {

template <>
void AIWritingAssistanceCreateClient<
    mojom::blink::AIRewriter,
    mojom::blink::AIManagerCreateRewriterClient,
    RewriterCreateOptions,
    Rewriter>::
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
AIWritingAssistanceBase<Rewriter,
                        mojom::blink::AIRewriter,
                        mojom::blink::AIManagerCreateRewriterClient,
                        RewriterCreateCoreOptions,
                        RewriterCreateOptions,
                        RewriterRewriteOptions>::GetSessionType() {
  return AIMetrics::AISessionType::kRewriter;
}

// static
template <>
void AIWritingAssistanceBase<Rewriter,
                             mojom::blink::AIRewriter,
                             mojom::blink::AIManagerCreateRewriterClient,
                             RewriterCreateCoreOptions,
                             RewriterCreateOptions,
                             RewriterRewriteOptions>::
    RemoteCanCreate(HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote,
                    RewriterCreateCoreOptions* options,
                    CanCreateCallback callback) {
  ai_manager_remote->CanCreateRewriter(ToMojoRewriterCreateOptions(options),
                                       std::move(callback));
}

Rewriter::Rewriter(ExecutionContext* execution_context,
                   scoped_refptr<base::SequencedTaskRunner> task_runner,
                   mojo::PendingRemote<mojom::blink::AIRewriter> pending_remote,
                   RewriterCreateOptions* options)
    : AIWritingAssistanceBase<Rewriter,
                              mojom::blink::AIRewriter,
                              mojom::blink::AIManagerCreateRewriterClient,
                              RewriterCreateCoreOptions,
                              RewriterCreateOptions,
                              RewriterRewriteOptions>(
          execution_context,
          task_runner,
          std::move(pending_remote),
          std::move(options),
          /*echo_whitespace_input=*/true) {}

void Rewriter::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  AIWritingAssistanceBase::Trace(visitor);
}

void Rewriter::remoteExecute(
    const String& input,
    const String& context,
    mojo::PendingRemote<blink::mojom::blink::ModelStreamingResponder>
        responder) {
  remote_->Rewrite(input, context, std::move(responder));
}

ScriptPromise<IDLString> Rewriter::rewrite(
    ScriptState* script_state,
    const String& writing_task,
    const RewriterRewriteOptions* options,
    ExceptionState& exception_state) {
  return AIWritingAssistanceBase::execute(script_state, writing_task, options,
                                          exception_state,
                                          AIMetrics::AIAPI::kRewriterRewrite);
}

ReadableStream* Rewriter::rewriteStreaming(
    ScriptState* script_state,
    const String& writing_task,
    const RewriterRewriteOptions* options,
    ExceptionState& exception_state) {
  return AIWritingAssistanceBase::executeStreaming(
      script_state, writing_task, options, exception_state,
      AIMetrics::AIAPI::kRewriterRewriteStreaming);
}

ScriptPromise<IDLDouble> Rewriter::measureInputUsage(
    ScriptState* script_state,
    const String& writing_task,
    const RewriterRewriteOptions* options,
    ExceptionState& exception_state) {
  return AIWritingAssistanceBase::measureInputUsage(script_state, writing_task,
                                                    options, exception_state);
}

void Rewriter::destroy(ScriptState* script_state,
                       ExceptionState& exception_state) {
  AIWritingAssistanceBase::destroy(script_state, exception_state);
}

}  // namespace blink
