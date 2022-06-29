// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_CREDENTIAL_STORE_H_
#define DEVICE_FIDO_MAC_CREDENTIAL_STORE_H_

#include <list>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/mac/foundation_util.h"
#include "base/memory/raw_ptr.h"
#include "device/fido/mac/authenticator_config.h"
#include "device/fido/mac/credential_metadata.h"
#include "device/fido/platform_credential_store.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if defined(__OBJC__)
@class LAContext;
#else
class LAContext;
#endif

namespace device::fido::mac {

// Credential represents a WebAuthn credential from the keychain.
struct COMPONENT_EXPORT(DEVICE_FIDO) Credential {
  Credential(base::ScopedCFTypeRef<SecKeyRef> private_key,
             std::vector<uint8_t> credential_id);
  Credential(const Credential&);
  Credential(Credential&& other);
  Credential& operator=(const Credential&);
  Credential& operator=(Credential&&);
  ~Credential();

  bool operator==(const Credential&) const;

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
    : public device::fido::PlatformCredentialStore {
 public:
  // Indicates whether a created credential should be client-side discoverable
  // (formerly known as "resident keys").
  enum Discoverable { kNonDiscoverable, kDiscoverable };

  explicit TouchIdCredentialStore(AuthenticatorConfig config);
  TouchIdCredentialStore(const TouchIdCredentialStore&) = delete;
  TouchIdCredentialStore& operator=(const TouchIdCredentialStore&) = delete;
  ~TouchIdCredentialStore() override;

  // Returns the access control object used when creating platform authenticator
  // credentials.
  static base::ScopedCFTypeRef<SecAccessControlRef> DefaultAccessControl();

  // An LAContext that has been successfully evaluated using |TouchIdContext|
  // may be passed in |authenticaton_context|, in order to authorize
  // credentials returned by the `Find*` instance methods for signing without
  // triggering a Touch ID prompt.
  void set_authentication_context(LAContext* authentication_context) {
    authentication_context_ = authentication_context;
  }

  // CreateCredential inserts a new credential into the keychain. It returns
  // the new credential and its public key, or absl::nullopt if an error
  // occurred.
  absl::optional<std::pair<Credential, base::ScopedCFTypeRef<SecKeyRef>>>
  CreateCredential(const std::string& rp_id,
                   const PublicKeyCredentialUserEntity& user,
                   Discoverable discoverable) const;

  // FindCredentialsFromCredentialDescriptorList returns all credentials that
  // match one of the given |descriptors| and belong to |rp_id|. A descriptor
  // matches a credential if its transports() set is either empty or contains
  // FidoTransportProtocol::kInternal, and if its id() is the credential ID.
  // The returned credentials may be discoverable or non-discoverable. If any
  // unexpected keychain API error occurs, absl::nullopt is returned instead.
  absl::optional<std::list<Credential>>
  FindCredentialsFromCredentialDescriptorList(
      const std::string& rp_id,
      const std::vector<PublicKeyCredentialDescriptor>& descriptors) const;

  // FindResidentCredentials returns the client-side discoverable credentials
  // for the given |rp_id|, or base::nulltopt if an error occurred.
  absl::optional<std::list<Credential>> FindResidentCredentials(
      const std::string& rp_id) const;

  // UnsealMetadata returns the CredentialMetadata for the given credential's
  // ID if it was encoded for the given RP ID, or absl::nullopt otherwise.
  absl::optional<CredentialMetadata> UnsealMetadata(
      const std::string& rp_id,
      const Credential& credential) const;

  // DeleteCredentialsForUserId deletes all credentials for the given RP and
  // user ID. Returns true if deleting succeeded or no matching credential
  // exists, and false if an error occurred.
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

  // Returns all credentials for the given `rp_id` (resident and non-resident).
  static std::vector<std::pair<Credential, CredentialMetadata>>
  FindCredentialsForTesting(AuthenticatorConfig config, std::string rp_id);

 private:
  absl::optional<std::list<Credential>> FindCredentialsImpl(
      const std::string& rp_id,
      absl::optional<base::span<const uint8_t>> user_id,
      const std::set<std::vector<uint8_t>>& credential_ids) const;

  bool DeleteCredentialById(base::span<const uint8_t> credential_id) const;

  AuthenticatorConfig config_;
  LAContext* authentication_context_ = nullptr;
};

}  // namespace device::fido::mac

#endif  // DEVICE_FIDO_MAC_CREDENTIAL_STORE_H_
