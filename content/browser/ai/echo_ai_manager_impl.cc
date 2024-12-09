// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ai/echo_ai_manager_impl.h"

#include "base/no_destructor.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "content/browser/ai/echo_ai_language_model.h"
#include "content/browser/ai/echo_ai_rewriter.h"
#include "content/browser/ai/echo_ai_summarizer.h"
#include "content/browser/ai/echo_ai_writer.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom.h"

namespace content {

namespace {

const int kMockDownloadPreparationTimeMillisecond = 300;
const int kMockModelSizeBytes = 3000;

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
    CanCreateLanguageModelCallback callback) {
  std::move(callback).Run(
      blink::mojom::ModelAvailabilityCheckResult::kAfterDownload);
}

void EchoAIManagerImpl::CreateLanguageModel(
    mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
        client,
    blink::mojom::AILanguageModelCreateOptionsPtr options) {
  mojo::Remote<blink::mojom::AIManagerCreateLanguageModelClient> client_remote(
      std::move(client));

  if (options->system_prompt.has_value() &&
      options->system_prompt->size() > kMaxContextSizeInTokens) {
    client_remote->OnError(blink::mojom::AIManagerCreateLanguageModelError::
                               kInitialPromptsTooLarge);
    return;
  }

  auto return_langauge_model_callback =
      base::BindOnce(&EchoAIManagerImpl::ReturnAILanguageModelCreationResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(client_remote));

  // In order to test the model download progress handling, the
  // `EchoAIManagerImpl` will always start from the `after-download` state, and
  // we simulate the downloading time by posting a delayed task.
  content::GetUIThreadTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&EchoAIManagerImpl::DoMockDownloadingAndReturn,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(return_langauge_model_callback)),
      base::Milliseconds(kMockDownloadPreparationTimeMillisecond));
}

void EchoAIManagerImpl::CanCreateSummarizer(
    CanCreateSummarizerCallback callback) {
  if (!summarizer_downloaded_) {
    std::move(callback).Run(
        /*result=*/blink::mojom::ModelAvailabilityCheckResult::kAfterDownload);
  } else {
    std::move(callback).Run(
        /*result=*/blink::mojom::ModelAvailabilityCheckResult::kReadily);
  }
}

void EchoAIManagerImpl::CreateSummarizer(
    mojo::PendingRemote<blink::mojom::AIManagerCreateSummarizerClient> client,
    blink::mojom::AISummarizerCreateOptionsPtr options) {
  mojo::Remote<blink::mojom::AIManagerCreateSummarizerClient> client_remote(
      std::move(client));
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

void EchoAIManagerImpl::GetModelInfo(GetModelInfoCallback callback) {
  std::move(callback).Run(blink::mojom::AIModelInfo::New(
      optimization_guide::features::GetOnDeviceModelDefaultTopK(),
      optimization_guide::features::GetOnDeviceModelMaxTopK(),
      optimization_guide::features::GetOnDeviceModelDefaultTemperature()));
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
        client_remote) {
  mojo::PendingRemote<blink::mojom::AILanguageModel> language_model;
  mojo::MakeSelfOwnedReceiver(std::make_unique<EchoAILanguageModel>(),
                              language_model.InitWithNewPipeAndPassReceiver());
  client_remote->OnResult(
      std::move(language_model),
      blink::mojom::AILanguageModelInfo::New(
          kMaxContextSizeInTokens,
          blink::mojom::AILanguageModelSamplingParams::New(
              optimization_guide::features::GetOnDeviceModelDefaultTopK(),
              optimization_guide::features::
                  GetOnDeviceModelDefaultTemperature())));
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
