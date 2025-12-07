// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/stub_secure_payment_confirmation_service.h"

#include "content/browser/webauth/authenticator_common_impl.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"

namespace content {

void StubSecurePaymentConfirmationService::Create(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<payments::mojom::SecurePaymentConfirmationService>
        receiver) {
  CHECK(render_frame_host);
  // Avoid creating the service if the RenderFrameHost isn't active, e.g. if a
  // request arrives during a navigation.
  if (!render_frame_host->IsActive()) {
    return;
  }
  // StubSecurePaymentConfirmationService owns itself. It self-destructs when
  // the RenderFrameHost navigates or is deleted. See DocumentService for
  // details.
  new StubSecurePaymentConfirmationService(*render_frame_host,
                                           std::move(receiver));
}

StubSecurePaymentConfirmationService::StubSecurePaymentConfirmationService(
    RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<payments::mojom::SecurePaymentConfirmationService>
        receiver)
    : DocumentService(render_frame_host, std::move(receiver)),
      authenticator_common_impl_(std::make_unique<AuthenticatorCommonImpl>(
          &render_frame_host,
          // The real SecurePaymentConfirmationService uses an
          // InternalAuthenticator so kInternalUses is set here.
          AuthenticatorCommonImpl::ServingRequestsFor::kInternalUses)) {}

StubSecurePaymentConfirmationService::~StubSecurePaymentConfirmationService() =
    default;

void StubSecurePaymentConfirmationService::
    SecurePaymentConfirmationAvailability(
        SecurePaymentConfirmationAvailabilityCallback callback) {
  std::move(callback).Run(
      payments::mojom::SecurePaymentConfirmationAvailabilityEnum::
          kUnavailableFeatureNotEnabled);
}

void StubSecurePaymentConfirmationService::StorePaymentCredential(
    const std::vector<uint8_t>& credential_id,
    const std::string& rp_id,
    const std::vector<uint8_t>& user_id,
    StorePaymentCredentialCallback callback) {
  std::move(callback).Run(
      payments::mojom::PaymentCredentialStorageStatus::SUCCESS);
}

void StubSecurePaymentConfirmationService::MakePaymentCredential(
    blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
    MakePaymentCredentialCallback callback) {
  authenticator_common_impl_->MakeCredential(origin(), std::move(options),
                                             /*payment_options=*/nullptr,
                                             std::move(callback));
}

}  // namespace content
