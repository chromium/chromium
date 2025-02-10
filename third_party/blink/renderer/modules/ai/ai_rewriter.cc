// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_rewriter.h"

#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_rewriter_rewrite_options.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/modules/ai/ai.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/modules/ai/model_execution_responder.h"

namespace blink {
namespace {

const char kExceptionMessageRewriterDestroyed[] =
    "The rewriter has been destroyed.";

}  // namespace

AIRewriter::AIRewriter(
    ExecutionContext* execution_context,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    mojo::PendingRemote<mojom::blink::AIRewriter> pending_remote,
    AIRewriterCreateOptions* options)
    : ExecutionContextClient(execution_context),
      task_runner_(std::move(task_runner)),
      remote_(execution_context),
      options_(options) {
  remote_.Bind(std::move(pending_remote), task_runner_);
}

void AIRewriter::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(remote_);
  visitor->Trace(options_);
}

ScriptPromise<IDLString> AIRewriter::rewrite(
    ScriptState* script_state,
    const String& input,
    const AIRewriterRewriteOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<IDLString>();
  }
  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAPIUsageMetricName(AIMetrics::AISessionType::kRewriter),
      AIMetrics::AIAPI::kRewriterRewrite);
  base::UmaHistogramCounts1M(AIMetrics::GetAISessionRequestSizeMetricName(
                                 AIMetrics::AISessionType::kRewriter),
                             int(input.CharactersSizeInBytes()));
  CHECK(options);
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(script_state);
  auto promise = resolver->Promise();

  AbortSignal* signal = options->getSignalOr(nullptr);
  if (signal && signal->aborted()) {
    resolver->Reject(signal->reason(script_state));
    return promise;
  }

  if (!remote_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kExceptionMessageRewriterDestroyed);
    return promise;
  }

  String trimmed_input = input.StripWhiteSpace();
  if (trimmed_input.empty()) {
    // Echo input consisting of only whitespace, unlike Writer or Summarizer.
    resolver->Resolve(input);
    return promise;
  }

  const String trimmed_context =
      options->getContextOr(g_empty_string).StripWhiteSpace();
  auto pending_remote = CreateModelExecutionResponder(
      script_state, signal, resolver, task_runner_,
      AIMetrics::AISessionType::kRewriter,
      /*complete_callback=*/base::DoNothing(),
      /*overflow_callback=*/base::DoNothing());
  remote_->Rewrite(trimmed_input, trimmed_context, std::move(pending_remote));
  return promise;
}

ReadableStream* AIRewriter::rewriteStreaming(
    ScriptState* script_state,
    const String& input,
    const AIRewriterRewriteOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return nullptr;
  }
  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAPIUsageMetricName(AIMetrics::AISessionType::kRewriter),
      AIMetrics::AIAPI::kRewriterRewriteStreaming);
  base::UmaHistogramCounts1M(AIMetrics::GetAISessionRequestSizeMetricName(
                                 AIMetrics::AISessionType::kRewriter),
                             int(input.CharactersSizeInBytes()));
  CHECK(options);
  AbortSignal* signal = options->getSignalOr(nullptr);
  if (HandleAbortSignal(signal, script_state, exception_state)) {
    return nullptr;
  }

  if (!remote_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kExceptionMessageRewriterDestroyed);
    return nullptr;
  }

  String trimmed_input = input.StripWhiteSpace();
  if (trimmed_input.empty()) {
    return CreateEmptyReadableStream(script_state,
                                     AIMetrics::AISessionType::kRewriter);
  }

  const String trimmed_context =
      options->getContextOr(g_empty_string).StripWhiteSpace();
  auto [readable_stream, pending_remote] =
      CreateModelExecutionStreamingResponder(
          script_state, signal, task_runner_,
          AIMetrics::AISessionType::kRewriter,
          /*complete_callback=*/base::DoNothing(),
          /*overflow_callback=*/base::DoNothing());
  remote_->Rewrite(trimmed_input, trimmed_context, std::move(pending_remote));
  return readable_stream;
}

void AIRewriter::destroy(ScriptState* script_state,
                         ExceptionState& exception_state) {
  remote_.reset();
}

}  // namespace blink
