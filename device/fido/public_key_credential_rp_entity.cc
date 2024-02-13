// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/public_key_credential_rp_entity.h"

#include <algorithm>
#include <utility>

#include "device/fido/fido_constants.h"

namespace device {

// static
std::optional<PublicKeyCredentialRpEntity>
PublicKeyCredentialRpEntity::CreateFromCBORValue(const cbor::Value& cbor) {
  if (!cbor.is_map()) {
    return std::nullopt;
  }
  const cbor::Value::MapValue& rp_map = cbor.GetMap();
  for (const auto& element : rp_map) {
    if (!element.first.is_string() || !element.second.is_string()) {
      return std::nullopt;
    }
    const std::string& key = element.first.GetString();
    if (key != kEntityIdMapKey && key != kEntityNameMapKey) {
      return std::nullopt;
    }
  }
  const auto id_it = rp_map.find(cbor::Value(kEntityIdMapKey));
  if (id_it == rp_map.end()) {
    return std::nullopt;
  }
  PublicKeyCredentialRpEntity rp(id_it->second.GetString());
  const auto name_it = rp_map.find(cbor::Value(kEntityNameMapKey));
  if (name_it != rp_map.end()) {
    rp.name = name_it->second.GetString();
  }
  return rp;
}

PublicKeyCredentialRpEntity::PublicKeyCredentialRpEntity() = default;

PublicKeyCredentialRpEntity::PublicKeyCredentialRpEntity(std::string id_)
    : id(std::move(id_)) {}

PublicKeyCredentialRpEntity::PublicKeyCredentialRpEntity(
    std::string id_,
    std::optional<std::string> name_)
    : id(std::move(id_)), name(std::move(name_)) {}

PublicKeyCredentialRpEntity::PublicKeyCredentialRpEntity(
    const PublicKeyCredentialRpEntity& other) = default;

PublicKeyCredentialRpEntity::PublicKeyCredentialRpEntity(
    PublicKeyCredentialRpEntity&& other) = default;

PublicKeyCredentialRpEntity& PublicKeyCredentialRpEntity::operator=(
    const PublicKeyCredentialRpEntity& other) = default;

PublicKeyCredentialRpEntity& PublicKeyCredentialRpEntity::operator=(
    PublicKeyCredentialRpEntity&& other) = default;

PublicKeyCredentialRpEntity::~PublicKeyCredentialRpEntity() = default;

bool PublicKeyCredentialRpEntity::operator==(
    const PublicKeyCredentialRpEntity& other) const {
  return id == other.id && name == other.name;
}

cbor::Value AsCBOR(const PublicKeyCredentialRpEntity& entity) {
  cbor::Value::MapValue rp_map;
  rp_map.emplace(kEntityIdMapKey, entity.id);
  if (entity.name)
    rp_map.emplace(kEntityNameMapKey, *entity.name);

  return cbor::Value(std::move(rp_map));
}

}  // namespace device
