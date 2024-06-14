// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ai/echo_ai_manager_impl.h"

#include "base/no_destructor.h"
#include "content/browser/ai/echo_ai_text_session.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-shared.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"

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
    CreateTextSessionCallback callback) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<EchoAITextSession>(),
                              std::move(receiver));
  std::move(callback).Run(/*success=*/true);
}

void EchoAIManagerImpl::GetDefaultTextSessionSamplingParams(
    GetDefaultTextSessionSamplingParamsCallback callback) {
  std::move(callback).Run(blink::mojom::AITextSessionSamplingParams::New(
      /*top_k=*/1, /*temperature=*/0));
}

}  // namespace content
