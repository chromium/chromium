// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_TRACKED_HASH_STORE_CONTENTS_H_
#define SERVICES_PREFERENCES_TRACKED_HASH_STORE_CONTENTS_H_

#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "base/values.h"

// Provides access to the contents of a preference hash store. The store
//   contains the following data:
// Contents: a client-defined dictionary that should map preference names to
//   authenticators. We currently use two types of authenticator: encrypted
//   hashes (modern) and HMACs (legacy fallback).
// Version: a client-defined version number for the format of Contents.
// Super authenticators: an authenticator that validates the entirety of
//   Contents. Just as we use two types of authenticator (encrypted hashes and
//   HMACs), we use two types of super authenticator: super encrypted hashes and
//   super HMACs.
class HashStoreContents {
 public:
  virtual ~HashStoreContents() {}

  // Returns true if this implementation of HashStoreContents can be copied via
  // MakeCopy().
  virtual bool IsCopyable() const = 0;

  // Returns a copy of this HashStoreContents. Must only be called on
  // lightweight implementations (which return true from IsCopyable()) and only
  // in scenarios where a copy cannot be avoided.
  virtual std::unique_ptr<HashStoreContents> MakeCopy() const = 0;

  // Returns the suffix to be appended to UMA histograms for this store type.
  // The returned value must either be an empty string or one of the values in
  // histograms.xml's TrackedPreferencesExternalValidators.
  virtual std::string_view GetUMASuffix() const = 0;

  // Discards all data related to this hash store.
  virtual void Reset() = 0;

  // Outputs the authenticator validating the atomic preference at path.
  // Returns true if an authenticator was successfully read and false otherwise.
  virtual bool GetAtomicPrefAuthenticator(const std::string& path,
                                          std::string* out_value) = 0;

  // Outputs the authenticators validating the split preference at path.
  // Returns true if authenticators were successfully read and false otherwise.
  virtual bool GetSplitPrefAuthenticators(
      const std::string& path,
      std::map<std::string, std::string>* out_value) = 0;

  // Sets the authenticator validating the atomic preference at path.
  virtual void SetAtomicPrefAuthenticator(const std::string& path,
                                          const std::string& value) = 0;

  // Sets the authenticator validating the split preference at path and
  // split_path. For example, |path| is 'extension' and |split_path| is some
  // extension id.
  virtual void SetSplitPrefAuthenticator(const std::string& path,
                                         const std::string& split_path,
                                         const std::string& value) = 0;

  // Sets the authenticator for the preference at |path|.
  // If |path| is a split preference |in_value| must be a DictionaryValue whose
  // keys are keys in the split preference and whose values are authenticators
  // of the corresponding values in the split preference. If |path| is an atomic
  // preference |in_value| must be a StringValue containing an authenticator of
  // the preference value.
  virtual void ImportAuthenticator(const std::string& path,
                                   const base::Value* in_value) = 0;

  // Removes the authenticator (for atomic preferences) or authenticators (for
  // split preferences) at |path|. Returns true if there was an entry at |path|
  // which was successfully removed.
  virtual bool RemoveAuthenticator(const std::string& path) = 0;

  // Returns true if this store supports super authenticators.
  virtual bool SupportsSuperAuthenticator() const = 0;

  // Only needed if this store supports super authenticators.
  virtual const base::DictValue* GetContents() const = 0;

  // Retrieves the super HMAC value previously stored by SetSuperHmac. May be
  // empty if no super HMAC has been stored or if this store does not support
  // super authenticators.
  virtual std::string GetSuperHmac() const = 0;

  // Stores a super HMAC value for this hash store.
  virtual void SetSuperHmac(const std::string& super_hmac) = 0;

  // Retrieves the super encrypted hash value previously stored by
  // SetSuperEncryptedHash. May be empty if no super encrypted hash has been
  // stored or if this store does not support it.
  virtual std::string GetSuperEncryptedHash() const = 0;

  // Stores a super encrypted hash value for this hash store.
  virtual void SetSuperEncryptedHash(
      const std::string& super_encrypted_hash) = 0;
};

#endif  // SERVICES_PREFERENCES_TRACKED_HASH_STORE_CONTENTS_H_
