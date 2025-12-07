// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PAYMENTS_STUB_SECURE_PAYMENT_CONFIRMATION_SERVICE_H_
#define CONTENT_BROWSER_PAYMENTS_STUB_SECURE_PAYMENT_CONFIRMATION_SERVICE_H_

#include <memory>

#include "content/public/browser/document_service.h"
#include "third_party/blink/public/mojom/payments/secure_payment_confirmation_service.mojom.h"

namespace content {

class AuthenticatorCommonImpl;

// A stub implementation of the SecurePaymentConfirmationService interface.
// This exists to support content browsertests which cannot use the real
// implementation.
//
// For the real implementation of SecurePaymentConfirmationService, see
// //components/payments/content/secure_payment_confirmation_service.h
//
// TODO(smcgruer): Consider whether we could move the real
// SecurePaymentConfirmationService implementation to //content, or whether this
// could be expanded to use in-memory storage for more thorough tests.
class StubSecurePaymentConfirmationService
    : public DocumentService<
          payments::mojom::SecurePaymentConfirmationService> {
 public:
  static void Create(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<payments::mojom::SecurePaymentConfirmationService>
          receiver);

  StubSecurePaymentConfirmationService(
      const StubSecurePaymentConfirmationService&) = delete;
  StubSecurePaymentConfirmationService& operator=(
      const StubSecurePaymentConfirmationService&) = delete;
  ~StubSecurePaymentConfirmationService() override;

  // mojom::SecurePaymentConfirmationService:
  void SecurePaymentConfirmationAvailability(
      SecurePaymentConfirmationAvailabilityCallback callback) override;

  // mojom::SecurePaymentConfirmationService:
  void StorePaymentCredential(const std::vector<uint8_t>& credential_id,
                              const std::string& rp_id,
                              const std::vector<uint8_t>& user_id,
                              StorePaymentCredentialCallback callback) override;

  // mojom::SecurePaymentConfirmationService:
  void MakePaymentCredential(
      blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
      MakePaymentCredentialCallback callback) override;

 private:
  StubSecurePaymentConfirmationService(
      RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<payments::mojom::SecurePaymentConfirmationService>
          receiver);

  std::unique_ptr<AuthenticatorCommonImpl> authenticator_common_impl_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PAYMENTS_STUB_SECURE_PAYMENT_CONFIRMATION_SERVICE_H_
