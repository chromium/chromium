// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_WEBAUTHN_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_WEBAUTHN_HANDLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/web_authn.h"
#include "content/browser/webauth/virtual_authenticator.h"
#include "content/common/content_export.h"
#include "device/fido/virtual_fido_device.h"

namespace content::protocol {

class WebAuthnHandler : public DevToolsDomainHandler,
                        public WebAuthn::Backend,
                        public VirtualAuthenticator::Observer {
 public:
  CONTENT_EXPORT WebAuthnHandler();
  CONTENT_EXPORT ~WebAuthnHandler() override;
  WebAuthnHandler(const WebAuthnHandler&) = delete;
  WebAuthnHandler operator=(const WebAuthnHandler&) = delete;

  // DevToolsDomainHandler:
  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;
  void Wire(UberDispatcher* dispatcher) override;

  // WebAuthn::Backend
  CONTENT_EXPORT Response Enable(Maybe<bool> enable_ui) override;
  CONTENT_EXPORT Response Disable() override;
  Response AddVirtualAuthenticator(
      std::unique_ptr<WebAuthn::VirtualAuthenticatorOptions> options,
      String* out_authenticator_id) override;
  Response RemoveVirtualAuthenticator(const String& authenticator_id) override;
  Response SetResponseOverrideBits(const String& authenticator_id,
                                   Maybe<bool> is_bogus_signature,
                                   Maybe<bool> is_bad_uv,
                                   Maybe<bool> is_bad_up) override;
  void AddCredential(const String& authenticator_id,
                     std::unique_ptr<protocol::WebAuthn::Credential> credential,
                     std::unique_ptr<AddCredentialCallback> callback) override;
  void GetCredential(const String& authenticator_id,
                     const Binary& credential_id,
                     std::unique_ptr<GetCredentialCallback> callback) override;
  void GetCredentials(
      const String& authenticator_id,
      std::unique_ptr<GetCredentialsCallback> callback) override;
  Response RemoveCredential(const String& in_authenticator_id,
                            const Binary& credential_id) override;
  Response ClearCredentials(const String& in_authenticator_id) override;
  Response SetUserVerified(const String& authenticator_id,
                           bool is_user_verified) override;
  Response SetAutomaticPresenceSimulation(const String& authenticator_id,
                                          bool enabled) override;
  Response SetCredentialProperties(const String& authenticator_id,
                                   const Binary& credential_id,
                                   Maybe<bool> backup_eligibility,
                                   Maybe<bool> backup_state) override;

 private:
  // Finds the authenticator with the given |id|. Returns Response::OK() if
  // successful, an error otherwise.
  Response FindAuthenticator(const String& id,
                             VirtualAuthenticator** out_authenticator);

  // VirtualAuthenticator::Observer:
  void OnCredentialCreated(
      VirtualAuthenticator* authenticator,
      const device::VirtualFidoDevice::Credential& credential) override;
  void OnCredentialDeleted(VirtualAuthenticator* authenticator,
                           base::span<const uint8_t> credential_id) override;
  void OnCredentialUpdated(
      VirtualAuthenticator* authenticator,
      const device::VirtualFidoDevice::Credential& credential) override;
  void OnAssertion(
      VirtualAuthenticator* authenticator,
      const device::VirtualFidoDevice::Credential& credential) override;
  void OnAuthenticatorWillBeDestroyed(
      VirtualAuthenticator* authenticator) override;

  raw_ptr<RenderFrameHostImpl> frame_host_ = nullptr;
  std::unique_ptr<WebAuthn::Frontend> frontend_;
  base::ScopedMultiSourceObservation<VirtualAuthenticator,
                                     VirtualAuthenticator::Observer>
      observations_{this};
};

}  // namespace content::protocol

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_WEBAUTHN_HANDLER_H_
