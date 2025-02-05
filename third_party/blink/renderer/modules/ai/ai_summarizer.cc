// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_summarizer.h"

#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink.h"
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
    AISummarizerCreateOptions* options)
    : ExecutionContextClient(context),
      task_runner_(task_runner),
      summarizer_remote_(context),
      options_(options) {
  summarizer_remote_.Bind(std::move(pending_remote), task_runner_);
}

void AISummarizer::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(summarizer_remote_);
  visitor->Trace(options_);
}

ScriptPromise<IDLString> AISummarizer::summarize(
    ScriptState* script_state,
    const String& input,
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

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(script_state);
  auto promise = resolver->Promise();
  AbortSignal* signal = options->getSignalOr(nullptr);
  if (signal && signal->aborted()) {
    resolver->Reject(signal->reason(script_state));
    return promise;
  }

  String trimmed_input = input.StripWhiteSpace();
  if (trimmed_input.empty()) {
    resolver->Resolve(trimmed_input);
    return promise;
  }

  auto pending_remote = CreateModelExecutionResponder(
      script_state, signal, resolver, task_runner_,
      AIMetrics::AISessionType::kSummarizer,
      /*complete_callback=*/base::DoNothing(),
      /*overflow_callback=*/base::DoNothing());
  summarizer_remote_->Summarize(trimmed_input,
                                options->getContextOr(g_empty_string),
                                std::move(pending_remote));
  return promise;
}

ReadableStream* AISummarizer::summarizeStreaming(
    ScriptState* script_state,
    const String& input,
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
  if (HandleAbortSignal(signal, script_state, exception_state)) {
    return nullptr;
  }

  String trimmed_input = input.StripWhiteSpace();
  if (trimmed_input.empty()) {
    return CreateEmptyReadableStream(script_state,
                                     AIMetrics::AISessionType::kSummarizer);
  }

  auto [readable_stream, pending_remote] =
      CreateModelExecutionStreamingResponder(
          script_state, signal, task_runner_,
          AIMetrics::AISessionType::kSummarizer,
          /*complete_callback=*/base::DoNothing(),
          /*overflow_callback=*/base::DoNothing());
  summarizer_remote_->Summarize(trimmed_input,
                                options->getContextOr(g_empty_string),
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
