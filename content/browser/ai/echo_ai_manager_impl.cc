// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ai/echo_ai_manager_impl.h"

#include "base/no_destructor.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "components/language/core/common/locale_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "content/browser/ai/echo_ai_language_model.h"
#include "content/browser/ai/echo_ai_rewriter.h"
#include "content/browser/ai/echo_ai_summarizer.h"
#include "content/browser/ai/echo_ai_writer.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-forward.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-shared.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom.h"

namespace content {

namespace {

const int kMockDownloadPreparationTimeMillisecond = 300;
const int kMockModelSizeBytes = 0x10000;

using blink::mojom::AILanguageCodePtr;

// TODO(crbug.com/394109104): This is duplicated from chrome AIManager in order
// to keep the consistent wpt results run from CQ, which currently only supports
// running wpt_internal/ tests on content_shell, using content EchoAIManager.
// If there is enough divergence in two AI Managers' code, it should be
// refactored to share the common code or use subclasses.
auto is_language_supported = [](const AILanguageCodePtr& language) {
  return language->code.empty() ||
         language::ExtractBaseLanguage(language->code) == "en";
};

bool IsLanguagesSupported(const std::vector<AILanguageCodePtr>& languages) {
  return std::ranges::all_of(languages, is_language_supported);
}

bool SupportedLanguages(const std::vector<AILanguageCodePtr>& input,
                        const std::vector<AILanguageCodePtr>& context,
                        const AILanguageCodePtr& output) {
  return IsLanguagesSupported(input) && IsLanguagesSupported(context) &&
         is_language_supported(output);
}

// Returns whether optional LanguageModel expected_inputs or expected_outputs
// vectors contain only supported languages. Returns true for absent languages.
bool AreExpectedLanguagesSupported(
    const std::optional<std::vector<blink::mojom::AILanguageModelExpectedPtr>>&
        expected_vector) {
  if (!expected_vector) {
    return true;
  }
  for (const auto& expected_entry : expected_vector.value()) {
    if (expected_entry->languages.has_value() &&
        !IsLanguagesSupported(expected_entry->languages.value())) {
      return false;
    }
  }
  return true;
}

// Returns whether `options` contains any unsupported AILanguageModelPromptType.
bool HasUnsupportedType(
    const blink::mojom::AILanguageModelCreateOptionsPtr& options) {
  bool has_unsupported_type = false;
  if (options) {
    if (options->expected_inputs.has_value()) {
      for (const auto& expected_input : options->expected_inputs.value()) {
        has_unsupported_type |=
            expected_input->type !=
                blink::mojom::AILanguageModelPromptType::kText &&
            !base::FeatureList::IsEnabled(
                blink::features::kAIPromptAPIMultimodalInput);
      }
    }
    if (options->expected_outputs.has_value()) {
      for (const auto& expected_output : options->expected_outputs.value()) {
        has_unsupported_type |= expected_output->type !=
                                blink::mojom::AILanguageModelPromptType::kText;
      }
    }
  }
  return has_unsupported_type;
}

}  // namespace

EchoAIManagerImpl::EchoAIManagerImpl() = default;

EchoAIManagerImpl::~EchoAIManagerImpl() = default;

// static
void EchoAIManagerImpl::Create(
    mojo::PendingReceiver<blink::mojom::AIManager> receiver) {
  static base::NoDestructor<EchoAIManagerImpl> ai;
  ai->receivers_.Add(ai.get(), std::move(receiver));
}

void EchoAIManagerImpl::CanCreateLanguageModel(
    blink::mojom::AILanguageModelCreateOptionsPtr options,
    CanCreateLanguageModelCallback callback) {
  if (HasUnsupportedType(options)) {
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableModelAdaptationNotAvailable);
    return;
  }
  if (options && (!AreExpectedLanguagesSupported(options->expected_inputs) ||
                  !AreExpectedLanguagesSupported(options->expected_outputs))) {
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableUnsupportedLanguage);
    return;
  }

  std::move(callback).Run(
      blink::mojom::ModelAvailabilityCheckResult::kDownloadable);
}

