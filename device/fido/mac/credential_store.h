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

#include "base/callback.h"
#include "base/component_export.h"
#include "base/mac/foundation_util.h"
#include "device/fido/mac/authenticator_config.h"
#include "device/fido/mac/credential_metadata.h"
#include "device/fido/platform_credential_store.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

  TouchIdCredentialStore(const TouchIdCredentialStore&) = delete;
  TouchIdCredentialStore& operator=(const TouchIdCredentialStore&) = delete;

  ~TouchIdCredentialStore() override;

  // An LAContext that has been successfully evaluated using |TouchIdContext|
  // may be passed in |authenticaton_context|, in order to authorize
  // credentials returned by the other instance methods for signing without
  // triggering a Touch ID prompt.
  void set_authentication_context(LAContext* authentication_context) {
    authentication_context_ = authentication_context;
  }

  // CreateCredential inserts a new credential into the keychain. It returns
  // the new credential and its public key, or absl::nullopt if an error
  // occurred.
  API_AVAILABLE(macosx(10.12.2))
  absl::optional<std::pair<Credential, base::ScopedCFTypeRef<SecKeyRef>>>
  CreateCredential(const std::string& rp_id,
                   const PublicKeyCredentialUserEntity& user,
                   bool is_resident,
                   SecAccessControlRef access_control) const;

  // FindCredentialsFromCredentialDescriptorList returns all credentials that
  // match one of the given |descriptors| and belong to |rp_id|. A descriptor
  // matches a credential if its transports() set is either empty or contains
  // FidoTransportProtocol::kInternal, and if its id() is the credential ID.
  // The returned credentials may be resident or non-resident. If any
  // unexpected keychain API error occurs, absl::nullopt is returned instead.
  API_AVAILABLE(macosx(10.12.2))
  absl::optional<std::list<Credential>>
  FindCredentialsFromCredentialDescriptorList(
      const std::string& rp_id,
      const std::vector<PublicKeyCredentialDescriptor>& descriptors) const;

  // FindResidentCredentials returns the resident credentials for the given
  // |rp_id|, or base::nulltopt if an error occurred.
  API_AVAILABLE(macosx(10.12.2))
  absl::optional<std::list<Credential>> FindResidentCredentials(
      const std::string& rp_id) const;

  // UnsealMetadata returns the CredentialMetadata for the given credential's
  // ID if it was encoded for the given RP ID, or absl::nullopt otherwise.
  API_AVAILABLE(macosx(10.12.2))
  absl::optional<CredentialMetadata> UnsealMetadata(
      const std::string& rp_id,
      const Credential& credential) const;

  // DeleteCredentialsForUserId deletes all (resident or non-resident)
  // credentials for the given RP and user ID. Returns true if deleting
  // succeeded or no matching credential exists, and false otherwise.
  API_AVAILABLE(macosx(10.12.2))
  bool DeleteCredentialsForUserId(const std::string& rp_id,
                                  base::span<const uint8_t> user_id) const;

  // PlatformCredentialStore:

  void DeleteCredentials(base::Time created_not_before,
                         base::Time created_not_after,
                         base::OnceClosure callback) override;

  void CountCredentials(base::Time created_not_before,
                        base::Time created_not_after,
                        base::OnceCallback<void(size_t)> callback) override;

  // Sync versions of the two above APIs.
  bool DeleteCredentialsSync(base::Time created_not_before,
                             base::Time created_not_after);

  size_t CountCredentialsSync(base::Time created_not_before,
                              base::Time created_not_after);

 private:
  API_AVAILABLE(macosx(10.12.2))
  absl::optional<std::list<Credential>> FindCredentialsImpl(
      const std::string& rp_id,
      const std::set<std::vector<uint8_t>>& credential_ids) const;

  AuthenticatorConfig config_;
  LAContext* authentication_context_ = nullptr;
};

}  // namespace mac
}  // namespace fido
}  // namespace device

#endif  // DEVICE_FIDO_MAC_CREDENTIAL_STORE_H_
