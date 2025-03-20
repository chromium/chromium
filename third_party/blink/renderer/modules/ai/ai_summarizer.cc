// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_summarizer.h"

#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_summarizer_summarize_options.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"

namespace blink {

AISummarizer::AISummarizer(
    ExecutionContext* execution_context,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    mojo::PendingRemote<mojom::blink::AISummarizer> pending_remote,
    AISummarizerCreateOptions* options)
    : AIWritingAssistanceBase<mojom::blink::AISummarizer,
                              AISummarizerCreateOptions,
                              AISummarizerSummarizeOptions>(
          execution_context,
          task_runner,
          std::move(pending_remote),
          std::move(options),
          AIMetrics::AISessionType::kSummarizer,
          /*echo_whitespace_input=*/false) {}

void AISummarizer::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  AIWritingAssistanceBase::Trace(visitor);
}

void AISummarizer::remoteExecute(
    const String& input,
    const String& context,
    mojo::PendingRemote<blink::mojom::blink::ModelStreamingResponder>
        responder) {
  remote_->Summarize(input, context, std::move(responder));
}

ScriptPromise<IDLString> AISummarizer::summarize(
    ScriptState* script_state,
    const String& writing_task,
    const AISummarizerSummarizeOptions* options,
    ExceptionState& exception_state) {
  return AIWritingAssistanceBase::execute(
      script_state, writing_task, options, exception_state,
      AIMetrics::AIAPI::kSummarizerSummarize);
}

ReadableStream* AISummarizer::summarizeStreaming(
    ScriptState* script_state,
    const String& writing_task,
    const AISummarizerSummarizeOptions* options,
    ExceptionState& exception_state) {
  return AIWritingAssistanceBase::executeStreaming(
      script_state, writing_task, options, exception_state,
      AIMetrics::AIAPI::kSummarizerSummarizeStreaming);
}

ScriptPromise<IDLDouble> AISummarizer::measureInputUsage(
    ScriptState* script_state,
    const String& writing_task,
    const AISummarizerSummarizeOptions* options,
    ExceptionState& exception_state) {
  return AIWritingAssistanceBase::measureInputUsage(script_state, writing_task,
                                                    options, exception_state);
}

void AISummarizer::destroy(ScriptState* script_state,
                           ExceptionState& exception_state) {
  AIWritingAssistanceBase::destroy(script_state, exception_state);
}

}  // namespace blink
