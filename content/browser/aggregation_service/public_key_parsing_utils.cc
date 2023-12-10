// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/public_key_parsing_utils.h"

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/containers/flat_set.h"
#include "base/values.h"
#include "content/browser/aggregation_service/public_key.h"

namespace content {

namespace {

// Constructs a public key from a single JSON key definition. Returns
// `std::nullopt`in case of an error or invalid JSON.
std::optional<PublicKey> GetPublicKey(base::Value& value) {
  if (!value.is_dict())
    return std::nullopt;

  base::Value::Dict& dict = value.GetDict();
  std::string* key_id = dict.FindString("id");
  if (!key_id) {
    return std::nullopt;
  }

  if (key_id->size() > PublicKey::kMaxIdSize) {
    return std::nullopt;
  }

  std::string* key = dict.FindString("key");
  if (!key) {
    return std::nullopt;
  }

  std::string key_string;
  if (!base::Base64Decode(*key, &key_string)) {
    return std::nullopt;
  }

  if (key_string.size() != PublicKey::kKeyByteLength)
    return std::nullopt;

  return PublicKey(std::move(*key_id),
                   std::vector<uint8_t>(key_string.begin(), key_string.end()));
}

}  // namespace

namespace aggregation_service {

std::vector<PublicKey> GetPublicKeys(base::Value& value) {
  if (!value.is_dict())
    return {};

  base::Value::List* keys_json = value.GetDict().FindList("keys");
  if (!keys_json) {
    return {};
  }

  std::vector<PublicKey> public_keys;
  base::flat_set<std::string> key_ids_set;

  for (auto& key_json : *keys_json) {
    // Return error (i.e. empty vector) if more keys than expected are
    // specified.
    if (public_keys.size() == PublicKeyset::kMaxNumberKeys)
      return {};

    std::optional<PublicKey> key = GetPublicKey(key_json);

    // Return error (i.e. empty vector) if any of the keys are invalid.
    if (!key.has_value())
      return {};

    // Return error (i.e. empty vector) if there's duplicate key id.
    if (!key_ids_set.insert(key.value().id).second)
      return {};

    public_keys.push_back(std::move(key.value()));
  }

  return public_keys;
}

}  // namespace aggregation_service

}  // namespace content
