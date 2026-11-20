// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_TRACKED_PREF_HASH_CALCULATOR_H_
#define SERVICES_PREFERENCES_TRACKED_PREF_HASH_CALCULATOR_H_

#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "components/os_crypt/async/common/encryptor.h"

// Calculates and validates preference value authenticators, both HMACs (legacy)
// and encrypted hashes (modern).
class PrefHashCalculator {
 public:
  enum ValidationResult {
    INVALID_HMAC,
    VALID_HMAC,
    VALID_ENCRYPTED,
    // The encrypted hash was decrypted but does not match the value, or it is
    // malformed. This is treated as tampering.
    INVALID_ENCRYPTED,
    WEAK_HASH_ENCRYPTED,
    // The encrypted hash could not be decrypted with the current key, e.g.
    // because the OS key was rotated or recreated. This indicates key loss
    // rather than tampering of the value.
    UNDECRYPTABLE_ENCRYPTED,
    // The key that the encrypted hash was encrypted with is unavailable, as
    // reported by os_crypt. This usually means the user hasn't unlocked the
    // keychain yet or there was a transient OS error, but it is also used for
    // keys that are intentionally unavailable (e.g. the default key when
    // running with a non-default user data directory on macOS). This
    // indicates key loss rather than tampering of the value.
    UNAVAILABLE_ENCRYPTED,
  };

  // Returns true if `result` indicates that an encrypted hash could not be
  // decrypted because its key is unavailable or has changed, as opposed to the
  // value having been tampered with.
  static bool IsKeyLoss(ValidationResult result);

  // Constructs a PrefHashCalculator using |seed_|, |device_id_|.
  // The same parameters must be used in order to
  // successfully validate generated hashes. |device_id_| may be empty.
  PrefHashCalculator(const std::string& seed, const std::string& device_id);

  PrefHashCalculator(const PrefHashCalculator&) = delete;
  PrefHashCalculator& operator=(const PrefHashCalculator&) = delete;

  ~PrefHashCalculator();

  // Calculates an HMAC value for the supplied preference |path| and |value|.
  // |value| may be null if the preference has no value.
  std::string CalculateHmac(const std::string& path,
                            const base::Value* value) const;
  std::string CalculateHmac(const std::string& path,
                            const base::DictValue* dict) const;

  // Validates the provided preference HMAC using current and legacy hashing
  // algorithms.
  ValidationResult ValidateHmac(const std::string& path,
                                const base::Value* value,
                                const std::string& hash) const;
  ValidationResult ValidateHmac(const std::string& path,
                                const base::DictValue* dict,
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
      const base::DictValue* dict,
      const os_crypt_async::Encryptor* encryptor) const;

  // Validates the OS-encrypted SHA256 |stored_encrypted_hash| of the
  // preference's |path| and |value|. Requires a non-null |encryptor|.
  ValidationResult ValidateEncryptedHash(
      const std::string& path,
      const base::Value* value,
      const std::string& stored_encrypted_hash,
      const os_crypt_async::Encryptor* encryptor) const;

 private:
  // HMAC concat(|device_id_|, |path|, |value|) with key |seed_|. Returns a
  // hex-encoded signature value, which HmacVerify() accepts as |signature|.
  std::string HmacSign(std::string_view path, std::string_view value) const;
  [[nodiscard]] bool HmacVerify(std::string_view path,
                                std::string_view value,
                                std::string_view signature) const;

  // Hash concat(|seed_|, |path|, |value|) and return the hash bytes as a
  // std::string, unencoded.
  std::string Hash(std::string_view path, std::string_view value) const;

  ValidationResult ValidateHmac(const std::string& path,
                                const std::string& value_as_string,
                                const std::string& hash) const;

  const std::string seed_;
  const std::string device_id_;
};

#endif  // SERVICES_PREFERENCES_TRACKED_PREF_HASH_CALCULATOR_H_
