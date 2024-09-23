// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/cdm/json_web_key.h"

#include <stddef.h>

#include <memory>
#include <string_view>
#include <utility>

#include "base/base64url.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "media/base/content_decryption_module.h"

namespace media {

const char kKeysTag[] = "keys";
const char kKeyTypeTag[] = "kty";
const char kKeyTypeOct[] = "oct";  // Octet sequence.
const char kKeyTag[] = "k";
const char kKeyIdTag[] = "kid";
const char kKeyIdsTag[] = "kids";
const char kTypeTag[] = "type";
const char kTemporarySession[] = "temporary";
const char kPersistentLicenseSession[] = "persistent-license";

static std::string ShortenTo64Characters(const std::string& input) {
  // Convert |input| into a string with escaped characters replacing any
  // non-ASCII characters. Limiting |input| to the first 65 characters so
  // we don't waste time converting a potentially long string and then
  // throwing away the excess.
  std::string escaped_str =
      base::EscapeBytesAsInvalidJSONString(input.substr(0, 65), false);
  if (escaped_str.length() <= 64u)
    return escaped_str;

  // This may end up truncating an escaped character, but the first part of
  // the string should provide enough information.
  return escaped_str.substr(0, 61).append("...");
}

static base::Value::Dict CreateJSONDictionary(const uint8_t* key,
                                              int key_length,
                                              const uint8_t* key_id,
                                              int key_id_length) {
  std::string key_string, key_id_string;
  base::Base64UrlEncode(
      base::span<const uint8_t>(key, static_cast<size_t>(key_length)),
      base::Base64UrlEncodePolicy::OMIT_PADDING, &key_string);
  base::Base64UrlEncode(
      base::span<const uint8_t>(key_id, static_cast<size_t>(key_id_length)),
      base::Base64UrlEncodePolicy::OMIT_PADDING, &key_id_string);

  base::Value::Dict jwk;
  jwk.Set(kKeyTypeTag, kKeyTypeOct);
  jwk.Set(kKeyTag, key_string);
  jwk.Set(kKeyIdTag, key_id_string);
  return jwk;
}

std::string GenerateJWKSet(const uint8_t* key,
                           int key_length,
                           const uint8_t* key_id,
                           int key_id_length) {
  // Create the JWK, and wrap it into a JWK Set.
  base::Value::List list;
  list.Append(CreateJSONDictionary(key, key_length, key_id, key_id_length));
  base::Value::Dict jwk_set;
  jwk_set.Set(kKeysTag, std::move(list));

  // Finally serialize |jwk_set| into a string and return it.
  std::string serialized_jwk;
  JSONStringValueSerializer serializer(&serialized_jwk);
  serializer.Serialize(base::Value(std::move(jwk_set)));
  return serialized_jwk;
}

std::string GenerateJWKSet(const KeyIdAndKeyPairs& keys,
                           CdmSessionType session_type) {
  base::Value::List list;
  for (const auto& key_pair : keys) {
    list.Append(base::Value(CreateJSONDictionary(
        reinterpret_cast<const uint8_t*>(key_pair.second.data()),
        key_pair.second.length(),
        reinterpret_cast<const uint8_t*>(key_pair.first.data()),
        key_pair.first.length())));
  }

  base::Value::Dict jwk_set;
  jwk_set.Set(kKeysTag, std::move(list));
  switch (session_type) {
    case CdmSessionType::kTemporary:
      jwk_set.Set(kTypeTag, kTemporarySession);
      break;
    case CdmSessionType::kPersistentLicense:
      jwk_set.Set(kTypeTag, kPersistentLicenseSession);
      break;
  }

  // Finally serialize |jwk_set| into a string and return it.
  std::string serialized_jwk;
  JSONStringValueSerializer serializer(&serialized_jwk);
  serializer.Serialize(base::Value(std::move(jwk_set)));
  return serialized_jwk;
}

// Processes a JSON Web Key to extract the key id and key value. Sets |jwk_key|
// to the id/value pair and returns true on success.
static bool ConvertJwkToKeyPair(const base::Value::Dict& jwk,
                                KeyIdAndKeyPair* jwk_key) {
  const base::Value* type = jwk.Find(kKeyTypeTag);
  if (!type || *type != kKeyTypeOct) {
    DVLOG(1) << "Missing or invalid '" << kKeyTypeTag
             << "': " << (type ? type->DebugString() : "");
    return false;
  }

  // Get the key id and actual key parameters.
  const base::Value* encoded_key_id = jwk.Find(kKeyIdTag);
  const base::Value* encoded_key = jwk.Find(kKeyTag);
  if (!encoded_key_id) {
    DVLOG(1) << "Missing '" << kKeyIdTag << "' parameter";
    return false;
  }
  if (!encoded_key) {
    DVLOG(1) << "Missing '" << kKeyTag << "' parameter";
    return false;
  }

  // Key ID and key are base64url-encoded strings, so decode them.
  std::string raw_key_id;
  if (!base::Base64UrlDecode(encoded_key_id->GetString(),
                             base::Base64UrlDecodePolicy::DISALLOW_PADDING,
                             &raw_key_id) ||
      raw_key_id.empty()) {
    DVLOG(1) << "Invalid '" << kKeyIdTag << "' value: " << encoded_key_id;
    return false;
  }

  std::string raw_key;
  if (!base::Base64UrlDecode(encoded_key->GetString(),
                             base::Base64UrlDecodePolicy::DISALLOW_PADDING,
                             &raw_key) ||
      raw_key.empty()) {
    DVLOG(1) << "Invalid '" << kKeyTag << "' value: " << encoded_key;
    return false;
  }

  // Add the decoded key ID and the decoded key to the list.
  *jwk_key = std::make_pair(raw_key_id, raw_key);
  return true;
}

bool ExtractKeysFromJWKSet(const std::string& jwk_set,
                           KeyIdAndKeyPairs* keys,
                           CdmSessionType* session_type) {
  if (!base::IsStringASCII(jwk_set)) {
    DVLOG(1) << "Non ASCII JWK Set: " << jwk_set;
    return false;
  }

  std::optional<base::Value> root = base::JSONReader::Read(jwk_set);
  if (!root || root->type() != base::Value::Type::DICT) {
    DVLOG(1) << "Not valid JSON: " << jwk_set;
    return false;
  }

  // Locate the set from the dictionary.
  base::Value::Dict* dictionary = root.value().GetIfDict();
  base::Value::List* list_val = dictionary->FindList(kKeysTag);
  if (!list_val) {
    DVLOG(1) << "Missing '" << kKeysTag
             << "' parameter or not a list in JWK Set";
    return false;
  }

  // Create a local list of keys, so that |jwk_keys| only gets updated on
  // success.
  KeyIdAndKeyPairs local_keys;
  for (size_t i = 0; i < list_val->size(); ++i) {
    base::Value& jwk = (*list_val)[i];
    if (!jwk.is_dict()) {
      DVLOG(1) << "Unable to access '" << kKeysTag << "'[" << i
               << "] in JWK Set";
      return false;
    }
    KeyIdAndKeyPair key_pair;
    if (!ConvertJwkToKeyPair(jwk.GetDict(), &key_pair)) {
      DVLOG(1) << "Error from '" << kKeysTag << "'[" << i << "]";
      return false;
    }
    local_keys.push_back(key_pair);
  }

  // Successfully processed all JWKs in the set. Now check if "type" is
  // specified.
  base::Value* value = dictionary->Find(kTypeTag);
  if (!value) {
    // Not specified, so use the default type.
    *session_type = CdmSessionType::kTemporary;
  } else {
    if (!value->is_string()) {
      DVLOG(1) << "Invalid '" << kTypeTag << "' value";
      return false;
    }
    const std::string session_type_id = value->GetString();
    if (session_type_id == kTemporarySession) {
      *session_type = CdmSessionType::kTemporary;
    } else if (session_type_id == kPersistentLicenseSession) {
      *session_type = CdmSessionType::kPersistentLicense;
    } else {
      DVLOG(1) << "Invalid '" << kTypeTag << "' value: " << session_type_id;
      return false;
    }
  }

  // All done.
  keys->swap(local_keys);
  return true;
}

bool ExtractKeyIdsFromKeyIdsInitData(const std::string& input,
                                     KeyIdList* key_ids,
                                     std::string* error_message) {
  if (!base::IsStringASCII(input)) {
    error_message->assign("Non ASCII: ");
    error_message->append(ShortenTo64Characters(input));
    return false;
  }

  std::optional<base::Value> root = base::JSONReader::Read(input);
  if (!root || root->type() != base::Value::Type::DICT) {
    error_message->assign("Not valid JSON: ");
    error_message->append(ShortenTo64Characters(input));
    return false;
  }

  // Locate the set from the dictionary.
  const base::Value::List* list_val = root->GetDict().FindList(kKeyIdsTag);
  if (!list_val) {
    error_message->assign("Missing '");
    error_message->append(kKeyIdsTag);
    error_message->append("' parameter or not a list");
    return false;
  }

  // Create a local list of key ids, so that |key_ids| only gets updated on
  // success.
  KeyIdList local_key_ids;
  for (size_t i = 0; i < list_val->size(); ++i) {
    const std::string* encoded_key_id = (*list_val)[i].GetIfString();
    if (!encoded_key_id) {
      error_message->assign("'");
      error_message->append(kKeyIdsTag);
      error_message->append("'[");
      error_message->append(base::NumberToString(i));
      error_message->append("] is not string.");
      return false;
    }

    // Key ID is a base64url-encoded string, so decode it.
    std::string raw_key_id;
    if (!base::Base64UrlDecode(*encoded_key_id,
                               base::Base64UrlDecodePolicy::DISALLOW_PADDING,
                               &raw_key_id) ||
        raw_key_id.empty()) {
      error_message->assign("'");
      error_message->append(kKeyIdsTag);
      error_message->append("'[");
      error_message->append(base::NumberToString(i));
      error_message->append("] is not valid base64url encoded. Value: ");
      error_message->append(ShortenTo64Characters(*encoded_key_id));
      return false;
    }

    // Add the decoded key ID to the list.
    local_key_ids.push_back(std::vector<uint8_t>(
        raw_key_id.data(), raw_key_id.data() + raw_key_id.length()));
  }

  // All done.
  key_ids->swap(local_key_ids);
  error_message->clear();
  return true;
}

void CreateLicenseRequest(const KeyIdList& key_ids,
                          CdmSessionType session_type,
                          std::vector<uint8_t>* license) {
  // Create the license request.
  base::Value::Dict request;
  base::Value::List list;
  for (const auto& key_id : key_ids) {
    std::string key_id_string;
    base::Base64UrlEncode(
        std::string_view(reinterpret_cast<const char*>(key_id.data()),
                         key_id.size()),
        base::Base64UrlEncodePolicy::OMIT_PADDING, &key_id_string);

    list.Append(key_id_string);
  }
  request.Set(kKeyIdsTag, std::move(list));

  switch (session_type) {
    case CdmSessionType::kTemporary:
      request.Set(kTypeTag, kTemporarySession);
      break;
    case CdmSessionType::kPersistentLicense:
      request.Set(kTypeTag, kPersistentLicenseSession);
      break;
  }

  // Serialize the license request as a string.
  std::string json;
  JSONStringValueSerializer serializer(&json);
  serializer.Serialize(request);

  // Convert the serialized license request into std::vector and return it.
  std::vector<uint8_t> result(json.begin(), json.end());
  license->swap(result);
}

base::Value::Dict MakeKeyIdsDictionary(const KeyIdList& key_ids) {
  base::Value::Dict dictionary;
  base::Value::List list;
  for (const auto& key_id : key_ids) {
    std::string key_id_string;
    base::Base64UrlEncode(
        std::string_view(reinterpret_cast<const char*>(key_id.data()),
                         key_id.size()),
        base::Base64UrlEncodePolicy::OMIT_PADDING, &key_id_string);

    list.Append(key_id_string);
  }
  dictionary.Set(kKeyIdsTag, std::move(list));
  return dictionary;
}

std::vector<uint8_t> SerializeDictionaryToVector(
    const base::Value::Dict& dictionary) {
  // Serialize the dictionary as a string.
  std::string json;
  JSONStringValueSerializer serializer(&json);
  serializer.Serialize(dictionary);

  // Convert the serialized data into std::vector and return it.
  return std::vector<uint8_t>(json.begin(), json.end());
}

void CreateKeyIdsInitData(const KeyIdList& key_ids,
                          std::vector<uint8_t>* init_data) {
  // Create the init_data.
  auto dictionary = MakeKeyIdsDictionary(key_ids);

  auto data = SerializeDictionaryToVector(dictionary);
  init_data->swap(data);
}

// The format is a JSON object. For sessions of type "persistent-license", the
// object shall contain the following member:
//    "kids"
//      An array of key IDs. Each element of the array is the base64url encoding
//      of the octet sequence containing the key ID value.
std::vector<uint8_t> CreateLicenseReleaseMessage(const KeyIdList& key_ids) {
  // Create the init_data.
  auto dictionary = MakeKeyIdsDictionary(key_ids);
  return SerializeDictionaryToVector(dictionary);
}

bool ExtractFirstKeyIdFromLicenseRequest(const std::vector<uint8_t>& license,
                                         std::vector<uint8_t>* first_key) {
  const std::string license_as_str(
      reinterpret_cast<const char*>(!license.empty() ? &license[0] : NULL),
      license.size());
  if (!base::IsStringASCII(license_as_str)) {
    DVLOG(1) << "Non ASCII license: " << license_as_str;
    return false;
  }

  std::optional<base::Value> root = base::JSONReader::Read(license_as_str);
  if (!root || root->type() != base::Value::Type::DICT) {
    DVLOG(1) << "Not valid JSON: " << license_as_str;
    return false;
  }

  // Locate the set from the dictionary.
  const base::Value::List* list_val = root->GetDict().FindList(kKeyIdsTag);
  if (!list_val) {
    DVLOG(1) << "Missing '" << kKeyIdsTag << "' parameter or not a list";
    return false;
  }

  // Get the first key.
  if (list_val->size() < 1) {
    DVLOG(1) << "Empty '" << kKeyIdsTag << "' list";
    return false;
  }

  const std::string* encoded_key = (*list_val)[0].GetIfString();
  if (!encoded_key) {
    DVLOG(1) << "First entry in '" << kKeyIdsTag << "' not a string";
    return false;
  }

  std::string decoded_string;
  if (!base::Base64UrlDecode(*encoded_key,
                             base::Base64UrlDecodePolicy::DISALLOW_PADDING,
                             &decoded_string) ||
      decoded_string.empty()) {
    DVLOG(1) << "Invalid '" << kKeyIdsTag << "' value: " << *encoded_key;
    return false;
  }

  std::vector<uint8_t> result(decoded_string.begin(), decoded_string.end());
  first_key->swap(result);
  return true;
}

}  // namespace media
