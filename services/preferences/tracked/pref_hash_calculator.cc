// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/tracked/pref_hash_calculator.h"

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "crypto/hmac.h"

namespace {

// Calculates an HMAC of |message| using |key|, encoded as a hexadecimal string.
std::string GetDigestString(const std::string& key,
                            const std::string& message) {
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  std::vector<uint8_t> digest(hmac.DigestLength());
  if (!hmac.Init(key) || !hmac.Sign(message, &digest[0], digest.size())) {
    NOTREACHED_IN_MIGRATION();
    return std::string();
  }
  return base::HexEncode(digest);
}

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

// Verifies that |digest_string| is a valid HMAC of |message| using |key|.
// |digest_string| must be encoded as a hexadecimal string.
bool VerifyDigestString(const std::string& key,
                        const std::string& message,
                        const std::string& digest_string) {
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  std::string digest;
  return base::HexStringToString(digest_string, &digest) && hmac.Init(key) &&
         hmac.Verify(message, digest);
}

// Renders |value| as a string. |value| may be NULL, in which case the result
// is an empty string. This method can be expensive and its result should be
// re-used rather than recomputed where possible.

std::string ValueAsString(const base::Value::Dict* value) {
  if (!value)
    return std::string();

  base::Value::Dict dict = value->Clone();
  RemoveEmptyValueDictEntries(dict);

  std::string value_as_string;
  JSONStringValueSerializer serializer(&value_as_string);
  serializer.Serialize(dict);

  return value_as_string;
}

std::string ValueAsString(const base::Value* value) {
  if (!value)
    return std::string();

  if (value->is_dict())
    return ValueAsString(&value->GetDict());

  std::string value_as_string;
  JSONStringValueSerializer serializer(&value_as_string);
  serializer.Serialize(*value);

  return value_as_string;
}

// Concatenates |device_id|, |path|, and |value_as_string| to give the hash
// input.
std::string GetMessage(const std::string& device_id,
                       const std::string& path,
                       const std::string& value_as_string) {
  std::string message;
  message.reserve(device_id.size() + path.size() + value_as_string.size());
  message.append(device_id);
  message.append(path);
  message.append(value_as_string);
  return message;
}

}  // namespace

PrefHashCalculator::PrefHashCalculator(const std::string& seed,
                                       const std::string& device_id,
                                       const std::string& legacy_device_id)
    : seed_(seed), device_id_(device_id), legacy_device_id_(legacy_device_id) {}

PrefHashCalculator::~PrefHashCalculator() {}

std::string PrefHashCalculator::Calculate(const std::string& path,
                                          const base::Value* value) const {
  return GetDigestString(seed_,
                         GetMessage(device_id_, path, ValueAsString(value)));
}

std::string PrefHashCalculator::Calculate(const std::string& path,
                                          const base::Value::Dict* dict) const {
  return GetDigestString(seed_,
                         GetMessage(device_id_, path, ValueAsString(dict)));
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
  if (VerifyDigestString(seed_, GetMessage(device_id_, path, value_as_string),
                         digest_string)) {
    return VALID;
  }
  if (!legacy_device_id_.empty() &&
      VerifyDigestString(seed_,
                         GetMessage(legacy_device_id_, path, value_as_string),
                         digest_string)) {
    return VALID_SECURE_LEGACY;
  }
  return INVALID;
}
