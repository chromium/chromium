// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/summarizer.h"

#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/ai_writing_assistance_create_client.h"

namespace blink {

template <>
void AIWritingAssistanceCreateClient<
    mojom::blink::AISummarizer,
    mojom::blink::AIManagerCreateSummarizerClient,
    SummarizerCreateOptions,
    Summarizer>::
    RemoteCreate(
        mojo::PendingRemote<mojom::blink::AIManagerCreateSummarizerClient>
            client_remote) {
  HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
      AIInterfaceProxy::GetAIManagerRemote(GetExecutionContext());
  ai_manager_remote->CreateSummarizer(std::move(client_remote),
                                      ToMojoSummarizerCreateOptions(options_));
}

// static
template <>
AIMetrics::AISessionType
AIWritingAssistanceBase<Summarizer,
                        mojom::blink::AISummarizer,
                        mojom::blink::AIManagerCreateSummarizerClient,
                        SummarizerCreateCoreOptions,
                        SummarizerCreateOptions,
                        SummarizerSummarizeOptions>::GetSessionType() {
  return AIMetrics::AISessionType::kSummarizer;
}

// static
template <>
void AIWritingAssistanceBase<Summarizer,
                             mojom::blink::AISummarizer,
                             mojom::blink::AIManagerCreateSummarizerClient,
                             SummarizerCreateCoreOptions,
                             SummarizerCreateOptions,
                             SummarizerSummarizeOptions>::
    RemoteCanCreate(HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote,
                    SummarizerCreateCoreOptions* options,
                    CanCreateCallback callback) {
  ai_manager_remote->CanCreateSummarizer(ToMojoSummarizerCreateOptions(options),
                                         std::move(callback));
}

Summarizer::Summarizer(
    ExecutionContext* execution_context,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    mojo::PendingRemote<mojom::blink::AISummarizer> pending_remote,
    SummarizerCreateOptions* options)
    : AIWritingAssistanceBase<Summarizer,
                              mojom::blink::AISummarizer,
                              mojom::blink::AIManagerCreateSummarizerClient,
                              SummarizerCreateCoreOptions,
                              SummarizerCreateOptions,
                              SummarizerSummarizeOptions>(
          execution_context,
          task_runner,
          std::move(pending_remote),
          std::move(options),
          /*echo_whitespace_input=*/false) {}

void Summarizer::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  AIWritingAssistanceBase::Trace(visitor);
}

void Summarizer::remoteExecute(
    const String& input,
    const String& context,
    mojo::PendingRemote<blink::mojom::blink::ModelStreamingResponder>
        responder) {
  remote_->Summarize(input, context, std::move(responder));
}

ScriptPromise<IDLString> Summarizer::summarize(
    ScriptState* script_state,
    const String& writing_task,
    const SummarizerSummarizeOptions* options,
    ExceptionState& exception_state) {
  return AIWritingAssistanceBase::execute(
      script_state, writing_task, options, exception_state,
      AIMetrics::AIAPI::kSummarizerSummarize);
}

ReadableStream* Summarizer::summarizeStreaming(
    ScriptState* script_state,
    const String& writing_task,
    const SummarizerSummarizeOptions* options,
    ExceptionState& exception_state) {
  return AIWritingAssistanceBase::executeStreaming(
      script_state, writing_task, options, exception_state,
      AIMetrics::AIAPI::kSummarizerSummarizeStreaming);
}

ScriptPromise<IDLDouble> Summarizer::measureInputUsage(
    ScriptState* script_state,
    const String& writing_task,
    const SummarizerSummarizeOptions* options,
    ExceptionState& exception_state) {
  return AIWritingAssistanceBase::measureInputUsage(script_state, writing_task,
                                                    options, exception_state);
}

void Summarizer::destroy(ScriptState* script_state,
                         ExceptionState& exception_state) {
  AIWritingAssistanceBase::destroy(script_state, exception_state);
}

}  // namespace blink
