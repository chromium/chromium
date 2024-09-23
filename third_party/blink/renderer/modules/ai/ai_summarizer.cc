// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_summarizer.h"

#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_summarizer.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/modules/ai/model_execution_responder.h"

namespace blink {

AISummarizer::AISummarizer(
    ExecutionContext* context,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    mojo::PendingRemote<mojom::blink::AISummarizer> pending_remote,
    const WTF::String& shared_context,
    V8AISummarizerType type,
    V8AISummarizerFormat format,
    V8AISummarizerLength length)
    : ExecutionContextClient(context),
      task_runner_(task_runner),
      summarizer_remote_(context),
      shared_context_(shared_context),
      type_(type),
      format_(format),
      length_(length) {
  summarizer_remote_.Bind(std::move(pending_remote), task_runner_);
}

void AISummarizer::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(summarizer_remote_);
}

ScriptPromise<IDLString> AISummarizer::summarize(
    ScriptState* script_state,
    const WTF::String& input,
    const AISummarizerSummarizeOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
  }

  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAPIUsageMetricName(AIMetrics::AISessionType::kSummarizer),
      AIMetrics::AIAPI::kSessionSummarize);

  // TODO(crbug.com/356058216): Shall we add separate text size UMAs for
  // summarization
  base::UmaHistogramCounts1M(AIMetrics::GetAISessionRequestSizeMetricName(
                                 AIMetrics::AISessionType::kSummarizer),
                             int(input.CharactersSizeInBytes()));

  if (is_destroyed_) {
    ThrowSessionDestroyedException(exception_state);
    return ScriptPromise<IDLString>();
  }

  AbortSignal* signal = options->getSignalOr(nullptr);
  if (signal && signal->aborted()) {
    ThrowAbortedException(exception_state);
    return ScriptPromise<IDLString>();
  }

  auto [promise, pending_remote] = CreateModelExecutionResponder(
      script_state, signal, task_runner_, AIMetrics::AISessionType::kSummarizer,
      /*complete_callback=*/base::DoNothing());
  summarizer_remote_->Summarize(input, options->getContextOr(WTF::String("")),
                                std::move(pending_remote));
  return promise;
}

ReadableStream* AISummarizer::summarizeStreaming(
    ScriptState* script_state,
    const WTF::String& input,
    const AISummarizerSummarizeOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return nullptr;
  }

  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAPIUsageMetricName(AIMetrics::AISessionType::kSummarizer),
      AIMetrics::AIAPI::kSessionSummarizeStreaming);

  // TODO(crbug.com/356058216): Shall we add separate text size UMAs for
  // summarization
  base::UmaHistogramCounts1M(AIMetrics::GetAISessionRequestSizeMetricName(
                                 AIMetrics::AISessionType::kSummarizer),
                             int(input.CharactersSizeInBytes()));

  if (is_destroyed_) {
    ThrowSessionDestroyedException(exception_state);
    return nullptr;
  }

  AbortSignal* signal = options->getSignalOr(nullptr);
  if (signal && signal->aborted()) {
    ThrowAbortedException(exception_state);
    return nullptr;
  }
  auto [readable_stream, pending_remote] =
      CreateModelExecutionStreamingResponder(
          script_state, signal, task_runner_,
          AIMetrics::AISessionType::kSummarizer,
          /*complete_callback=*/base::DoNothing());
  summarizer_remote_->Summarize(input, options->getContextOr(WTF::String("")),
                                std::move(pending_remote));
  return readable_stream;
}

// TODO(crbug.com/355967885): reset the remote to destroy the session.
void AISummarizer::destroy(ScriptState* script_state,
                           ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return;
  }

  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAPIUsageMetricName(AIMetrics::AISessionType::kSummarizer),
      AIMetrics::AIAPI::kSessionDestroy);

  if (!is_destroyed_) {
    is_destroyed_ = true;
    summarizer_remote_.reset();
  }
}

}  // namespace blink
