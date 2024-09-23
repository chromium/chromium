// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/content/browser/internal_authenticator_impl.h"

#include <string>
#include <utility>

#include "content/public/browser/authenticator_common.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

namespace content {

InternalAuthenticatorImpl::InternalAuthenticatorImpl(
    RenderFrameHost* render_frame_host)
    : WebContentsObserver(WebContents::FromRenderFrameHost(render_frame_host)),
      effective_origin_(render_frame_host->GetLastCommittedOrigin()),
      authenticator_common_(AuthenticatorCommon::Create(render_frame_host)) {
  // Disabling WebAuthn modal dialogs to avoid conflict with Autofill's own
  // modal dialogs. Since WebAuthn is designed for websites, rather than browser
  // components, the UI can be confusing for users in the case for Autofill.
  // Autofill only ever uses platform authenticators and can take place
  // on any webpage.
  authenticator_common_->DisableUI();

  // WebAuthn policy is to disallow use on pages with "Not secure" or "None" TLS
  // connection status. However, internal clients such as autofill may be called
  // from these pages (e.g. on chrome://settings/payments).
  authenticator_common_->DisableTLSCheck();
}

InternalAuthenticatorImpl::~InternalAuthenticatorImpl() = default;

void InternalAuthenticatorImpl::SetEffectiveOrigin(const url::Origin& origin) {
  effective_origin_ = url::Origin(origin);
  DCHECK(!effective_origin_.opaque());
}

void InternalAuthenticatorImpl::SetPaymentOptions(
    blink::mojom::PaymentOptionsPtr payment) {
  payment_ = std::move(payment);
}

void InternalAuthenticatorImpl::MakeCredential(
    blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
    blink::mojom::Authenticator::MakeCredentialCallback callback) {
  authenticator_common_->MakeCredential(effective_origin_, std::move(options),
                                        std::move(callback));
}

void InternalAuthenticatorImpl::GetAssertion(
    blink::mojom::PublicKeyCredentialRequestOptionsPtr options,
    blink::mojom::Authenticator::GetAssertionCallback callback) {
  authenticator_common_->GetAssertion(effective_origin_, std::move(options),
                                      std::move(payment_), std::move(callback));
}

void InternalAuthenticatorImpl::IsUserVerifyingPlatformAuthenticatorAvailable(
    blink::mojom::Authenticator::
        IsUserVerifyingPlatformAuthenticatorAvailableCallback callback) {
  authenticator_common_->IsUserVerifyingPlatformAuthenticatorAvailable(
      effective_origin_, std::move(callback));
}

bool InternalAuthenticatorImpl::IsGetMatchingCredentialIdsSupported() {
  // TODO(crbug.com/40868539): Not yet supported on any desktop platform.
  return false;
}

void InternalAuthenticatorImpl::GetMatchingCredentialIds(
    const std::string& relying_party_id,
    const std::vector<std::vector<uint8_t>>& credential_ids,
    bool require_third_party_payment_bit,
    webauthn::GetMatchingCredentialIdsCallback callback) {
  // Not yet supported on any desktop platform.
  NOTREACHED_IN_MIGRATION();
}

void InternalAuthenticatorImpl::Cancel() {
  authenticator_common_->Cancel();
}

content::RenderFrameHost* InternalAuthenticatorImpl::GetRenderFrameHost() {
  return authenticator_common_->GetRenderFrameHost();
}

void InternalAuthenticatorImpl::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  // If the RenderFrameHost itself is navigated then this function will cause
  // request state to be cleaned up. It's also possible for a navigation in the
  // same frame to use a fresh RenderFrameHost. In this case,
  // |render_frame_host_->IsCurrent()| will start returning false, causing all
  // focus checks to fail if any Mojo requests are made in that state.
  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument() ||
      navigation_handle->GetRenderFrameHost() != GetRenderFrameHost()) {
    return;
  }

  authenticator_common_->Cleanup();
}

}  // namespace content
