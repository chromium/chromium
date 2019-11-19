// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_WEBAUTHN_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_WEBAUTHN_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/web_authn.h"
#include "content/common/content_export.h"

namespace content {
class VirtualAuthenticator;
class VirtualFidoDiscoveryFactory;
namespace protocol {

class WebAuthnHandler : public DevToolsDomainHandler, public WebAuthn::Backend {
 public:
  CONTENT_EXPORT WebAuthnHandler();
  CONTENT_EXPORT ~WebAuthnHandler() override;

  // DevToolsDomainHandler:
  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;
  void Wire(UberDispatcher* dispatcher) override;

  // WebAuthn::Backend
  CONTENT_EXPORT Response Enable() override;
  CONTENT_EXPORT Response Disable() override;
  Response AddVirtualAuthenticator(
      std::unique_ptr<WebAuthn::VirtualAuthenticatorOptions> options,
      String* out_authenticator_id) override;
  Response RemoveVirtualAuthenticator(const String& authenticator_id) override;
  Response AddCredential(
      const String& authenticator_id,
      std::unique_ptr<protocol::WebAuthn::Credential> credential) override;
  Response GetCredential(
      const String& authenticator_id,
      const Binary& credential_id,
      std::unique_ptr<WebAuthn::Credential>* out_credential) override;
  Response GetCredentials(
      const String& authenticator_id,
      std::unique_ptr<protocol::Array<protocol::WebAuthn::Credential>>*
          out_credentials) override;
  Response RemoveCredential(const String& in_authenticator_id,
                            const Binary& credential_id) override;
  Response ClearCredentials(const String& in_authenticator_id) override;
  Response SetUserVerified(const String& authenticator_id,
                           bool is_user_verified) override;

 private:
  // Finds the authenticator with the given |id|. Returns Response::OK() if
  // successful, an error otherwise.
  Response FindAuthenticator(const String& id,
                             VirtualAuthenticator** out_authenticator);
  RenderFrameHostImpl* frame_host_ = nullptr;
  VirtualFidoDiscoveryFactory* virtual_discovery_factory_ = nullptr;
  DISALLOW_COPY_AND_ASSIGN(WebAuthnHandler);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_WEBAUTHN_HANDLER_H_
