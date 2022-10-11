// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/public_key_parsing_utils.h"

#include <stdint.h>

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/containers/flat_set.h"
#include "base/values.h"
#include "content/browser/aggregation_service/public_key.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

// Constructs a public key from a single JSON key definition. Returns
// `absl::nullopt`in case of an error or invalid JSON.
absl::optional<PublicKey> GetPublicKey(base::Value& value) {
  if (!value.is_dict())
    return absl::nullopt;

  absl::optional<base::Value> id = value.ExtractKey("id");
  if (!id || !id.value().is_string())
    return absl::nullopt;

  std::string key_id = id->GetString();
  if (key_id.size() > PublicKey::kMaxIdSize)
    return absl::nullopt;

  absl::optional<base::Value> key = value.ExtractKey("key");
  if (!key || !key.value().is_string())
    return absl::nullopt;

  std::string key_string = key->GetString();
  if (!base::Base64Decode(key_string, &key_string))
    return absl::nullopt;

  if (key_string.size() != PublicKey::kKeyByteLength)
    return absl::nullopt;

  return PublicKey(std::move(key_id),
                   std::vector<uint8_t>(key_string.begin(), key_string.end()));
}

}  // namespace

namespace aggregation_service {

std::vector<PublicKey> GetPublicKeys(base::Value& value) {
  if (!value.is_dict())
    return {};

  absl::optional<base::Value> keys_json = value.ExtractKey("keys");
  if (!keys_json.has_value() || !keys_json.value().is_list())
    return {};

  std::vector<PublicKey> public_keys;
  base::flat_set<std::string> key_ids_set;

  for (auto& key_json : keys_json.value().GetList()) {
    // Return error (i.e. empty vector) if more keys than expected are
    // specified.
    if (public_keys.size() == PublicKeyset::kMaxNumberKeys)
      return {};

    absl::optional<PublicKey> key = GetPublicKey(key_json);

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
