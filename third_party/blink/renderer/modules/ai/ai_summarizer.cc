// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_summarizer.h"

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace {

// TODO(crbug.com/351745455): Support length options
constexpr char kSummarizePrompt[] = R"(
You are an assistant that summarizes text. The summary must be accurate and fit within one short paragraph.
TEXT: %s
SUMMARY: )";

}  // namespace

namespace blink {

WTF::String BuildPromptInput(const WTF::String& summarize_input) {
  WTF::StringBuilder builder;
  builder.AppendFormat(kSummarizePrompt, summarize_input.Utf8().c_str());
  return builder.ReleaseString();
}

AISummarizer::AISummarizer(ExecutionContext* context,
                           AITextSession* text_session,
                           scoped_refptr<base::SequencedTaskRunner> task_runner)
    : ExecutionContextClient(context),
      text_session_(text_session),
      task_runner_(task_runner) {}

void AISummarizer::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(text_session_);
}

ScriptPromise<IDLString> AISummarizer::summarize(
    ScriptState* script_state,
    const WTF::String& input,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
  }

  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAPIUsageMetricName(AIMetrics::AISessionType::kText),
      AIMetrics::AIAPI::kSessionSummarize);

  // TODO(crbug.com/356058216): Shall we add separate text size UMAs for
  // summarization
  base::UmaHistogramCounts1M(AIMetrics::GetAISessionRequestSizeMetricName(
                                 AIMetrics::AISessionType::kText),
                             int(input.CharactersSizeInBytes()));

  AITextSession::Responder* responder =
      MakeGarbageCollected<AITextSession::Responder>(script_state);

  if (!text_session_) {
    ThrowSessionDestroyedException(exception_state);
    return ScriptPromise<IDLString>();
  } else {
    text_session_->GetRemoteTextSession()->Prompt(
        BuildPromptInput(input),
        responder->BindNewPipeAndPassRemote(task_runner_));
  }

  return responder->GetResolver()->Promise();
}

ReadableStream* AISummarizer::summarizeStreaming(
    ScriptState* script_state,
    const WTF::String& input,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return nullptr;
  }

  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAPIUsageMetricName(AIMetrics::AISessionType::kText),
      AIMetrics::AIAPI::kSessionSummarizeStreaming);

  // TODO(crbug.com/356058216): Shall we add separate text size UMAs for
  // summarization
  base::UmaHistogramCounts1M(AIMetrics::GetAISessionRequestSizeMetricName(
                                 AIMetrics::AISessionType::kText),
                             int(input.CharactersSizeInBytes()));

  AITextSession::StreamingResponder* streaming_responder =
      MakeGarbageCollected<AITextSession::StreamingResponder>(script_state);

  if (!text_session_) {
    ThrowSessionDestroyedException(exception_state);
    return nullptr;
  }

  text_session_->GetRemoteTextSession()->Prompt(
      BuildPromptInput(input),
      streaming_responder->BindNewPipeAndPassRemote(task_runner_));

  // Set the high water mark to 1 so the backpressure will be applied on
  // every enqueue.
  return ReadableStream::CreateWithCountQueueingStrategy(
      script_state, streaming_responder, 1);
}

void AISummarizer::destroy(ScriptState* script_state,
                           ExceptionState& exception_state) {
  text_session_->destroy(script_state, exception_state);
  text_session_ = nullptr;
}

}  // namespace blink
