// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/connection_endpoint_metadata.h"

#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

namespace {
const char kSupportedProtocolAlpnsKey[] = "supported_protocol_alpns";
const char kEchConfigListKey[] = "ech_config_list";
}  // namespace

ConnectionEndpointMetadata::ConnectionEndpointMetadata() = default;
ConnectionEndpointMetadata::~ConnectionEndpointMetadata() = default;
ConnectionEndpointMetadata::ConnectionEndpointMetadata(
    const ConnectionEndpointMetadata&) = default;
ConnectionEndpointMetadata::ConnectionEndpointMetadata(
    ConnectionEndpointMetadata&&) = default;

base::Value ConnectionEndpointMetadata::ToValue() const {
  base::Value::DictStorage dict;

  base::Value::ListStorage alpns_list;
  for (const std::string& alpn : supported_protocol_alpns) {
    alpns_list.emplace_back(alpn);
  }
  dict.emplace(kSupportedProtocolAlpnsKey, std::move(alpns_list));

  dict.emplace(kEchConfigListKey, base::Base64Encode(ech_config_list));

  return base::Value(std::move(dict));
}

// static
absl::optional<ConnectionEndpointMetadata>
ConnectionEndpointMetadata::FromValue(const base::Value& value) {
  if (!value.is_dict())
    return absl::nullopt;

  const base::Value* alpns_value =
      value.FindListKey(kSupportedProtocolAlpnsKey);
  const std::string* ech_config_list_value =
      value.FindStringKey(kEchConfigListKey);

  if (!alpns_value || !ech_config_list_value)
    return absl::nullopt;

  ConnectionEndpointMetadata metadata;

  std::vector<std::string> alpns;
  for (const base::Value& value : alpns_value->GetListDeprecated()) {
    if (!value.is_string())
      return absl::nullopt;
    metadata.supported_protocol_alpns.push_back(value.GetString());
  }

  absl::optional<std::vector<uint8_t>> decoded =
      base::Base64Decode(*ech_config_list_value);
  if (!decoded)
    return absl::nullopt;
  metadata.ech_config_list = std::move(*decoded);

  return metadata;
}

}  // namespace net
