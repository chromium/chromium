// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_TRACKED_PREF_HASH_STORE_TRANSACTION_H_
#define SERVICES_PREFERENCES_TRACKED_PREF_HASH_STORE_TRANSACTION_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/values.h"
#include "services/preferences/public/mojom/tracked_preference_validation_delegate.mojom.h"

// Used to perform a series of checks/transformations on a PrefHashStore.
class PrefHashStoreTransaction {
 public:
  // Finalizes any remaining work after the transaction has been performed.
  virtual ~PrefHashStoreTransaction() {}

  // Returns the suffix to be appended to UMA histograms for the store contained
  // in this transaction.
  virtual std::string_view GetStoreUMASuffix() const = 0;

  // Checks |initial_value| against the existing stored value hash.
  virtual prefs::mojom::TrackedPreferenceValidationDelegate::ValueState
  CheckValue(const std::string& path,
             const base::Value* initial_value,
             std::optional<size_t> reporting_id = std::nullopt) const = 0;

  // Stores an HMAC of the current |value| of the preference at |path|.
  virtual void StoreHmac(const std::string& path, const base::Value* value) = 0;

  // Checks |initial_value| against the existing stored hashes for the split
  // preference at |path|. |initial_split_value| being an empty dictionary or
  // NULL is equivalent. |invalid_keys| must initially be empty. |invalid_keys|
  // will not be modified unless the return value is CHANGED, in which case it
  // will be filled with the keys that are considered invalid (unknown or
  // changed).
  virtual prefs::mojom::TrackedPreferenceValidationDelegate::ValueState
  CheckSplitValue(const std::string& path,
                  const base::DictValue* initial_split_value,
                  std::vector<std::string>* invalid_keys,
                  std::optional<size_t> reporting_id = std::nullopt) const = 0;

  // Stores HMACs for the |value| of the split preference at |path|.
  // |split_value| being an empty dictionary or NULL is equivalent.
  virtual void StoreSplitHmac(const std::string& path,
                              const base::DictValue* split_value) = 0;

  // Indicates whether the store contains any authenticator (HMAC and / or
  // encrypted hash) for the preference at |path|.
  virtual bool HasAuthenticator(const std::string& path) const = 0;

  // Sets the authentication data for the preference at `path` to the values in
  // `auth_data`.
  //
  // If `auth_data` is a StringValue, `auth_data` will be imported as an HMAC of
  // the preference value.
  //
  // If `auth_data` is a DictValue, the encrypted hash and legacy HMAC will be
  // set based on the keys present in the dictionary:
  //   - If the `"mac"` key is present, its value will be imported as an HMAC of
  //     the preference value. If the key is absent, any existing HMAC for the
  //     preference will be cleared.
  //   - If the `"encrypted_hash"` key is present, its value will be imported as
  //     the encrypted hash for the preference value. If the key is absent,
  //     any existing encrypted hash for the preference will be cleared.
  //
  // Any HMACs in `auth_data` should originate from a PrefHashStore sharing the
  // same MAC parameters as this transaction's store.
  // The (in)validity of the super HMAC and super encrypted hash will be
  // maintained by this call.
  virtual void ImportAuthData(const std::string& path,
                              const base::Value* auth_data) = 0;

  // Removes all authenticators at the path `path`, if any exist. The
  // (in)validity of the super HMAC and super encrypted hash is maintained by
  // this call.
  virtual void ClearAuthenticators(const std::string& path) = 0;

  // Indicates whether the super HMAC was successfully verified at the beginning
  // of this transaction.
  virtual bool IsSuperHmacValid() const = 0;

  // Forces a valid super HMAC to be stored when this transaction terminates.
  // Returns true if this results in a change to the store contents.
  virtual bool StampSuperHmac() = 0;

  // Removes the encrypted hash for the authenticator path `path`.
  virtual void ClearEncryptedHash(const std::string& path) = 0;

  // Stores the OS-encrypted hash of the preference at |path| and |value|.
  // |value| may be NULL. Requires the encryptor to have been provided at
  // transaction start.
  virtual void StoreEncryptedHash(const std::string& path,
                                  const base::Value* value) = 0;

  // Stores the OS-encrypted hashes for the |value| of the split preference at
  // |path|. |value| being an empty dictionary or NULL is equivalent. Requires
  // the encryptor to have been provided at transaction start.
  virtual void StoreSplitEncryptedHash(const std::string& path,
                                       const base::DictValue* value) = 0;

  // Retrieves the stored OS-encrypted hash (Base64 encoded) for the
  // preference at |path|. Returns nullopt if no encrypted hash is stored.
  virtual std::optional<std::string> GetEncryptedHash(
      const std::string& path) const = 0;

  // Retrieves the stored legacy HMAC for the preference at |path|.
  // Returns nullopt if no HMAC is stored.
  virtual std::optional<std::string> GetHmac(const std::string& path) const = 0;

  // Returns true if an OS-encrypted hash is stored for the preference at
  // |path|. This could be an atomic hash or hashes for a split dictionary.
  virtual bool HasEncryptedHash(const std::string& path) const = 0;
};

#endif  // SERVICES_PREFERENCES_TRACKED_PREF_HASH_STORE_TRANSACTION_H_
