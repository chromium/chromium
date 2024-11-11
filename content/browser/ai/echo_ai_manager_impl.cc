// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ai/echo_ai_manager_impl.h"

#include "base/no_destructor.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "content/browser/ai/echo_ai_assistant.h"
#include "content/browser/ai/echo_ai_rewriter.h"
#include "content/browser/ai/echo_ai_summarizer.h"
#include "content/browser/ai/echo_ai_writer.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/ai/ai_assistant.mojom.h"

namespace content {

namespace {

const int kMockDownloadPreperationTimeMillisecond = 300;
const int kMockModelSizeBytes = 3000;

}  // namespace

EchoAIManagerImpl::EchoAIManagerImpl() = default;

EchoAIManagerImpl::~EchoAIManagerImpl() = default;

// static
void EchoAIManagerImpl::Create(
    base::SupportsUserData& context_user_data,
    mojo::PendingReceiver<blink::mojom::AIManager> receiver) {
  static base::NoDestructor<EchoAIManagerImpl> ai;
  ai->receivers_.Add(ai.get(), std::move(receiver));
}

void EchoAIManagerImpl::CanCreateAssistant(
    CanCreateAssistantCallback callback) {
  std::move(callback).Run(
      blink::mojom::ModelAvailabilityCheckResult::kAfterDownload);
}

void EchoAIManagerImpl::CreateAssistant(
    mojo::PendingRemote<blink::mojom::AIManagerCreateAssistantClient> client,
    blink::mojom::AIAssistantCreateOptionsPtr options) {
  mojo::Remote<blink::mojom::AIManagerCreateAssistantClient> client_remote(
      std::move(client));

  // In order to test the model download progress handling, the
  // `EchoAIManagerImpl` will always start from the `after-download` state, and
  // we simulate the downloading time by posting a delayed task.
  content::GetUIThreadTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&EchoAIManagerImpl::DoMockDownloadingAndReturn,
                     weak_ptr_factory_.GetWeakPtr(), std::move(client_remote)),
      base::Milliseconds(kMockDownloadPreperationTimeMillisecond));
}

void EchoAIManagerImpl::CanCreateSummarizer(
    CanCreateSummarizerCallback callback) {
  std::move(callback).Run(
      /*result=*/blink::mojom::ModelAvailabilityCheckResult::kReadily);
}

void EchoAIManagerImpl::CreateSummarizer(
    mojo::PendingRemote<blink::mojom::AIManagerCreateSummarizerClient> client,
    blink::mojom::AISummarizerCreateOptionsPtr options) {
  mojo::Remote<blink::mojom::AIManagerCreateSummarizerClient> client_remote(
      std::move(client));
  mojo::PendingRemote<blink::mojom::AISummarizer> summarizer;
  mojo::MakeSelfOwnedReceiver(std::make_unique<EchoAISummarizer>(),
                              summarizer.InitWithNewPipeAndPassReceiver());
  client_remote->OnResult(std::move(summarizer));
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

void EchoAIManagerImpl::ReturnAIAssistantCreationResult(
    mojo::Remote<blink::mojom::AIManagerCreateAssistantClient> client_remote) {
  mojo::PendingRemote<blink::mojom::AIAssistant> assistant;
  mojo::MakeSelfOwnedReceiver(std::make_unique<EchoAIAssistant>(),
                              assistant.InitWithNewPipeAndPassReceiver());
  client_remote->OnResult(
      std::move(assistant),
      blink::mojom::AIAssistantInfo::New(
          kMaxContextSizeInTokens,
          blink::mojom::AIAssistantSamplingParams::New(
              optimization_guide::features::GetOnDeviceModelDefaultTopK(),
              optimization_guide::features::
                  GetOnDeviceModelDefaultTemperature())));
}

void EchoAIManagerImpl::DoMockDownloadingAndReturn(
    mojo::Remote<blink::mojom::AIManagerCreateAssistantClient> client_remote) {
  // Mock the downloading process update for testing.
  for (auto& observer : download_progress_observers_) {
    observer->OnDownloadProgressUpdate(kMockModelSizeBytes / 3,
                                       kMockModelSizeBytes);
    observer->OnDownloadProgressUpdate(kMockModelSizeBytes / 3 * 2,
                                       kMockModelSizeBytes);
    observer->OnDownloadProgressUpdate(kMockModelSizeBytes,
                                       kMockModelSizeBytes);
  }

  ReturnAIAssistantCreationResult(std::move(client_remote));
}

void EchoAIManagerImpl::AddModelDownloadProgressObserver(
    mojo::PendingRemote<blink::mojom::ModelDownloadProgressObserver>
        observer_remote) {
  download_progress_observers_.Add(std::move(observer_remote));
}

}  // namespace content
