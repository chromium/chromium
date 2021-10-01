// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_auth_request_service.h"

#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/federated_auth_request_impl.h"
#include "content/browser/webid/webid_utils.h"

namespace content {

// static
void FederatedAuthRequestService::Create(
    RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::FederatedAuthRequest> receiver) {
  DCHECK(host);

  // TODO(kenrb): This should also be verified in the renderer process before
  // the mojo method is invoked, causing the promise to be rejected.
  // https://crbug.com/1141125
  // It is safe to access host->GetLastCommittedOrigin during construction
  // but DocumentService::origin() should be used thereafter.
  if (!IsSameOriginWithAncestors(host, host->GetLastCommittedOrigin())) {
    mojo::ReportBadMessage(
        "navigator.id.get cannot be invoked from within cross-origin iframes.");
    return;
  }

  // FederatedAuthRequestService owns itself. It will self-destruct when a mojo
  // interface error occurs, the RenderFrameHost is deleted, or the
  // RenderFrameHost navigates to a new document.
  new FederatedAuthRequestService(host, std::move(receiver));
}

FederatedAuthRequestService::FederatedAuthRequestService(
    RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::FederatedAuthRequest> receiver)
    : DocumentService(host, std::move(receiver)) {
  impl_ = std::make_unique<FederatedAuthRequestImpl>(
      host, host->GetLastCommittedOrigin());
}

FederatedAuthRequestService::~FederatedAuthRequestService() = default;

void FederatedAuthRequestService::RequestIdToken(
    const GURL& provider,
    const std::string& client_id,
    const std::string& nonce,
    blink::mojom::RequestMode mode,
    bool prefer_auto_sign_in,
    RequestIdTokenCallback callback) {
  impl_->RequestIdToken(provider, client_id, nonce, mode, prefer_auto_sign_in,
                        std::move(callback));
}

void FederatedAuthRequestService::Logout(
    std::vector<blink::mojom::LogoutRequestPtr> logout_requests,
    LogoutCallback callback) {
  impl_->Logout(std::move(logout_requests), std::move(callback));
}

}  // namespace content
