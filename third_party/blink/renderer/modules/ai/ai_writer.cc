// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_writer.h"

#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/ai_writing_assistance_create_client.h"

namespace blink {

template <>
void AIWritingAssistanceCreateClient<mojom::blink::AIWriter,
                                     mojom::blink::AIManagerCreateWriterClient,
                                     AIWriterCreateOptions,
                                     AIWriter>::
    RemoteCreate(mojo::PendingRemote<mojom::blink::AIManagerCreateWriterClient>
                     client_remote) {
  HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
      AIInterfaceProxy::GetAIManagerRemote(GetExecutionContext());
  ai_manager_remote->CreateWriter(std::move(client_remote),
                                  ToMojoWriterCreateOptions(options_));
}

// static
template <>
AIMetrics::AISessionType
AIWritingAssistanceBase<AIWriter,
                        mojom::blink::AIWriter,
                        mojom::blink::AIManagerCreateWriterClient,
                        AIWriterCreateCoreOptions,
                        AIWriterCreateOptions,
                        AIWriterWriteOptions>::GetSessionType() {
  return AIMetrics::AISessionType::kWriter;
}

// static
template <>
void AIWritingAssistanceBase<AIWriter,
                             mojom::blink::AIWriter,
                             mojom::blink::AIManagerCreateWriterClient,
                             AIWriterCreateCoreOptions,
                             AIWriterCreateOptions,
                             AIWriterWriteOptions>::
    RemoteCanCreate(HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote,
                    AIWriterCreateCoreOptions* options,
                    CanCreateCallback callback) {
  ai_manager_remote->CanCreateWriter(ToMojoWriterCreateOptions(options),
                                     std::move(callback));
}

AIWriter::AIWriter(ExecutionContext* execution_context,
                   scoped_refptr<base::SequencedTaskRunner> task_runner,
                   mojo::PendingRemote<mojom::blink::AIWriter> pending_remote,
                   AIWriterCreateOptions* options)
    : AIWritingAssistanceBase<AIWriter,
                              mojom::blink::AIWriter,
                              mojom::blink::AIManagerCreateWriterClient,
                              AIWriterCreateCoreOptions,
                              AIWriterCreateOptions,
                              AIWriterWriteOptions>(
          execution_context,
          task_runner,
          std::move(pending_remote),
          std::move(options),
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
