// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ai/echo_ai_manager_impl.h"

#include "base/no_destructor.h"
#include "base/supports_user_data.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "content/browser/ai/echo_ai_text_session.h"
#include "content/browser/ai/echo_ai_writer.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

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

void EchoAIManagerImpl::CanCreateTextSession(
    CanCreateTextSessionCallback callback) {
  std::move(callback).Run(
      /*result=*/blink::mojom::ModelAvailabilityCheckResult::kReadily);
}

void EchoAIManagerImpl::CreateTextSession(
    mojo::PendingReceiver<blink::mojom::AITextSession> receiver,
    blink::mojom::AITextSessionSamplingParamsPtr sampling_params,
    const std::optional<std::string>& system_prompt,
    CreateTextSessionCallback callback) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<EchoAITextSession>(),
                              std::move(receiver));
  std::move(callback).Run(/*success=*/true);
}

void EchoAIManagerImpl::GetTextModelInfo(GetTextModelInfoCallback callback) {
  std::move(callback).Run(blink::mojom::AITextModelInfo::New(
      optimization_guide::features::GetOnDeviceModelDefaultTopK(),
      optimization_guide::features::GetOnDeviceModelMaxTopK(),
      optimization_guide::features::GetOnDeviceModelDefaultTemperature()));
}

void EchoAIManagerImpl::CreateWriter(
    const std::optional<std::string>& shared_context,
    mojo::PendingRemote<blink::mojom::AIManagerCreateWriterClient> client) {
  mojo::Remote<blink::mojom::AIManagerCreateWriterClient> client_remote(
      std::move(client));
  mojo::PendingRemote<blink::mojom::AIWriter> writer;
  mojo::MakeSelfOwnedReceiver(std::make_unique<EchoAIWriter>(),
                              writer.InitWithNewPipeAndPassReceiver());
  client_remote->OnResult(std::move(writer));
}

}  // namespace content