void EchoAIManagerImpl::CreateLanguageModel(
    mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
        client,
    blink::mojom::AILanguageModelCreateOptionsPtr options) {
  mojo::Remote<blink::mojom::AIManagerCreateLanguageModelClient> client_remote(
      std::move(client));

  size_t initial_size = 0;
  for (const auto& initial_prompt : options->initial_prompts) {
    for (const auto& content : initial_prompt->content) {
      if (content->is_text()) {
        initial_size += content->get_text().size();
      } else {
        initial_size += 100;  // TODO(crbug.com/415304330): Improve estimate.
      }
      if (initial_size > kMaxContextSizeInTokens) {
        client_remote->OnError(
            blink::mojom::AIManagerCreateClientError::kInitialInputTooLarge,
            blink::mojom::QuotaErrorInfo::New(initial_size,
                                              kMaxContextSizeInTokens));
        return;
      }
    }
  }

  if (HasUnsupportedType(options)) {
    client_remote->OnError(
        blink::mojom::AIManagerCreateClientError::kUnableToCreateSession,
        /*quota_error_info=*/nullptr);
    return;
  }
  if (options && (!AreExpectedLanguagesSupported(options->expected_inputs) ||
                  !AreExpectedLanguagesSupported(options->expected_outputs))) {
    client_remote->OnError(
        blink::mojom::AIManagerCreateClientError::kUnsupportedLanguage,
        /*quota_error_info=*/nullptr);
    return;
  }
  base::flat_set<blink::mojom::AILanguageModelPromptType> enabled_input_types;
  if (options->expected_inputs.has_value()) {
    for (const auto& expected_input : options->expected_inputs.value()) {
      enabled_input_types.insert(expected_input->type);
    }
  }

  auto return_language_model_callback =
      base::BindOnce(&EchoAIManagerImpl::ReturnAILanguageModelCreationResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(client_remote),
                     std::move(options->sampling_params), enabled_input_types);

  // In order to test the model download progress handling, the
  // `EchoAIManagerImpl` will always start from the `after-download` state, and
  // we simulate the downloading time by posting a delayed task.
  content::GetUIThreadTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&EchoAIManagerImpl::DoMockDownloadingAndReturn,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(return_language_model_callback)),
      base::Milliseconds(kMockDownloadPreparationTimeMillisecond));
}

void EchoAIManagerImpl::CanCreateSummarizer(
    blink::mojom::AISummarizerCreateOptionsPtr options,
    CanCreateSummarizerCallback callback) {
  CanCreateWritingAssistanceClient<blink::mojom::AISummarizerCreateOptionsPtr,
                                   CanCreateSummarizerCallback>(
      std::move(options), std::move(callback));
}

void EchoAIManagerImpl::CreateSummarizer(
    mojo::PendingRemote<blink::mojom::AIManagerCreateSummarizerClient> client,
    blink::mojom::AISummarizerCreateOptionsPtr options) {
  CreateWritingAssistanceClient<blink::mojom::AISummarizerCreateOptionsPtr,
                                blink::mojom::AIManagerCreateSummarizerClient,
                                blink::mojom::AISummarizer, EchoAISummarizer>(
      std::move(client), std::move(options));
}

void EchoAIManagerImpl::GetLanguageModelParams(
    GetLanguageModelParamsCallback callback) {
  std::move(callback).Run(blink::mojom::AILanguageModelParams::New(
      blink::mojom::AILanguageModelSamplingParams::New(
          optimization_guide::features::GetOnDeviceModelDefaultTopK(),
          optimization_guide::features::GetOnDeviceModelDefaultTemperature()),
      blink::mojom::AILanguageModelSamplingParams::New(
          optimization_guide::features::GetOnDeviceModelMaxTopK(),
          /*temperature=*/2.0f)));
}

void EchoAIManagerImpl::CanCreateWriter(
    blink::mojom::AIWriterCreateOptionsPtr options,
    CanCreateWriterCallback callback) {
  CanCreateWritingAssistanceClient<blink::mojom::AIWriterCreateOptionsPtr,
                                   CanCreateWriterCallback>(
      std::move(options), std::move(callback));
}

void EchoAIManagerImpl::CreateWriter(
    mojo::PendingRemote<blink::mojom::AIManagerCreateWriterClient> client,
    blink::mojom::AIWriterCreateOptionsPtr options) {
  CreateWritingAssistanceClient<blink::mojom::AIWriterCreateOptionsPtr,
                                blink::mojom::AIManagerCreateWriterClient,
                                blink::mojom::AIWriter, EchoAIWriter>(
      std::move(client), std::move(options));
}

void EchoAIManagerImpl::CanCreateRewriter(
    blink::mojom::AIRewriterCreateOptionsPtr options,
    CanCreateRewriterCallback callback) {
  CanCreateWritingAssistanceClient<blink::mojom::AIRewriterCreateOptionsPtr,
                                   CanCreateRewriterCallback>(
      std::move(options), std::move(callback));
}

void EchoAIManagerImpl::CreateRewriter(
    mojo::PendingRemote<blink::mojom::AIManagerCreateRewriterClient> client,
    blink::mojom::AIRewriterCreateOptionsPtr options) {
  CreateWritingAssistanceClient<blink::mojom::AIRewriterCreateOptionsPtr,
                                blink::mojom::AIManagerCreateRewriterClient,
                                blink::mojom::AIRewriter, EchoAIRewriter>(
      std::move(client), std::move(options));
}

