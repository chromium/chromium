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
  // Avoid creating the service if the RenderFrameHost isn't active, e.g. if a
  // request arrives during a navigation.
  if (!render_frame_host->IsActive()) {
    return;
  }

  // AuthenticatorImpl owns itself. It self-destructs when the RenderFrameHost
  // navigates or is deleted. See DocumentService for details.
  DCHECK(render_frame_host);
  new AuthenticatorImpl(
      render_frame_host, std::move(receiver),
      std::make_unique<AuthenticatorCommon>(render_frame_host));
}

AuthenticatorImpl::AuthenticatorImpl(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::Authenticator> receiver,
    std::unique_ptr<AuthenticatorCommon> authenticator_common)
    : DocumentService(render_frame_host, std::move(receiver)),
      authenticator_common_(std::move(authenticator_common)) {
  authenticator_common_->EnableRequestProxyExtensionsAPISupport();
  DCHECK(authenticator_common_);
}

AuthenticatorImpl::~AuthenticatorImpl() = default;

// DocumentService
void AuthenticatorImpl::WillBeDestroyed(DocumentServiceDestructionReason) {
  // Explicit reset() `authenticator_commmon_` now, which tries to reply to
  // pending Mojo callbacks in its destructor.
  //
  // TODO(https://crbug.com/1317534): Previously, running the callbacks in the
  // destructor was required to avoid triggering DCHECKs since the
  // mojo::Receiver was (incorrectly) not yet reset in the destructor.
  //
  // The destruction order is fixed so running the reply callbacks should no
  // longer be necessary; however, there are now unit test-only dependencies on
  // this behavior. Remove those test dependencies and this `WillBeDestroyed()`
  // override can be completely deleted.
  authenticator_common_.reset();
}

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

void AuthenticatorImpl::Cancel() {
  authenticator_common_->Cancel();
}

}  // namespace content
