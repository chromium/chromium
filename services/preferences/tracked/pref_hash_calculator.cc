// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/tracked/pref_hash_calculator.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/enterprise_util.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "crypto/hash.h"
#include "crypto/hmac.h"
#include "crypto/secure_util.h"

namespace {

void RemoveEmptyValueDictEntries(base::Value::Dict& dict);
void RemoveEmptyValueListEntries(base::Value::List& list);

// Removes empty Dict and List Values from |dict|, potentially nested.
// This function may leave |dict| empty, and |dict| may be empty when passed in.
void RemoveEmptyValueDictEntries(base::Value::Dict& dict) {
  auto it = dict.begin();
  while (it != dict.end()) {
    base::Value& value = it->second;
    if (value.is_list()) {
      base::Value::List& sub_list = value.GetList();
      RemoveEmptyValueListEntries(sub_list);
      if (sub_list.empty()) {
        it = dict.erase(it);
        continue;
      }
    }
    if (value.is_dict()) {
      base::Value::Dict& sub_dict = value.GetDict();
      RemoveEmptyValueDictEntries(sub_dict);
      if (sub_dict.empty()) {
        it = dict.erase(it);
        continue;
      }
    }
    it++;
  }
}

// Removes empty Dict and List Values from |list|, potentially nested.
// This function may leave |list| empty, and |list| may be empty when passed in.
void RemoveEmptyValueListEntries(base::Value::List& list) {
  auto it = list.begin();
  while (it != list.end()) {
    base::Value& item = *it;
    if (item.is_list()) {
      base::Value::List& sub_list = item.GetList();
      RemoveEmptyValueListEntries(sub_list);
      if (sub_list.empty()) {
        it = list.erase(it);
        continue;
      }
    }
    if (item.is_dict()) {
      base::Value::Dict& sub_dict = item.GetDict();
      RemoveEmptyValueDictEntries(sub_dict);
      if (sub_dict.empty()) {
        it = list.erase(it);
        continue;
      }
    }
    it++;
  }
}

// Renders |value| as a string. |value| may be NULL, in which case the result
// is an empty string. This method can be expensive and its result should be
// re-used rather than recomputed where possible.

std::string ValueAsString(const base::Value::Dict* value) {
  if (!value)
    return std::string();

  base::Value::Dict dict = value->Clone();
  RemoveEmptyValueDictEntries(dict);
  return base::WriteJson(dict).value_or(std::string());
}

std::string ValueAsString(const base::Value* value) {
  if (!value)
    return std::string();

  if (value->is_dict())
    return ValueAsString(&value->GetDict());

  return base::WriteJson(*value).value_or(std::string());
}

}  // namespace

PrefHashCalculator::PrefHashCalculator(const std::string& seed,
                                       const std::string& device_id)
    : seed_(seed), device_id_(device_id) {}

PrefHashCalculator::~PrefHashCalculator() {}

std::string PrefHashCalculator::Calculate(const std::string& path,
                                          const base::Value* value) const {
  return HmacSign(path, ValueAsString(value));
}

std::string PrefHashCalculator::Calculate(const std::string& path,
                                          const base::Value::Dict* dict) const {
  return HmacSign(path, ValueAsString(dict));
}

PrefHashCalculator::ValidationResult PrefHashCalculator::Validate(
    const std::string& path,
    const base::Value* value,
    const std::string& digest_string) const {
  return Validate(path, ValueAsString(value), digest_string);
}

PrefHashCalculator::ValidationResult PrefHashCalculator::Validate(
    const std::string& path,
    const base::Value::Dict* dict,
    const std::string& digest_string) const {
  return Validate(path, ValueAsString(dict), digest_string);
}

