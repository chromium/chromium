// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ai/mock_ai_manager_impl.h"

#include "content/browser/ai/mock_ai_text_session.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"

namespace content {

DOCUMENT_USER_DATA_KEY_IMPL(MockAIManagerImpl);

MockAIManagerImpl::MockAIManagerImpl(content::RenderFrameHost* rfh)
    : DocumentUserData<MockAIManagerImpl>(rfh) {}

MockAIManagerImpl::~MockAIManagerImpl() = default;

// static
void MockAIManagerImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::AIManager> receiver) {
  MockAIManagerImpl* ai =
      MockAIManagerImpl::GetOrCreateForCurrentDocument(render_frame_host);
  ai->receiver_.Bind(std::move(receiver));
}

void MockAIManagerImpl::CanCreateTextSession(
    CanCreateTextSessionCallback callback) {
  std::move(callback).Run(/*can_create=*/true);
}

void MockAIManagerImpl::CreateTextSession(
    mojo::PendingReceiver<blink::mojom::AITextSession> receiver,
    blink::mojom::AITextSessionSamplingParamsPtr sampling_params,
    CreateTextSessionCallback callback) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<MockAITextSession>(),
                              std::move(receiver));
  std::move(callback).Run(/*success=*/true);
}

void MockAIManagerImpl::GetDefaultTextSessionSamplingParams(
    GetDefaultTextSessionSamplingParamsCallback callback) {
  std::move(callback).Run(blink::mojom::AITextSessionSamplingParams::New(
      /*top_k=*/1, /*temperature=*/0));
}

}  // namespace content
