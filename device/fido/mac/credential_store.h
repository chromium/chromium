// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_CREDENTIAL_STORE_H_
#define DEVICE_FIDO_MAC_CREDENTIAL_STORE_H_

#include <list>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/apple/foundation_util.h"
#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "device/fido/mac/authenticator_config.h"
#include "device/fido/mac/credential_metadata.h"
#include "device/fido/platform_credential_store.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_user_entity.h"

#if defined(__OBJC__)
@class LAContext;
#endif

namespace device::fido::mac {

// Credential represents a WebAuthn credential from the keychain.
struct COMPONENT_EXPORT(DEVICE_FIDO) Credential {
  Credential(base::apple::ScopedCFTypeRef<SecKeyRef> private_key,
             std::vector<uint8_t> credential_id,
             CredentialMetadata metadata,
             std::string rp_id);
  Credential(const Credential&);
  Credential(Credential&& other);
  Credential& operator=(const Credential&);
  Credential& operator=(Credential&&);
  ~Credential();

  bool operator==(const Credential&) const;

  // Returns whether the user must be verified via Touch ID or device unlock
  // in order to sign with the secure enclave key pair for this credential.
  bool RequiresUvForSignature() const;

  // An opaque reference to the private key that can be used for signing.
  base::apple::ScopedCFTypeRef<SecKeyRef> private_key;

  // The credential ID is a handle to the key that gets passed to the RP. This
  // ID is opaque to the RP, but is obtained by encrypting the credential
  // metadata with a profile-specific metadata secret. See |CredentialMetadata|
  // for more information.
  std::vector<uint8_t> credential_id;

  CredentialMetadata metadata;

  std::string rp_id;
};

// TouchIdCredentialStore allows operations on Touch ID platform authenticator
// credentials stored in the macOS keychain.
//
// The Touch ID authenticator creates keychain items in the "iOS-style"
// keychain, which scopes item access based on the application-identifier or
// keychain-access-group entitlements, and therefore requires code signing with
// a real Apple developer ID.
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

#if defined(__OBJC__)
  // An LAContext that has been successfully evaluated using |TouchIdContext|
  // may be passed in |authentication_context|, in order to authorize
  // credentials returned by the `Find*` instance methods for signing without
  // triggering a Touch ID prompt.
  void SetAuthenticationContext(LAContext* authentication_context);
#endif  // __OBJC__

  // CreateCredential inserts a new credential into the keychain. It returns
  // the new credential and its public key, or std::nullopt if an error
  // occurred.
  std::optional<std::pair<Credential, base::apple::ScopedCFTypeRef<SecKeyRef>>>
  CreateCredential(const std::string& rp_id,
                   const PublicKeyCredentialUserEntity& user,
                   Discoverable discoverable) const;

  // CreateCredentialLegacyCredentialForTesting inserts a credential for an old
  // `CredentialMetadata::Version`. Such credentials can't be created anymore,
  // but they still exist and we need to be able to exercise them.
  std::optional<std::pair<Credential, base::apple::ScopedCFTypeRef<SecKeyRef>>>
  CreateCredentialLegacyCredentialForTesting(
      CredentialMetadata::Version version,
      const std::string& rp_id,
      const PublicKeyCredentialUserEntity& user,
      Discoverable discoverable) const;

  // FindCredentialsFromCredentialDescriptorList returns all credentials that
  // match one of the given |descriptors| and belong to |rp_id|. A descriptor
  // matches a credential if its transports() set is either empty or contains
  // FidoTransportProtocol::kInternal, and if its id() is the credential ID.
  // The returned credentials may be discoverable or non-discoverable. If any
  // unexpected keychain API error occurs, std::nullopt is returned instead.
  std::optional<std::list<Credential>>
  FindCredentialsFromCredentialDescriptorList(
      const std::string& rp_id,
      const std::vector<PublicKeyCredentialDescriptor>& descriptors) const;

  // FindResidentCredentials returns the client-side discoverable credentials
  // for the given |rp_id|. If |rp_id| is not specified, all resident
  // credentials are returned. nullopt is returned if an error occurred.
  std::optional<std::list<Credential>> FindResidentCredentials(
      const std::optional<std::string>& rp_id) const;

  // DeleteCredentialsForUserId deletes all credentials for the given RP and
  // user ID. Returns true if deleting succeeded or no matching credential
  // exists, and false if an error occurred.
  bool DeleteCredentialsForUserId(const std::string& rp_id,
                                  const std::vector<uint8_t>& user_id) const;

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

  bool DeleteCredentialById(base::span<const uint8_t> credential_id) const;

  bool UpdateCredential(base::span<uint8_t> credential_id,
                        const std::string& username);

  size_t CountCredentialsSync(base::Time created_not_before,
                              base::Time created_not_after);

  // Returns all credentials for the given `rp_id` (resident and non-resident).
  static std::vector<Credential> FindCredentialsForTesting(
      AuthenticatorConfig config,
      std::string rp_id);

 private:
  std::optional<std::list<Credential>> FindCredentialsImpl(
      const std::optional<std::string>& rp_id,
      const std::set<std::vector<uint8_t>>& credential_ids) const;

  AuthenticatorConfig config_;

  struct ObjCStorage;
  std::unique_ptr<ObjCStorage> objc_storage_;
};

}  // namespace device::fido::mac

#endif  // DEVICE_FIDO_MAC_CREDENTIAL_STORE_H_
