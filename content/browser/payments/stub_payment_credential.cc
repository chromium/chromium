// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/stub_payment_credential.h"

#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"

namespace content {

void StubPaymentCredential::Create(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<payments::mojom::PaymentCredential> receiver) {
  CHECK(render_frame_host);
  // Avoid creating the service if the RenderFrameHost isn't active, e.g. if a
  // request arrives during a navigation.
  if (!render_frame_host->IsActive()) {
    return;
  }
  // StubPaymentCredential owns itself. It self-destructs when the
  // RenderFrameHost navigates or is deleted. See DocumentService for details.
  new StubPaymentCredential(*render_frame_host, std::move(receiver));
}

StubPaymentCredential::StubPaymentCredential(
    RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<payments::mojom::PaymentCredential> receiver)
    : DocumentService(render_frame_host, std::move(receiver)) {}

void StubPaymentCredential::StorePaymentCredential(
    const std::vector<uint8_t>& credential_id,
    const std::string& rp_id,
    const std::vector<uint8_t>& user_id,
    StorePaymentCredentialCallback callback) {
  std::move(callback).Run(
      payments::mojom::PaymentCredentialStorageStatus::SUCCESS);
}

void StubPaymentCredential::MakePaymentCredential(
    blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
    MakePaymentCredentialCallback callback) {
  // This method on this stub is not implemented.
  std::move(callback).Run(
      blink::mojom::AuthenticatorStatus::UNKNOWN_ERROR,
      /*make_credential_authenticator_response_ptr=*/nullptr,
      /*webauthn_dom_exception_details_ptr=*/nullptr);
}

}  // namespace content