PrefHashCalculator::ValidationResult PrefHashCalculator::Validate(
    const std::string& path,
    const std::string& value_as_string,
    const std::string& digest_string) const {
#if BUILDFLAG(IS_WIN)
  // On enterprise-managed devices, bypass legacy HMAC validation. This is to
  // support roaming user profiles, where the device-specific HMAC would fail
  // upon roaming. Preference integrity on these devices is maintained by the
  // encrypted hash.
  if (base::IsEnterpriseDevice()) {
    return VALID;
  }
#endif
  return HmacVerify(path, value_as_string, digest_string) ? VALID : INVALID;
}

std::optional<std::string> PrefHashCalculator::CalculateEncryptedHash(
    const std::string& path,
    const base::Value* value,
    const os_crypt_async::Encryptor* encryptor) const {
  DCHECK(encryptor);

  std::optional<std::vector<uint8_t>> encrypted_bytes =
      encryptor->EncryptString(Hash(path, ValueAsString(value)));

  if (!encrypted_bytes) {
    return std::nullopt;
  }

  return base::Base64Encode(*encrypted_bytes);
}

std::optional<std::string> PrefHashCalculator::CalculateEncryptedHash(
    const std::string& path,
    const base::Value::Dict* dict,
    const os_crypt_async::Encryptor* encryptor) const {
  DCHECK(encryptor);

  std::optional<std::vector<uint8_t>> encrypted_bytes =
      encryptor->EncryptString(Hash(path, ValueAsString(dict)));

  if (!encrypted_bytes) {
    return std::nullopt;
  }

  return base::Base64Encode(*encrypted_bytes);
}

PrefHashCalculator::ValidationResult PrefHashCalculator::ValidateEncrypted(
    const std::string& path,
    const base::Value* value,
    const std::string& stored_encrypted_hash_base64,
    const os_crypt_async::Encryptor* encryptor) const {
  DCHECK(encryptor);

  std::optional<std::vector<uint8_t>> encrypted_hash =
      base::Base64Decode(stored_encrypted_hash_base64);
  if (!encrypted_hash) {
    return INVALID_ENCRYPTED;
  }

  std::optional<std::string> decrypted_hash =
      encryptor->DecryptData(*encrypted_hash);
  if (!decrypted_hash) {
    return INVALID_ENCRYPTED;
  }

  std::string expected_hash = Hash(path, ValueAsString(value));
  return crypto::SecureMemEqual(base::as_byte_span(*decrypted_hash),
                                base::as_byte_span(expected_hash))
             ? VALID_ENCRYPTED
             : INVALID_ENCRYPTED;
}

std::string PrefHashCalculator::HmacSign(std::string_view path,
                                         std::string_view value) const {
  crypto::hmac::HmacSigner signer(crypto::hash::kSha256,
                                  base::as_byte_span(seed_));
  signer.Update(base::as_byte_span(device_id_));
  signer.Update(base::as_byte_span(path));
  signer.Update(base::as_byte_span(value));
  std::array<uint8_t, crypto::hash::kSha256Size> result;
  signer.Finish(result);
  return base::HexEncode(result);
}

[[nodiscard]] bool PrefHashCalculator::HmacVerify(
    std::string_view path,
    std::string_view value,
    std::string_view sig_hex) const {
  std::array<uint8_t, crypto::hash::kSha256Size> sig;
  if (!base::HexStringToSpan(sig_hex, sig)) {
    return false;
  }
  crypto::hmac::HmacVerifier verifier(crypto::hash::kSha256,
                                      base::as_byte_span(seed_));
  verifier.Update(base::as_byte_span(device_id_));
  verifier.Update(base::as_byte_span(path));
  verifier.Update(base::as_byte_span(value));
  return verifier.Finish(sig);
}

std::string PrefHashCalculator::Hash(std::string_view path,
                                     std::string_view value) const {
  crypto::hash::Hasher hasher(crypto::hash::kSha256);
  hasher.Update(base::as_byte_span(seed_));
  hasher.Update(base::as_byte_span(path));
  hasher.Update(base::as_byte_span(value));

  std::array<uint8_t, crypto::hash::kSha256Size> result;
  hasher.Finish(result);
  return std::string(base::as_string_view(result));
}
