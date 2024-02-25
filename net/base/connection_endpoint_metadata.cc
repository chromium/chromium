// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/connection_endpoint_metadata.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/values.h"

namespace net {

namespace {
const char kSupportedProtocolAlpnsKey[] = "supported_protocol_alpns";
const char kEchConfigListKey[] = "ech_config_list";
const char kTargetNameKey[] = "target_name";
}  // namespace

ConnectionEndpointMetadata::ConnectionEndpointMetadata() = default;

ConnectionEndpointMetadata::ConnectionEndpointMetadata(
    std::vector<std::string> supported_protocol_alpns,
    EchConfigList ech_config_list,
    std::string target_name)
    : supported_protocol_alpns(std::move(supported_protocol_alpns)),
      ech_config_list(std::move(ech_config_list)),
      target_name(std::move(target_name)) {}

ConnectionEndpointMetadata::~ConnectionEndpointMetadata() = default;
ConnectionEndpointMetadata::ConnectionEndpointMetadata(
    const ConnectionEndpointMetadata&) = default;
ConnectionEndpointMetadata::ConnectionEndpointMetadata(
    ConnectionEndpointMetadata&&) = default;

base::Value ConnectionEndpointMetadata::ToValue() const {
  base::Value::Dict dict;

  base::Value::List alpns_list;
  for (const std::string& alpn : supported_protocol_alpns) {
    alpns_list.Append(alpn);
  }
  dict.Set(kSupportedProtocolAlpnsKey, std::move(alpns_list));

  dict.Set(kEchConfigListKey, base::Base64Encode(ech_config_list));

  if (!target_name.empty()) {
    dict.Set(kTargetNameKey, target_name);
  }

  return base::Value(std::move(dict));
}

// static
std::optional<ConnectionEndpointMetadata> ConnectionEndpointMetadata::FromValue(
    const base::Value& value) {
  const base::Value::Dict* dict = value.GetIfDict();
  if (!dict)
    return std::nullopt;

  const base::Value::List* alpns_list =
      dict->FindList(kSupportedProtocolAlpnsKey);
  const std::string* ech_config_list_value =
      dict->FindString(kEchConfigListKey);
  const std::string* target_name_value = dict->FindString(kTargetNameKey);

  if (!alpns_list || !ech_config_list_value)
    return std::nullopt;

  ConnectionEndpointMetadata metadata;

  std::vector<std::string> alpns;
  for (const base::Value& alpn : *alpns_list) {
    if (!alpn.is_string())
      return std::nullopt;
    metadata.supported_protocol_alpns.push_back(alpn.GetString());
  }

  std::optional<std::vector<uint8_t>> decoded =
      base::Base64Decode(*ech_config_list_value);
  if (!decoded)
    return std::nullopt;
  metadata.ech_config_list = std::move(*decoded);

  if (target_name_value) {
    metadata.target_name = *target_name_value;
  }

  return metadata;
}

}  // namespace net
