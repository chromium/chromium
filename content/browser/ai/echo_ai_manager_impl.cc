// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ai/echo_ai_manager_impl.h"

#include "base/no_destructor.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "content/browser/ai/echo_ai_text_session.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

EchoAIManagerImpl::EchoAIManagerImpl(content::BrowserContext* browser_context) {
}

EchoAIManagerImpl::~EchoAIManagerImpl() = default;

// static
void EchoAIManagerImpl::Create(
    content::BrowserContext* browser_context,
    mojo::PendingReceiver<blink::mojom::AIManager> receiver) {
  static base::NoDestructor<EchoAIManagerImpl> ai(browser_context);
  ai->receivers_.Add(ai.get(), std::move(receiver));
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

}  // namespace content
