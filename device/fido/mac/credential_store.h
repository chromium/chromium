// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_CREDENTIAL_STORE_H_
#define DEVICE_FIDO_MAC_CREDENTIAL_STORE_H_

#include <os/availability.h>

#include <list>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/mac/foundation_util.h"
#include "base/optional.h"
#include "device/fido/mac/authenticator_config.h"
#include "device/fido/mac/credential_metadata.h"
#include "device/fido/platform_credential_store.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_user_entity.h"

namespace device {
namespace fido {
namespace mac {

class LAContext;

// Credential represents a WebAuthn credential from the keychain.
struct COMPONENT_EXPORT(FIDO) Credential {
  Credential(base::ScopedCFTypeRef<SecKeyRef> private_key,
             std::vector<uint8_t> credential_id);
  ~Credential();
  Credential(const Credential&) = delete;
  Credential& operator=(const Credential&) = delete;
  Credential(Credential&& other);
  Credential& operator=(Credential&& other);

  // An opaque reference to the private key that can be used for signing.
  base::ScopedCFTypeRef<SecKeyRef> private_key;

  // The credential ID is a handle to the key that gets passed to the RP. This
  // ID is opaque to the RP, but is obtained by encrypting the credential
  // metadata with a profile-specific metadata secret. See |CredentialMetadata|
  // for more information.
  std::vector<uint8_t> credential_id;
};

// TouchIdCredentialStore allows operations on Touch ID platform authenticator
// credentials stored in the macOS keychain.
class COMPONENT_EXPORT(DEVICE_FIDO) TouchIdCredentialStore
    : public ::device::fido::PlatformCredentialStore {
 public:
  explicit TouchIdCredentialStore(AuthenticatorConfig config);
  ~TouchIdCredentialStore() override;

  // An LAContext that has been successfully evaluated using |TouchIdContext|
  // may be passed in |authenticaton_context|, in order to authorize
  // credentials returned by the other instance methods for signing without
  // triggering a Touch ID prompt.
  void set_authentication_context(LAContext* authentication_context) {
    authentication_context_ = authentication_context;
  }

  // CreateCredential inserts a new credential into the keychain. It returns
  // the new credential and its public key, or base::nullopt if an error
  // occurred.
  API_AVAILABLE(macosx(10.12.2))
  base::Optional<std::pair<Credential, base::ScopedCFTypeRef<SecKeyRef>>>
  CreateCredential(const std::string& rp_id,
                   const PublicKeyCredentialUserEntity& user,
                   bool is_resident,
                   SecAccessControlRef access_control) const;

  // FindCredentialsFromCredentialDescriptorList returns all credentials that
  // match one of the given |descriptors| and belong to |rp_id|. A descriptor
  // matches a credential if its transports() set is either empty or contains
  // FidoTransportProtocol::kInternal, and if its id() is the credential ID.
  // The returned credentials may be resident or non-resident. If any
  // unexpected keychain API error occurs, base::nullopt is returned instead.
  API_AVAILABLE(macosx(10.12.2))
  base::Optional<std::list<Credential>>
  FindCredentialsFromCredentialDescriptorList(
      const std::string& rp_id,
      const std::vector<PublicKeyCredentialDescriptor>& descriptors) const;

  // FindResidentCredentials returns the resident credentials for the given
  // |rp_id|, or base::nulltopt if an error occurred.
  API_AVAILABLE(macosx(10.12.2))
  base::Optional<std::list<Credential>> FindResidentCredentials(
      const std::string& rp_id) const;

  // UnsealMetadata returns the CredentialMetadata for the given credential's
  // ID if it was encoded for the given RP ID, or base::nullopt otherwise.
  API_AVAILABLE(macosx(10.12.2))
  base::Optional<CredentialMetadata> UnsealMetadata(
      const std::string& rp_id,
      const Credential& credential) const;

  // DeleteCredentialsForUserId deletes all (resident or non-resident)
  // credentials for the given RP and user ID. Returns true if deleting
  // succeeded or no matching credential exists, and false otherwise.
  API_AVAILABLE(macosx(10.12.2))
  bool DeleteCredentialsForUserId(const std::string& rp_id,
                                  base::span<const uint8_t> user_id) const;

  // PlatformCredentialStore:

  // DeleteCredentials deletes Touch ID authenticator credentials from
  // the macOS keychain that were created within the given time interval and
  // with the given metadata secret (which is tied to a browser profile). The
  // |keychain_access_group| parameter is an identifier tied to Chrome's code
  // signing identity that identifies the set of all keychain items associated
  // with the Touch ID WebAuthentication authenticator.
  //
  // Returns false if any attempt to delete a credential failed (but others may
  // still have succeeded), and true otherwise.
  //
  // On platforms where Touch ID is not supported, or when the Touch ID WebAuthn
  // authenticator feature flag is disabled, this method does nothing and
  // returns true.
  bool DeleteCredentials(base::Time created_not_before,
                         base::Time created_not_after) override;

  // CountCredentials returns the number of credentials that would get
  // deleted by a call to |DeleteWebAuthnCredentials| with identical arguments.
  size_t CountCredentials(base::Time created_not_before,
                          base::Time created_not_after) override;

 private:
  API_AVAILABLE(macosx(10.12.2))
  base::Optional<std::list<Credential>> FindCredentialsImpl(
      const std::string& rp_id,
      const std::set<std::vector<uint8_t>>& credential_ids) const;

  AuthenticatorConfig config_;
  LAContext* authentication_context_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TouchIdCredentialStore);
};

}  // namespace mac
}  // namespace fido
}  // namespace device

#endif  // DEVICE_FIDO_MAC_CREDENTIAL_STORE_H_
