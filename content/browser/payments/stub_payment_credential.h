// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PAYMENTS_STUB_PAYMENT_CREDENTIAL_H_
#define CONTENT_BROWSER_PAYMENTS_STUB_PAYMENT_CREDENTIAL_H_

#include "content/public/browser/document_service.h"
#include "third_party/blink/public/mojom/payments/payment_credential.mojom.h"

namespace content {

// A stub implementation of the PaymentCredential interface, which simply
// returns success without actually storing a credential. This exists to support
// content browsertests around WebAuthn credential creation.
//
// For the real implementation of PaymentCredential, see
// //components/payments/content/payment_credential.h
//
// TODO(smcgruer): Consider whether we could move the real PaymentCredential
// implementation to //content, or whether this could be expanded to use
// in-memory storage for more thorough tests.
class StubPaymentCredential
    : public DocumentService<payments::mojom::PaymentCredential> {
 public:
  static void Create(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<payments::mojom::PaymentCredential> receiver);

  StubPaymentCredential(const StubPaymentCredential&) = delete;
  StubPaymentCredential& operator=(const StubPaymentCredential&) = delete;

  // mojom::PaymentCredential
  void StorePaymentCredential(const std::vector<uint8_t>& credential_id,
                              const std::string& rp_id,
                              const std::vector<uint8_t>& user_id,
                              StorePaymentCredentialCallback callback) override;

 private:
  StubPaymentCredential(
      RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<payments::mojom::PaymentCredential> receiver);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PAYMENTS_STUB_PAYMENT_CREDENTIAL_H_
