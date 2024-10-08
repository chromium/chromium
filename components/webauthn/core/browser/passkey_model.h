// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_MODEL_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_MODEL_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/webauthn/core/browser/passkey_model_change.h"

namespace base {
class Location;
}

namespace sync_pb {
class WebauthnCredentialSpecifics;
}

namespace syncer {
class DataTypeControllerDelegate;
}

namespace webauthn {

// PasskeyModel provides access to passkeys, which are represented as
// WebauthnCredentialSpecifics in Sync.
//
// By design, non-syncing passkeys are not supported (unlike passwords e.g.,
// which can be created on the local device only, without uploading them to
// Sync).
//
// Aside from Sync passkeys, there might be other WebAuthn platform credentials
// on the device. E.g., non-passkey credentials in the browser-provided
// authenticators on CrOS (u2fd) and macOS (//device/fido/mac); or platform
// credentials owned by Windows Hello or iCloud Keychain. None of these are
// accessible though PasskeyModel.
class PasskeyModel : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Notifies the observer that passkeys have changed, e.g. because a new one
    // was downloaded or deleted.
    virtual void OnPasskeysChanged(
        const std::vector<PasskeyModelChange>& changes) = 0;

    // Notifies the observer that the passkey model is shutting down.
    virtual void OnPasskeyModelShuttingDown() = 0;

    // Notifies the observer when the passkey model becomes ready.
    virtual void OnPasskeyModelIsReady(bool is_ready) = 0;
  };

  // Represents the WebAuthn PublicKeyCredentialUserEntity passed by the Relying
  // Party during registration.
  struct UserEntity {
    UserEntity(std::vector<uint8_t> id,
               std::string name,
               std::string display_name);
    ~UserEntity();
    std::vector<uint8_t> id;
    std::string name;
    std::string display_name;
  };

  // Attributes of a passkey that can be updated. If an attribute is set to
  // empty, then the entity attribute will also be set to empty.
  struct PasskeyUpdate {
    std::string user_name;
    std::string user_display_name;
  };

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns the sync DataTypeControllerDelegate for the WEBAUTHN_CREDENTIAL
  // data type.
  virtual base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetDataTypeControllerDelegate() = 0;

  // Returns true if the model has finished loading state from disk and is ready
  // to sync.
  virtual bool IsReady() const = 0;

  // Returns true if there are no passkeys in the account.
  virtual bool IsEmpty() const = 0;

  virtual base::flat_set<std::string> GetAllSyncIds() const = 0;

  // Returns the list of all passkeys, including those that are shadowed.
  virtual std::vector<sync_pb::WebauthnCredentialSpecifics> GetAllPasskeys()
      const = 0;

  // Returns the passkey matching the given Relying Party and credential ID, if
  // any. Shadowed entities, which aren't suitable for generating assertions,
  // are ignored.
  virtual std::optional<sync_pb::WebauthnCredentialSpecifics>
  GetPasskeyByCredentialId(const std::string& rp_id,
                           const std::string& credential_id) const = 0;

  // Returns all passkeys for the given Relying Party ID. Shadowed entities,
  // which aren't suitable for generating assertions, are ignored.
  virtual std::vector<sync_pb::WebauthnCredentialSpecifics>
  GetPasskeysForRelyingPartyId(const std::string& rp_id) const = 0;

  // Deletes the passkey with the given `credential_id`. If the passkey is the
  // head of the shadow chain, then all passkeys for the same (user id, rp id)
  // are deleted as well. `location` is used for logging purposes and
  // investigations. Returns true if a passkey was found and deleted, false
  // otherwise.
  virtual bool DeletePasskey(const std::string& credential_id,
                             const base::Location& location) = 0;

  // Deletes all passkeys.
  virtual void DeleteAllPasskeys() = 0;

  // Updates attributes of the passkey with the given `credential_id`. Returns
  // true if the credential was found and updated, false otherwise.
  // |updated_by_user| should be true if the user explicitly requested this
  // update, e.g. through the password manager. Passkeys updated by the user
  // will be permantently marked as such. Any further attempts to update the
  // passkey with |updated_by_user| set to |false| will be dropped.
  virtual bool UpdatePasskey(const std::string& credential_id,
                             PasskeyUpdate change,
                             bool updated_by_user) = 0;

  // Updates the `last_used_time` attribute of the passkey with the given
  // `credential_id`. Returns true if the credential was found and updated,
  // false otherwise.
  virtual bool UpdatePasskeyTimestamp(const std::string& credential_id,
                                      base::Time last_used_time) = 0;

  // Creates a passkey for the given RP and user and returns the new entity
  // specifics.
  //
  // The returned entity's `encrypted` field will contain an
  // `WebauthnCredentialSpecifics_EncryptedData` message, encrypted to
  // `trusted_vault_key`. `public_key_spki_der_out`, if non-null, will be set to
  // a DER-serialized X.509 SubjectPublicKeyInfo of the corresponding public
  // key.
  //
  // Any existing passkeys for the same RP and user ID are shadowed (i.e., added
  // to the new passkey's `newly_shadowed_credential_ids`), causing them not to
  // be enumerated by other methods in the model.
  virtual sync_pb::WebauthnCredentialSpecifics CreatePasskey(
      std::string_view rp_id,
      const UserEntity& user_entity,
      base::span<const uint8_t> trusted_vault_key,
      int32_t trusted_vault_key_version,
      std::vector<uint8_t>* public_key_spki_der_out) = 0;

  // Creates a passkey from a pre-constructed protobuf.
  // Existing passkeys for the same RP and user ID will be shadowed, but
  // otherwise all fields in the entity should be filled by the caller.
  virtual void CreatePasskey(sync_pb::WebauthnCredentialSpecifics& passkey) = 0;

  // Inserts the given passkey specifics into the model and returns the entity
  // sync_id.
  virtual std::string AddNewPasskeyForTesting(
      sync_pb::WebauthnCredentialSpecifics passkey) = 0;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_MODEL_H_
