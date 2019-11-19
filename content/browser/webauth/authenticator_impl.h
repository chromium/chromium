// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_IMPL_H_
#define CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "content/browser/webauth/authenticator_common.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"

namespace base {
class OneShotTimer;
}

namespace device {

struct PlatformAuthenticatorInfo;
struct CtapGetAssertionRequest;
class FidoRequestHandlerBase;

enum class FidoReturnCode : uint8_t;

}  // namespace device

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace url {
class Origin;
}

namespace content {

class RenderFrameHost;

// Implementation of the public Authenticator interface.
class CONTENT_EXPORT AuthenticatorImpl : public blink::mojom::Authenticator,
                                         public WebContentsObserver {
 public:
  explicit AuthenticatorImpl(RenderFrameHost* render_frame_host);

  // By being able to set AuthenticatorCommon, this constructor permits setting
  // the connector and timer for testing. Using this constructor will also empty
  // out the protocol set, since no device discovery will take place during
  // tests.
  AuthenticatorImpl(RenderFrameHost* render_frame_host,
                    std::unique_ptr<AuthenticatorCommon> authenticator_common);
  ~AuthenticatorImpl() override;

  // Creates a binding between this implementation and |request|.
  //
  // Note that one AuthenticatorImpl instance can be bound to exactly one
  // interface connection at a time, and disconnected when the frame navigates
  // to a new active document.
  void Bind(mojo::PendingReceiver<blink::mojom::Authenticator> receiver);

 private:
  friend class AuthenticatorImplTest;

  AuthenticatorCommon* get_authenticator_common_for_testing() {
    return authenticator_common_.get();
  }

  // mojom:Authenticator
  void MakeCredential(
      blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
      MakeCredentialCallback callback) override;
  void GetAssertion(blink::mojom::PublicKeyCredentialRequestOptionsPtr options,
                    GetAssertionCallback callback) override;
  void IsUserVerifyingPlatformAuthenticatorAvailable(
      IsUserVerifyingPlatformAuthenticatorAvailableCallback callback) override;
  void Cancel() override;

  // WebContentsObserver:
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

  RenderFrameHost* const render_frame_host_;
  std::unique_ptr<AuthenticatorCommon> authenticator_common_;

  // Owns pipes to this Authenticator from |render_frame_host_|.
  mojo::Receiver<blink::mojom::Authenticator> receiver_{this};

  base::WeakPtrFactory<AuthenticatorImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_IMPL_H_
