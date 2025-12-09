// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/summarizer.h"

#include "base/metrics/metrics_hashes.h"
#include "base/strings/strcat.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/ai_writing_assistance_create_client.h"

namespace blink {

using SummarizerBase =
    AIWritingAssistanceBase<Summarizer,
                            mojom::blink::AISummarizer,
                            mojom::blink::AIManagerCreateSummarizerClient,
                            SummarizerCreateCoreOptions,
                            SummarizerCreateOptions,
                            SummarizerSummarizeOptions>;

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

template <>
void AIWritingAssistanceCreateClient<
    mojom::blink::AISummarizer,
    mojom::blink::AIManagerCreateSummarizerClient,
    SummarizerCreateOptions,
    Summarizer>::RemoteCanCreate(CanCreateCallback callback) {
  HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
      AIInterfaceProxy::GetAIManagerRemote(GetExecutionContext());
  ai_manager_remote->CanCreateSummarizer(
      ToMojoSummarizerCreateOptions(options_), std::move(callback));
}

// static
template <>
AIMetrics::AISessionType SummarizerBase::GetSessionType() {
  return AIMetrics::AISessionType::kSummarizer;
}

// static
template <>
network::mojom::PermissionsPolicyFeature
SummarizerBase::GetPermissionsPolicy() {
  return network::mojom::PermissionsPolicyFeature::kSummarizer;
}

// static
template <>
void SummarizerBase::RemoteCanCreate(
    HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote,
    SummarizerCreateCoreOptions* options,
    CanCreateCallback callback) {
  ai_manager_remote->CanCreateSummarizer(ToMojoSummarizerCreateOptions(options),
                                         std::move(callback));
}

// static
template <>
void SummarizerBase::RecordCreateOptionMetrics(
    const SummarizerCreateCoreOptions& options,
    std::string function_name) {
  WritingAssistanceMetricsOptionType type_metric;
  WritingAssistanceMetricsOptionFormat format_metric;
  WritingAssistanceMetricsOptionLength length_metric;
  switch (options.type().AsEnum()) {
    case V8SummarizerType::Enum::kTldr:
      type_metric = WritingAssistanceMetricsOptionType::kTldr;
      break;
    case V8SummarizerType::Enum::kKeyPoints:
      type_metric = WritingAssistanceMetricsOptionType::kKeyPoints;
      break;
    case V8SummarizerType::Enum::kTeaser:
      type_metric = WritingAssistanceMetricsOptionType::kTeaser;
      break;
    case V8SummarizerType::Enum::kHeadline:
      type_metric = WritingAssistanceMetricsOptionType::kHeadline;
      break;
  }
  switch (options.format().AsEnum()) {
    case V8SummarizerFormat::Enum::kPlainText:
      format_metric = WritingAssistanceMetricsOptionFormat::kPlainText;
      break;
    case V8SummarizerFormat::Enum::kMarkdown:
      format_metric = WritingAssistanceMetricsOptionFormat::kMarkdown;
      break;
  }
  switch (options.length().AsEnum()) {
    case V8SummarizerLength::Enum::kShort:
      length_metric = WritingAssistanceMetricsOptionLength::kShort;
      break;
    case V8SummarizerLength::Enum::kMedium:
      length_metric = WritingAssistanceMetricsOptionLength::kMedium;
      break;
    case V8SummarizerLength::Enum::kLong:
      length_metric = WritingAssistanceMetricsOptionLength::kLong;
      break;
  }
  std::string metric_name =
      AIMetrics::GetAIAPIUsageMetricName(SummarizerBase::GetSessionType());
  base::UmaHistogramEnumeration(
      base::StrCat({metric_name, ".", function_name, ".CoreOptionType"}),
      type_metric);
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

Summarizer::Summarizer(
    ScriptState* script_state,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    mojo::PendingRemote<mojom::blink::AISummarizer> pending_remote,
    SummarizerCreateOptions* options)
    : AIWritingAssistanceBase<Summarizer,
                              mojom::blink::AISummarizer,
                              mojom::blink::AIManagerCreateSummarizerClient,
                              SummarizerCreateCoreOptions,
                              SummarizerCreateOptions,
                              SummarizerSummarizeOptions>(
          script_state,
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
  return AIWritingAssistanceBase::execute(script_state, writing_task, options,
                                          exception_state);
}

ReadableStream* Summarizer::summarizeStreaming(
    ScriptState* script_state,
    const String& writing_task,
    const SummarizerSummarizeOptions* options,
    ExceptionState& exception_state) {
  return AIWritingAssistanceBase::executeStreaming(script_state, writing_task,
                                                   options, exception_state);
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