template <typename AICreateOptions, typename CanCreateCallback>
void EchoAIManagerImpl::CanCreateWritingAssistanceClient(
    AICreateOptions options,
    CanCreateCallback callback) {
  if (options && !SupportedLanguages(options->expected_input_languages,
                                     options->expected_context_languages,
                                     options->output_language)) {
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableUnsupportedLanguage);
    return;
  }
  if (!model_downloaded_) {
    std::move(callback).Run(
        blink::mojom::ModelAvailabilityCheckResult::kDownloadable);
  } else {
    std::move(callback).Run(
        blink::mojom::ModelAvailabilityCheckResult::kAvailable);
  }
}

template <typename AICreateOptions,
          typename AIClientRemote,
          typename AIPendingRemote,
          typename EchoAIClient>
void EchoAIManagerImpl::CreateWritingAssistanceClient(
    mojo::PendingRemote<AIClientRemote> client,
    AICreateOptions options) {
  mojo::Remote<AIClientRemote> client_remote(std::move(client));
  if (options && !SupportedLanguages(options->expected_input_languages,
                                     options->expected_context_languages,
                                     options->output_language)) {
    client_remote->OnError(
        blink::mojom::AIManagerCreateClientError::kUnsupportedLanguage,
        /*quota_error_info=*/nullptr);
    return;
  }
  auto return_task =
      base::BindOnce(&EchoAIManagerImpl::ReturnAIClientCreationResult<
                         AIClientRemote, AIPendingRemote, EchoAIClient>,
                     weak_ptr_factory_.GetWeakPtr(), std::move(client_remote));
  if (!model_downloaded_) {
    // In order to test the model download progress handling, the
    // `EchoAIManagerImpl` will always start from the `after-download` state,
    // and we simulate the downloading time by posting a delayed task.
    content::GetUIThreadTaskRunner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&EchoAIManagerImpl::DoMockDownloadingAndReturn,
                       weak_ptr_factory_.GetWeakPtr(), std::move(return_task)),
        base::Milliseconds(kMockDownloadPreparationTimeMillisecond));
  } else {
    std::move(return_task).Run();
  }
}

template <typename AIClientRemote,
          typename AIPendingRemote,
          typename EchoAIClient>
void EchoAIManagerImpl::ReturnAIClientCreationResult(
    mojo::Remote<AIClientRemote> client_remote) {
  model_downloaded_ = true;
  mojo::PendingRemote<AIPendingRemote> pending_remote;
  mojo::MakeSelfOwnedReceiver(std::make_unique<EchoAIClient>(),
                              pending_remote.InitWithNewPipeAndPassReceiver());
  client_remote->OnResult(std::move(pending_remote));
}

void EchoAIManagerImpl::ReturnAILanguageModelCreationResult(
    mojo::Remote<blink::mojom::AIManagerCreateLanguageModelClient>
        client_remote,
    blink::mojom::AILanguageModelSamplingParamsPtr sampling_params,
    base::flat_set<blink::mojom::AILanguageModelPromptType>
        enabled_input_types) {
  mojo::PendingRemote<blink::mojom::AILanguageModel> language_model;
  auto model_sampling_params =
      sampling_params
          ? std::move(sampling_params)
          : blink::mojom::AILanguageModelSamplingParams::New(
                optimization_guide::features::GetOnDeviceModelDefaultTopK(),
                optimization_guide::features::
                    GetOnDeviceModelDefaultTemperature());

  mojo::MakeSelfOwnedReceiver(
      std::make_unique<EchoAILanguageModel>(model_sampling_params->Clone(),
                                            enabled_input_types),
      language_model.InitWithNewPipeAndPassReceiver());
  client_remote->OnResult(
      std::move(language_model),
      blink::mojom::AILanguageModelInstanceInfo::New(
          kMaxContextSizeInTokens,
          /*current_tokens=*/0, std::move(model_sampling_params),
          std::vector<blink::mojom::AILanguageModelPromptType>(
              enabled_input_types.begin(), enabled_input_types.end())));
}

void EchoAIManagerImpl::DoMockDownloadingAndReturn(base::OnceClosure callback) {
  // Mock the downloading process update for testing.
  for (auto& observer : download_progress_observers_) {
    observer->OnDownloadProgressUpdate(0, kMockModelSizeBytes);
    observer->OnDownloadProgressUpdate(kMockModelSizeBytes / 3,
                                       kMockModelSizeBytes);
    observer->OnDownloadProgressUpdate(kMockModelSizeBytes / 3 * 2,
                                       kMockModelSizeBytes);
    observer->OnDownloadProgressUpdate(kMockModelSizeBytes,
                                       kMockModelSizeBytes);
  }

  std::move(callback).Run();
}

void EchoAIManagerImpl::AddModelDownloadProgressObserver(
    mojo::PendingRemote<blink::mojom::ModelDownloadProgressObserver>
        observer_remote) {
  download_progress_observers_.Add(std::move(observer_remote));
}

}  // namespace content
