// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/rewriter.h"

#include "base/metrics/metrics_hashes.h"
#include "base/strings/strcat.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/ai_writing_assistance_create_client.h"

namespace blink {

using RewriterBase =
    AIWritingAssistanceBase<Rewriter,
                            mojom::blink::AIRewriter,
                            mojom::blink::AIManagerCreateRewriterClient,
                            RewriterCreateCoreOptions,
                            RewriterCreateOptions,
                            RewriterRewriteOptions>;

template <>
void AIWritingAssistanceCreateClient<
    mojom::blink::AIRewriter,
    mojom::blink::AIManagerCreateRewriterClient,
    RewriterCreateOptions,
    Rewriter>::
    RemoteCreate(
        mojo::PendingRemote<mojom::blink::AIManagerCreateRewriterClient>
            client_remote) {
  HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
      AIInterfaceProxy::GetAIManagerRemote(GetExecutionContext());
  ai_manager_remote->CreateRewriter(std::move(client_remote),
                                    ToMojoRewriterCreateOptions(options_));
}

template <>
void AIWritingAssistanceCreateClient<
    mojom::blink::AIRewriter,
    mojom::blink::AIManagerCreateRewriterClient,
    RewriterCreateOptions,
    Rewriter>::RemoteCanCreate(CanCreateCallback callback) {
  HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
      AIInterfaceProxy::GetAIManagerRemote(GetExecutionContext());
  ai_manager_remote->CanCreateRewriter(ToMojoRewriterCreateOptions(options_),
                                       std::move(callback));
}

// static
template <>
AIMetrics::AISessionType RewriterBase::GetSessionType() {
  return AIMetrics::AISessionType::kRewriter;
}

// static
template <>
network::mojom::PermissionsPolicyFeature RewriterBase::GetPermissionsPolicy() {
  return network::mojom::PermissionsPolicyFeature::kRewriter;
}

// static
template <>
void RewriterBase::RemoteCanCreate(
    HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote,
    RewriterCreateCoreOptions* options,
    CanCreateCallback callback) {
  ai_manager_remote->CanCreateRewriter(ToMojoRewriterCreateOptions(options),
                                       std::move(callback));
}

// static
template <>
void RewriterBase::RecordCreateOptionMetrics(
    const RewriterCreateCoreOptions& options,
    std::string function_name) {
  WritingAssistanceMetricsOptionTone tone_metric;
  WritingAssistanceMetricsOptionFormat format_metric;
  WritingAssistanceMetricsOptionLength length_metric;
  switch (options.tone().AsEnum()) {
    case V8RewriterTone::Enum::kAsIs:
      tone_metric = WritingAssistanceMetricsOptionTone::kAsIs;
      break;
    case V8RewriterTone::Enum::kMoreFormal:
      tone_metric = WritingAssistanceMetricsOptionTone::kMoreFormal;
      break;
    case V8RewriterTone::Enum::kMoreCasual:
      tone_metric = WritingAssistanceMetricsOptionTone::kMoreCasual;
      break;
  }
  switch (options.format().AsEnum()) {
    case V8RewriterFormat::Enum::kAsIs:
      format_metric = WritingAssistanceMetricsOptionFormat::kAsIs;
      break;
    case V8RewriterFormat::Enum::kPlainText:
      format_metric = WritingAssistanceMetricsOptionFormat::kPlainText;
      break;
    case V8RewriterFormat::Enum::kMarkdown:
      format_metric = WritingAssistanceMetricsOptionFormat::kMarkdown;
      break;
  }
  switch (options.length().AsEnum()) {
    case V8RewriterLength::Enum::kAsIs:
      length_metric = WritingAssistanceMetricsOptionLength::kAsIs;
      break;
    case V8RewriterLength::Enum::kShorter:
      length_metric = WritingAssistanceMetricsOptionLength::kShorter;
      break;
    case V8RewriterLength::Enum::kLonger:
      length_metric = WritingAssistanceMetricsOptionLength::kLonger;
      break;
  }
  std::string metric_name =
      AIMetrics::GetAIAPIUsageMetricName(RewriterBase::GetSessionType());
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

Rewriter::Rewriter(ScriptState* script_state,
                   scoped_refptr<base::SequencedTaskRunner> task_runner,
                   mojo::PendingRemote<mojom::blink::AIRewriter> pending_remote,
                   RewriterCreateOptions* options)
    : AIWritingAssistanceBase<Rewriter,
                              mojom::blink::AIRewriter,
                              mojom::blink::AIManagerCreateRewriterClient,
                              RewriterCreateCoreOptions,
                              RewriterCreateOptions,
                              RewriterRewriteOptions>(
          script_state,
          task_runner,
          std::move(pending_remote),
          std::move(options),
          /*echo_whitespace_input=*/true) {}

void Rewriter::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  AIWritingAssistanceBase::Trace(visitor);
}

void Rewriter::remoteExecute(
    const String& input,
    const String& context,
    mojo::PendingRemote<blink::mojom::blink::ModelStreamingResponder>
        responder) {
  remote_->Rewrite(input, context, std::move(responder));
}

ScriptPromise<IDLString> Rewriter::rewrite(
    ScriptState* script_state,
    const String& writing_task,
    const RewriterRewriteOptions* options,
    ExceptionState& exception_state) {
  return AIWritingAssistanceBase::execute(script_state, writing_task, options,
                                          exception_state);
}

ReadableStream* Rewriter::rewriteStreaming(
    ScriptState* script_state,
    const String& writing_task,
    const RewriterRewriteOptions* options,
    ExceptionState& exception_state) {
  return AIWritingAssistanceBase::executeStreaming(script_state, writing_task,
                                                   options, exception_state);
}

ScriptPromise<IDLDouble> Rewriter::measureInputUsage(
    ScriptState* script_state,
    const String& writing_task,
    const RewriterRewriteOptions* options,
    ExceptionState& exception_state) {
  return AIWritingAssistanceBase::measureInputUsage(script_state, writing_task,
                                                    options, exception_state);
}

void Rewriter::destroy(ScriptState* script_state,
                       ExceptionState& exception_state) {
  AIWritingAssistanceBase::destroy(script_state, exception_state);
}

}  // namespace blink
