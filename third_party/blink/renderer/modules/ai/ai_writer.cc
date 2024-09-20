// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_writer.h"

#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_writer_write_options.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/modules/ai/model_execution_responder.h"

namespace blink {
namespace {

const char kExceptionMessageWriterDestroyed[] =
    "The writer has been destroyed.";

}  // namespace

AIWriter::AIWriter(ExecutionContext* execution_context,
                   scoped_refptr<base::SequencedTaskRunner> task_runner,
                   mojo::PendingRemote<mojom::blink::AIWriter> pending_remote,
                   const String& shared_context_string)
    : ExecutionContextClient(execution_context),
      task_runner_(std::move(task_runner)),
      remote_(execution_context),
      shared_context_string_(shared_context_string) {
  remote_.Bind(std::move(pending_remote), task_runner_);
}

void AIWriter::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(remote_);
}

ScriptPromise<IDLString> AIWriter::write(ScriptState* script_state,
                                         const String& input,
                                         const AIWriterWriteOptions* options,
                                         ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<IDLString>();
  }
  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAPIUsageMetricName(AIMetrics::AISessionType::kWriter),
      AIMetrics::AIAPI::kWriterWrite);
  base::UmaHistogramCounts1M(AIMetrics::GetAISessionRequestSizeMetricName(
                                 AIMetrics::AISessionType::kWriter),
                             int(input.CharactersSizeInBytes()));

  CHECK(options);
  AbortSignal* signal = options->getSignalOr(nullptr);
  if (signal && signal->aborted()) {
    ThrowAbortedException(exception_state);
    return ScriptPromise<IDLString>();
  }
  const String context_string = options->getContextOr(String());

  if (!remote_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kExceptionMessageWriterDestroyed);
    return ScriptPromise<IDLString>();
  }
  auto [promise, pending_remote] = CreateModelExecutionResponder(
      script_state, signal, task_runner_, AIMetrics::AISessionType::kWriter,
      base::DoNothing());
  remote_->Write(input, context_string, std::move(pending_remote));
  return promise;
}

ReadableStream* AIWriter::writeStreaming(ScriptState* script_state,
                                         const String& input,
                                         const AIWriterWriteOptions* options,
                                         ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return nullptr;
  }
  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAPIUsageMetricName(AIMetrics::AISessionType::kWriter),
      AIMetrics::AIAPI::kWriterWriteStreaming);
  base::UmaHistogramCounts1M(AIMetrics::GetAISessionRequestSizeMetricName(
                                 AIMetrics::AISessionType::kWriter),
                             int(input.CharactersSizeInBytes()));
  CHECK(options);
  AbortSignal* signal = options->getSignalOr(nullptr);
  if (signal && signal->aborted()) {
    ThrowAbortedException(exception_state);
    return nullptr;
  }
  const String context_string = options->getContextOr(String());

  if (!remote_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kExceptionMessageWriterDestroyed);
    return nullptr;
  }
  auto [readable_stream, pending_remote] =
      CreateModelExecutionStreamingResponder(script_state, signal, task_runner_,
                                             AIMetrics::AISessionType::kWriter,
                                             base::DoNothing());
  remote_->Write(input, context_string, std::move(pending_remote));
  return readable_stream;
}

void AIWriter::destroy(ScriptState* script_state,
                       ExceptionState& exception_state) {
  remote_.reset();
}

}  // namespace blink
