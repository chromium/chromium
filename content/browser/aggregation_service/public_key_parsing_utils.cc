// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/public_key_parsing_utils.h"

#include <stdint.h>

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/browser/aggregation_service/public_key.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

const char kKeyId[] = "id";
const char kKeyKey[] = "key";
const char kKeyNotBefore[] = "not_before";
const char kKeyNotAfter[] = "not_after";

// Returns the latest version id from multiple JSON arrays tagged with version
// ids, currently based on the lexicographical comparison of version ids. In
// case of an error or invalid JSON, returns an empty string.
std::string GetLatestVersion(base::Value& value) {
  if (!value.is_dict())
    return "";

  // TODO(crbug.com/1218920): Implement the logic to get latest supported
  // version.

  std::string latest_version;

  for (const auto kv : value.DictItems()) {
    const std::string& version = kv.first;
    if (version < latest_version)
      continue;

    if (!kv.second.is_list())
      continue;

    latest_version = version;
  }

  return latest_version;
}

// This attempts to extract the 64-bit integer value associated with `key`. In
// case of failure, e.g. the key does not exist or doesn't have 64-bit integer
// type, nullopt is returned.
absl::optional<int64_t> ExtractKeyInt64(base::Value& value,
                                        const std::string& key) {
  absl::optional<base::Value> str = value.ExtractKey(key);
  if (!str || !str.value().is_string())
    return absl::nullopt;

  int64_t number = 0;
  if (!base::StringToInt64(str.value().GetString(), &number))
    return absl::nullopt;

  return absl::make_optional(number);
}

// Constructs a public key from a single JSON key definition. Returns
// `absl::nullopt`in case of an error or invalid JSON.
absl::optional<PublicKey> GetPublicKey(base::Value& value) {
  if (!value.is_dict())
    return absl::nullopt;

  absl::optional<base::Value> id = value.ExtractKey(kKeyId);
  if (!id || !id.value().is_string())
    return absl::nullopt;

  absl::optional<base::Value> key = value.ExtractKey(kKeyKey);
  if (!key || !key.value().is_string())
    return absl::nullopt;

  absl::optional<int64_t> not_before = ExtractKeyInt64(value, kKeyNotBefore);
  if (!not_before)
    return absl::nullopt;

  absl::optional<int64_t> not_after = ExtractKeyInt64(value, kKeyNotAfter);
  if (!not_after)
    return absl::nullopt;

  std::string key_string = key->GetString();
  if (!base::Base64Decode(key_string, &key_string))
    return absl::nullopt;

  return PublicKey(id.value().GetString(),
                   std::vector<uint8_t>(key_string.begin(), key_string.end()),
                   base::Time::UnixEpoch() +
                       base::TimeDelta::FromMilliseconds(not_before.value()),
                   base::Time::UnixEpoch() +
                       base::TimeDelta::FromMilliseconds(not_after.value()));
}

}  // namespace

namespace aggregation_service {

std::vector<PublicKey> GetPublicKeys(base::Value& value) {
  if (!value.is_dict())
    return {};

  std::string latest_version = GetLatestVersion(value);
  if (latest_version.empty())
    return {};

  absl::optional<base::Value> latest_json_object =
      value.ExtractKey(latest_version);

  if (!latest_json_object.value().is_list())
    return {};

  std::vector<PublicKey> keys_vec;
  for (auto& key_json : latest_json_object.value().GetList()) {
    absl::optional<PublicKey> key = GetPublicKey(key_json);
    if (key)
      keys_vec.push_back(key.value());
  }

  return keys_vec;
}

}  // namespace aggregation_service

}  // namespace content
