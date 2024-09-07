// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_IMPL_H_
#define CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_IMPL_H_

#include <stdint.h>

#include <memory>

#include "content/common/content_export.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"

namespace content {

class AuthenticatorCommonImpl;
class RenderFrameHost;

// Implementation of the public Authenticator interface.
class CONTENT_EXPORT AuthenticatorImpl
    : public DocumentService<blink::mojom::Authenticator> {
 public:
  static void Create(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::Authenticator> receiver);

  static void CreateForTesting(
      RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<blink::mojom::Authenticator> receiver,
      std::unique_ptr<AuthenticatorCommonImpl> authenticator_common_impl);

  AuthenticatorImpl(const AuthenticatorImpl&) = delete;
  AuthenticatorImpl& operator=(const AuthenticatorImpl&) = delete;

 private:
  friend class AuthenticatorImplTest;
  friend class AuthenticatorImplRequestDelegateTest;

  AuthenticatorImpl(
      RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<blink::mojom::Authenticator> receiver,
      std::unique_ptr<AuthenticatorCommonImpl> authenticator_common_impl);
  ~AuthenticatorImpl() override;

  // mojom::Authenticator
  void MakeCredential(
      blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
      MakeCredentialCallback callback) override;
  void GetAssertion(blink::mojom::PublicKeyCredentialRequestOptionsPtr options,
                    GetAssertionCallback callback) override;
  void GetClientCapabilities(GetClientCapabilitiesCallback callback) override;
  void Report(blink::mojom::PublicKeyCredentialReportOptionsPtr options,
              ReportCallback callback) override;
  void IsUserVerifyingPlatformAuthenticatorAvailable(
      IsUserVerifyingPlatformAuthenticatorAvailableCallback callback) override;
  void IsConditionalMediationAvailable(
      IsConditionalMediationAvailableCallback callback) override;
  void Cancel() override;

  std::unique_ptr<AuthenticatorCommonImpl> authenticator_common_impl_;

  // Owns pipes to this Authenticator from |render_frame_host_|.
  mojo::Receiver<blink::mojom::Authenticator> receiver_{this};

  base::WeakPtrFactory<AuthenticatorImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_IMPL_H_
