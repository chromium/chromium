// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/writer.h"

#include "base/metrics/metrics_hashes.h"
#include "base/strings/strcat.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/ai_writing_assistance_create_client.h"

namespace blink {

using WriterBase =
    AIWritingAssistanceBase<Writer,
                            mojom::blink::AIWriter,
                            mojom::blink::AIManagerCreateWriterClient,
                            WriterCreateCoreOptions,
                            WriterCreateOptions,
                            WriterWriteOptions>;

template <>
void AIWritingAssistanceCreateClient<mojom::blink::AIWriter,
                                     mojom::blink::AIManagerCreateWriterClient,
                                     WriterCreateOptions,
                                     Writer>::
    RemoteCreate(mojo::PendingRemote<mojom::blink::AIManagerCreateWriterClient>
                     client_remote) {
  HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
      AIInterfaceProxy::GetAIManagerRemote(GetExecutionContext());
  ai_manager_remote->CreateWriter(std::move(client_remote),
                                  ToMojoWriterCreateOptions(options_));
}

template <>
void AIWritingAssistanceCreateClient<mojom::blink::AIWriter,
                                     mojom::blink::AIManagerCreateWriterClient,
                                     WriterCreateOptions,
                                     Writer>::RemoteCanCreate(CanCreateCallback
                                                                  callback) {
  HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
      AIInterfaceProxy::GetAIManagerRemote(GetExecutionContext());
  ai_manager_remote->CanCreateWriter(ToMojoWriterCreateOptions(options_),
                                     std::move(callback));
}

// static
template <>
AIMetrics::AISessionType WriterBase::GetSessionType() {
  return AIMetrics::AISessionType::kWriter;
}

// static
template <>
network::mojom::PermissionsPolicyFeature WriterBase::GetPermissionsPolicy() {
  return network::mojom::PermissionsPolicyFeature::kWriter;
}

// static
template <>
void WriterBase::RemoteCanCreate(
    HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote,
    WriterCreateCoreOptions* options,
    CanCreateCallback callback) {
  ai_manager_remote->CanCreateWriter(ToMojoWriterCreateOptions(options),
                                     std::move(callback));
}

// static
template <>
void WriterBase::RecordCreateOptionMetrics(
    const WriterCreateCoreOptions& options,
    std::string function_name) {
  WritingAssistanceMetricsOptionTone tone_metric;
  WritingAssistanceMetricsOptionFormat format_metric;
  WritingAssistanceMetricsOptionLength length_metric;
  switch (options.tone().AsEnum()) {
    case V8WriterTone::Enum::kFormal:
      tone_metric = WritingAssistanceMetricsOptionTone::kFormal;
      break;
    case V8WriterTone::Enum::kNeutral:
      tone_metric = WritingAssistanceMetricsOptionTone::kNeutral;
      break;
    case V8WriterTone::Enum::kCasual:
      tone_metric = WritingAssistanceMetricsOptionTone::kCasual;
      break;
  }
  switch (options.format().AsEnum()) {
    case V8WriterFormat::Enum::kPlainText:
      format_metric = WritingAssistanceMetricsOptionFormat::kPlainText;
      break;
    case V8WriterFormat::Enum::kMarkdown:
      format_metric = WritingAssistanceMetricsOptionFormat::kMarkdown;
      break;
  }
  switch (options.length().AsEnum()) {
    case V8WriterLength::Enum::kShort:
      length_metric = WritingAssistanceMetricsOptionLength::kShort;
      break;
    case V8WriterLength::Enum::kMedium:
      length_metric = WritingAssistanceMetricsOptionLength::kMedium;
      break;
    case V8WriterLength::Enum::kLong:
      length_metric = WritingAssistanceMetricsOptionLength::kLong;
      break;
  }
  std::string metric_name =
      AIMetrics::GetAIAPIUsageMetricName(WriterBase::GetSessionType());
  base::UmaHistogramEnumeration(
      base::StrCat({metric_name, ".", function_name, ".CoreOptionTone"}),
      tone_metric);
  base::UmaHistogramEnumeration(
      base::StrCat({metric_name, ".", function_name, ".CoreOptionFormat"}),
      format_metric);
  base::UmaHistogramEnumeration(
      base::StrCat({metric_name, ".", function_name, ".CoreOptionLength"}),
      length_metric);

  // expectedContextLanguages and expectedInputLanguages and outputLanguage
  // should be canonicalized. See ValidateAndCanonicalizeBCP47Language.
  if (options.hasExpectedContextLanguages()) {
    for (const auto& lang : options.expectedContextLanguages()) {
      base::UmaHistogramSparse(base::StrCat({metric_name, ".", function_name,
                                             ".ExpectedContextLanguage"}),
                               static_cast<base::HistogramBase::Sample32>(
                                   base::HashMetricName(lang.Ascii())));
    }
  }
  if (options.hasExpectedInputLanguages()) {
    for (const auto& lang : options.expectedInputLanguages()) {
      base::UmaHistogramSparse(base::StrCat({metric_name, ".", function_name,
                                             ".ExpectedInputLanguage"}),
                               static_cast<base::HistogramBase::Sample32>(
                                   base::HashMetricName(lang.Ascii())));
    }
  }
  if (options.hasOutputLanguage()) {
    base::UmaHistogramSparse(
        base::StrCat({metric_name, ".", function_name, ".OutputLanguage"}),
        static_cast<base::HistogramBase::Sample32>(
            base::HashMetricName(options.outputLanguage().Ascii())));
  }
}

Writer::Writer(ScriptState* script_state,
               scoped_refptr<base::SequencedTaskRunner> task_runner,
               mojo::PendingRemote<mojom::blink::AIWriter> pending_remote,
               WriterCreateOptions* options)
    : AIWritingAssistanceBase<Writer,
                              mojom::blink::AIWriter,
                              mojom::blink::AIManagerCreateWriterClient,
                              WriterCreateCoreOptions,
                              WriterCreateOptions,
                              WriterWriteOptions>(
          script_state,
          task_runner,
          std::move(pending_remote),
          std::move(options),
          /*echo_whitespace_input=*/false) {}

void Writer::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  AIWritingAssistanceBase::Trace(visitor);
}

void Writer::remoteExecute(
    const String& input,
    const String& context,
    mojo::PendingRemote<blink::mojom::blink::ModelStreamingResponder>
        responder) {
  remote_->Write(input, context, std::move(responder));
}

ScriptPromise<IDLString> Writer::write(ScriptState* script_state,
                                       const String& writing_task,
                                       const WriterWriteOptions* options,
                                       ExceptionState& exception_state) {
  return AIWritingAssistanceBase::execute(script_state, writing_task, options,
                                          exception_state);
}

ReadableStream* Writer::writeStreaming(ScriptState* script_state,
                                       const String& writing_task,
                                       const WriterWriteOptions* options,
                                       ExceptionState& exception_state) {
  return AIWritingAssistanceBase::executeStreaming(script_state, writing_task,
                                                   options, exception_state);
}

ScriptPromise<IDLDouble> Writer::measureInputUsage(
    ScriptState* script_state,
    const String& writing_task,
    const WriterWriteOptions* options,
    ExceptionState& exception_state) {
  return AIWritingAssistanceBase::measureInputUsage(script_state, writing_task,
                                                    options, exception_state);
}

void Writer::destroy(ScriptState* script_state,
                     ExceptionState& exception_state) {
  AIWritingAssistanceBase::destroy(script_state, exception_state);
}

}  // namespace blink
