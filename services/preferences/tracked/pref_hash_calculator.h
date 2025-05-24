// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_TRACKED_PREF_HASH_CALCULATOR_H_
#define SERVICES_PREFERENCES_TRACKED_PREF_HASH_CALCULATOR_H_

#include <string>

#include "base/values.h"
#include "components/os_crypt/async/common/encryptor.h"

// Calculates and validates preference value hashes.
class PrefHashCalculator {
 public:
  enum ValidationResult {
    INVALID,
    VALID,
    // Valid under a deprecated but as secure algorithm.
    VALID_SECURE_LEGACY,
    VALID_ENCRYPTED,
    INVALID_ENCRYPTED,
  };

  // Constructs a PrefHashCalculator using |seed|, |device_id| and
  // |legacy_device_id|. The same parameters must be used in order to
  // successfully validate generated hashes. |_device_id| or |legacy_device_id|
  // may be empty.
  PrefHashCalculator(const std::string& seed,
                     const std::string& device_id,
                     const std::string& legacy_device_id);

  PrefHashCalculator(const PrefHashCalculator&) = delete;
  PrefHashCalculator& operator=(const PrefHashCalculator&) = delete;

  ~PrefHashCalculator();

  // Calculates a hash value for the supplied preference |path| and |value|.
  // |value| may be null if the preference has no value.
  std::string Calculate(const std::string& path,
                        const base::Value* value) const;
  std::string Calculate(const std::string& path,
                        const base::Value::Dict* dict) const;

  // Validates the provided preference hash using current and legacy hashing
  // algorithms.
  ValidationResult Validate(const std::string& path,
                            const base::Value* value,
                            const std::string& hash) const;
  ValidationResult Validate(const std::string& path,
                            const base::Value::Dict* dict,
                            const std::string& hash) const;

  // Calculates the OS-encrypted SHA256 hash of the preference's |path| and
  // |value|. Requires a non-null |encryptor|. Returns an empty string on
  // failure.
  std::optional<std::string> CalculateEncryptedHash(
      const std::string& path,
      const base::Value* value,
      const os_crypt_async::Encryptor* encryptor) const;

  // Calculates the OS-encrypted SHA256 hash of the preference's |path| and
  // dictionary |dict|. Requires a non-null |encryptor|. Returns an empty string
  // on failure.
  std::optional<std::string> CalculateEncryptedHash(
      const std::string& path,
      const base::Value::Dict* dict,
      const os_crypt_async::Encryptor* encryptor) const;

  // Validates the OS-encrypted SHA256 |stored_encrypted_hash| of the
  // preference's |path| and |value|. Requires a non-null |encryptor|.
  ValidationResult ValidateEncrypted(
      const std::string& path,
      const base::Value* value,
      const std::string& stored_encrypted_hash,
      const os_crypt_async::Encryptor* encryptor) const;

 private:
  ValidationResult Validate(const std::string& path,
                            const std::string& value_as_string,
                            const std::string& hash) const;

  const std::string seed_;
  const std::string device_id_;
  const std::string legacy_device_id_;
};

#endif  // SERVICES_PREFERENCES_TRACKED_PREF_HASH_CALCULATOR_H_
