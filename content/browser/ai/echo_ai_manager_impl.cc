// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ai/echo_ai_manager_impl.h"

#include "base/no_destructor.h"
#include "base/supports_user_data.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "content/browser/ai/echo_ai_assistant.h"
#include "content/browser/ai/echo_ai_rewriter.h"
#include "content/browser/ai/echo_ai_summarizer.h"
#include "content/browser/ai/echo_ai_writer.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/ai/ai_assistant.mojom.h"

namespace content {

EchoAIManagerImpl::EchoAIManagerImpl(content::BrowserContext* browser_context,
                                     ReceiverContext context) {}

EchoAIManagerImpl::~EchoAIManagerImpl() = default;

// static
void EchoAIManagerImpl::Create(
    content::BrowserContext* browser_context,
    ReceiverContext context,
    mojo::PendingReceiver<blink::mojom::AIManager> receiver) {
  static base::NoDestructor<EchoAIManagerImpl> ai(browser_context, context);
  ai->receivers_.Add(ai.get(), std::move(receiver), context);
}

void EchoAIManagerImpl::CanCreateAssistant(
    CanCreateAssistantCallback callback) {
  std::move(callback).Run(
      /*result=*/blink::mojom::ModelAvailabilityCheckResult::kReadily);
}

void EchoAIManagerImpl::CreateAssistant(
    mojo::PendingReceiver<blink::mojom::AIAssistant> receiver,
    blink::mojom::AIAssistantSamplingParamsPtr sampling_params,
    const std::optional<std::string>& system_prompt,
    std::vector<blink::mojom::AIAssistantInitialPromptPtr> initial_prompts,
    CreateAssistantCallback callback) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<EchoAIAssistant>(),
                              std::move(receiver));
  std::move(callback).Run(blink::mojom::AIAssistantInfo::New(
      optimization_guide::features::GetOnDeviceModelMaxTokensForContext(),
      blink::mojom::AIAssistantSamplingParams::New(
          optimization_guide::features::GetOnDeviceModelDefaultTopK(),
          optimization_guide::features::GetOnDeviceModelDefaultTemperature())));
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
  mojo::PendingRemote<blink::mojom::AISummarizer> summarzier;
  mojo::MakeSelfOwnedReceiver(std::make_unique<EchoAISummarizer>(),
                              summarzier.InitWithNewPipeAndPassReceiver());
  client_remote->OnResult(std::move(summarzier));
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

}  // namespace content
