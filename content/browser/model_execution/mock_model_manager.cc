// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/model_execution/mock_model_manager.h"

#include "content/browser/model_execution/mock_model_execution_session.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/model_execution/model_manager.mojom.h"

DOCUMENT_USER_DATA_KEY_IMPL(MockModelManager);

MockModelManager::MockModelManager(content::RenderFrameHost* rfh)
    : DocumentUserData<MockModelManager>(rfh) {}

MockModelManager::~MockModelManager() = default;

// static
void MockModelManager::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::ModelManager> receiver) {
  MockModelManager* model_manager =
      MockModelManager::GetOrCreateForCurrentDocument(render_frame_host);
  model_manager->receiver_.Bind(std::move(receiver));
}

void MockModelManager::CanCreateGenericSession(
    CanCreateGenericSessionCallback callback) {
  std::move(callback).Run(/*can_create=*/true);
}

void MockModelManager::CreateGenericSession(
    mojo::PendingReceiver<blink::mojom::ModelGenericSession> receiver,
    blink::mojom::ModelGenericSessionSamplingParamsPtr sampling_params,
    CreateGenericSessionCallback callback) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<MockModelExecutionSession>(),
                              std::move(receiver));
  std::move(callback).Run(/*success=*/true);
}

void MockModelManager::GetDefaultGenericSessionSamplingParams(
    GetDefaultGenericSessionSamplingParamsCallback callback) {
  std::move(callback).Run(blink::mojom::ModelGenericSessionSamplingParams::New(
      /*top_k=*/1, /*temperature=*/0));
}
