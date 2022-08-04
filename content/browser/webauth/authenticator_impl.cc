// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/authenticator_impl.h"

#include <memory>
#include <utility>

#include "content/browser/webauth/authenticator_common.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/render_frame_host.h"

namespace content {

void AuthenticatorImpl::Create(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::Authenticator> receiver) {
  CHECK(render_frame_host);
  // Avoid creating the service if the RenderFrameHost isn't active, e.g. if a
  // request arrives during a navigation.
  if (!render_frame_host->IsActive()) {
    return;
  }
  // AuthenticatorImpl owns itself. It self-destructs when the RenderFrameHost
  // navigates or is deleted. See DocumentService for details.
  new AuthenticatorImpl(
      *render_frame_host, std::move(receiver),
      std::make_unique<AuthenticatorCommon>(render_frame_host));
}

void AuthenticatorImpl::CreateForTesting(
    RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<blink::mojom::Authenticator> receiver,
    std::unique_ptr<AuthenticatorCommon> authenticator_common) {
  new AuthenticatorImpl(render_frame_host, std::move(receiver),
                        std::move(authenticator_common));
}

AuthenticatorImpl::AuthenticatorImpl(
    RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<blink::mojom::Authenticator> receiver,
    std::unique_ptr<AuthenticatorCommon> authenticator_common)
    : DocumentService(render_frame_host, std::move(receiver)),
      authenticator_common_(std::move(authenticator_common)) {
  authenticator_common_->EnableRequestProxyExtensionsAPISupport();
  DCHECK(authenticator_common_);
}

AuthenticatorImpl::~AuthenticatorImpl() = default;

// mojom::Authenticator
void AuthenticatorImpl::MakeCredential(
    blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
    MakeCredentialCallback callback) {
  authenticator_common_->MakeCredential(origin(), std::move(options),
                                        std::move(callback));
}

// mojom:Authenticator
void AuthenticatorImpl::GetAssertion(
    blink::mojom::PublicKeyCredentialRequestOptionsPtr options,
    GetAssertionCallback callback) {
  authenticator_common_->GetAssertion(origin(), std::move(options),
                                      /*payment=*/nullptr, std::move(callback));
}

void AuthenticatorImpl::IsUserVerifyingPlatformAuthenticatorAvailable(
    IsUserVerifyingPlatformAuthenticatorAvailableCallback callback) {
  authenticator_common_->IsUserVerifyingPlatformAuthenticatorAvailable(
      std::move(callback));
}

void AuthenticatorImpl::IsConditionalMediationAvailable(
    IsConditionalMediationAvailableCallback callback) {
  authenticator_common_->IsConditionalMediationAvailable(std::move(callback));
}

void AuthenticatorImpl::Cancel() {
  authenticator_common_->Cancel();
}

}  // namespace content
