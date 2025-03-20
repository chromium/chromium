// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_writer.h"

#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_writer_write_options.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"

namespace blink {

AIWriter::AIWriter(ExecutionContext* execution_context,
                   scoped_refptr<base::SequencedTaskRunner> task_runner,
                   mojo::PendingRemote<mojom::blink::AIWriter> pending_remote,
                   AIWriterCreateOptions* options)
    : AIWritingAssistanceBase<mojom::blink::AIWriter,
                              AIWriterCreateOptions,
                              AIWriterWriteOptions>(
          execution_context,
          task_runner,
          std::move(pending_remote),
          std::move(options),
          AIMetrics::AISessionType::kWriter,
          /*echo_whitespace_input=*/false) {}

void AIWriter::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  AIWritingAssistanceBase::Trace(visitor);
}

void AIWriter::remoteExecute(
    const String& input,
    const String& context,
    mojo::PendingRemote<blink::mojom::blink::ModelStreamingResponder>
        responder) {
  remote_->Write(input, context, std::move(responder));
}

ScriptPromise<IDLString> AIWriter::write(ScriptState* script_state,
                                         const String& writing_task,
                                         const AIWriterWriteOptions* options,
                                         ExceptionState& exception_state) {
  return AIWritingAssistanceBase::execute(script_state, writing_task, options,
                                          exception_state,
                                          AIMetrics::AIAPI::kWriterWrite);
}

ReadableStream* AIWriter::writeStreaming(ScriptState* script_state,
                                         const String& writing_task,
                                         const AIWriterWriteOptions* options,
                                         ExceptionState& exception_state) {
  return AIWritingAssistanceBase::executeStreaming(
      script_state, writing_task, options, exception_state,
      AIMetrics::AIAPI::kWriterWriteStreaming);
}

ScriptPromise<IDLDouble> AIWriter::measureInputUsage(
    ScriptState* script_state,
    const String& writing_task,
    const AIWriterWriteOptions* options,
    ExceptionState& exception_state) {
  return AIWritingAssistanceBase::measureInputUsage(script_state, writing_task,
                                                    options, exception_state);
}

void AIWriter::destroy(ScriptState* script_state,
                       ExceptionState& exception_state) {
  AIWritingAssistanceBase::destroy(script_state, exception_state);
}

}  // namespace blink
