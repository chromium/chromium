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
#include "third_party/blink/public/mojom/ai/ai_common.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-forward.h"
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
    std::optional<std::vector<blink::mojom::AILanguageCodePtr>>
        expected_input_languages,
    CanCreateLanguageModelCallback callback) {
  if (expected_input_languages.has_value() &&
      !IsLanguagesSupported(expected_input_languages.value())) {
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

  if (options->system_prompt.has_value() &&
      options->system_prompt->size() > kMaxContextSizeInTokens) {
    client_remote->OnError(
        blink::mojom::AIManagerCreateClientError::kInitialInputTooLarge);
    return;
  }

  auto return_language_model_callback =
      base::BindOnce(&EchoAIManagerImpl::ReturnAILanguageModelCreationResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(client_remote),
                     std::move(options->sampling_params));

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
  if (options && !SupportedLanguages(options->expected_input_languages,
                                     options->expected_context_languages,
                                     options->output_language)) {
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableUnsupportedLanguage);
    return;
  }
  if (!summarizer_downloaded_) {
    std::move(callback).Run(
        blink::mojom::ModelAvailabilityCheckResult::kDownloadable);
  } else {
    std::move(callback).Run(
        blink::mojom::ModelAvailabilityCheckResult::kAvailable);
  }
}

void EchoAIManagerImpl::CreateSummarizer(
    mojo::PendingRemote<blink::mojom::AIManagerCreateSummarizerClient> client,
    blink::mojom::AISummarizerCreateOptionsPtr options) {
  mojo::Remote<blink::mojom::AIManagerCreateSummarizerClient> client_remote(
      std::move(client));
  if (options && !SupportedLanguages(options->expected_input_languages,
                                     options->expected_context_languages,
                                     options->output_language)) {
    client_remote->OnResult(mojo::PendingRemote<blink::mojom::AISummarizer>());
    return;
  }
  auto return_summarizer_task =
      base::BindOnce(&EchoAIManagerImpl::ReturnAISummarizerCreationResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(client_remote));
  if (!summarizer_downloaded_) {
    // In order to test the model download progress handling, the
    // `EchoAIManagerImpl` will always start from the `after-download` state,
    // and we simulate the downloading time by posting a delayed task.
    content::GetUIThreadTaskRunner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&EchoAIManagerImpl::DoMockDownloadingAndReturn,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(return_summarizer_task)),
        base::Milliseconds(kMockDownloadPreparationTimeMillisecond));
  } else {
    std::move(return_summarizer_task).Run();
  }
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
  std::move(callback).Run(
      blink::mojom::ModelAvailabilityCheckResult::kAvailable);
}

void EchoAIManagerImpl::CreateWriter(
    mojo::PendingRemote<blink::mojom::AIManagerCreateWriterClient> client,
    blink::mojom::AIWriterCreateOptionsPtr options) {
  mojo::Remote<blink::mojom::AIManagerCreateWriterClient> client_remote(
      std::move(client));
  mojo::PendingRemote<blink::mojom::AIWriter> writer;
  mojo::MakeSelfOwnedReceiver(std::make_unique<EchoAIWriter>(),
                              writer.InitWithNewPipeAndPassReceiver());
  client_remote->OnResult(std::move(writer));
}

void EchoAIManagerImpl::CanCreateRewriter(
    blink::mojom::AIRewriterCreateOptionsPtr options,
    CanCreateRewriterCallback callback) {
  std::move(callback).Run(
      blink::mojom::ModelAvailabilityCheckResult::kAvailable);
}

void EchoAIManagerImpl::CreateRewriter(
    mojo::PendingRemote<blink::mojom::AIManagerCreateRewriterClient> client,
    blink::mojom::AIRewriterCreateOptionsPtr options) {
  mojo::Remote<blink::mojom::AIManagerCreateRewriterClient> client_remote(
      std::move(client));
  mojo::PendingRemote<::blink::mojom::AIRewriter> rewriter;
  mojo::MakeSelfOwnedReceiver(std::make_unique<EchoAIRewriter>(),
                              rewriter.InitWithNewPipeAndPassReceiver());
  client_remote->OnResult(std::move(rewriter));
}

void EchoAIManagerImpl::ReturnAILanguageModelCreationResult(
    mojo::Remote<blink::mojom::AIManagerCreateLanguageModelClient>
        client_remote,
    blink::mojom::AILanguageModelSamplingParamsPtr sampling_params) {
  mojo::PendingRemote<blink::mojom::AILanguageModel> language_model;
  auto model_sampling_params =
      sampling_params
          ? std::move(sampling_params)
          : blink::mojom::AILanguageModelSamplingParams::New(
                optimization_guide::features::GetOnDeviceModelDefaultTopK(),
                optimization_guide::features::
                    GetOnDeviceModelDefaultTemperature());

  mojo::MakeSelfOwnedReceiver(
      std::make_unique<EchoAILanguageModel>(model_sampling_params->Clone()),
      language_model.InitWithNewPipeAndPassReceiver());
  client_remote->OnResult(std::move(language_model),
                          blink::mojom::AILanguageModelInstanceInfo::New(
                              kMaxContextSizeInTokens,
                              /*current_tokens=*/0,
                              std::move(model_sampling_params), std::nullopt));
}

void EchoAIManagerImpl::ReturnAISummarizerCreationResult(
    mojo::Remote<blink::mojom::AIManagerCreateSummarizerClient> client_remote) {
  summarizer_downloaded_ = true;
  mojo::PendingRemote<blink::mojom::AISummarizer> summarizer;
  mojo::MakeSelfOwnedReceiver(std::make_unique<EchoAISummarizer>(),
                              summarizer.InitWithNewPipeAndPassReceiver());
  client_remote->OnResult(std::move(summarizer));
}

void EchoAIManagerImpl::DoMockDownloadingAndReturn(base::OnceClosure callback) {
  // Mock the downloading process update for testing.
  for (auto& observer : download_progress_observers_) {
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
