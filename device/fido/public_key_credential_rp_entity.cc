// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/public_key_credential_rp_entity.h"

#include <algorithm>
#include <utility>

#include "device/fido/fido_constants.h"

namespace device {

// static
base::Optional<PublicKeyCredentialRpEntity>
PublicKeyCredentialRpEntity::CreateFromCBORValue(const cbor::Value& cbor) {
  if (!cbor.is_map() || cbor.GetMap().size() > 3) {
    return base::nullopt;
  }
  const cbor::Value::MapValue& rp_map = cbor.GetMap();
  for (const auto& element : rp_map) {
    if (!element.first.is_string() || !element.second.is_string()) {
      return base::nullopt;
    }
    const std::string& key = element.first.GetString();
    if (key != kEntityIdMapKey && key != kEntityNameMapKey &&
        key != kIconUrlMapKey) {
      return base::nullopt;
    }
  }
  const auto id_it = rp_map.find(cbor::Value(kEntityIdMapKey));
  if (id_it == rp_map.end()) {
    return base::nullopt;
  }
  PublicKeyCredentialRpEntity rp(id_it->second.GetString());
  const auto name_it = rp_map.find(cbor::Value(kEntityNameMapKey));
  if (name_it != rp_map.end()) {
    rp.name = name_it->second.GetString();
  }
  const auto icon_it = rp_map.find(cbor::Value(kIconUrlMapKey));
  if (icon_it != rp_map.end()) {
    rp.icon_url = GURL(icon_it->second.GetString());
  }
  return rp;
}

PublicKeyCredentialRpEntity::PublicKeyCredentialRpEntity() = default;

PublicKeyCredentialRpEntity::PublicKeyCredentialRpEntity(std::string id_)
    : id(std::move(id_)) {}

PublicKeyCredentialRpEntity::PublicKeyCredentialRpEntity(
    std::string id_,
    base::Optional<std::string> name_,
    base::Optional<GURL> icon_url_)
    : id(std::move(id_)),
      name(std::move(name_)),
      icon_url(std::move(icon_url_)) {}

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
  return id == other.id && name == other.name && icon_url == other.icon_url;
}

cbor::Value AsCBOR(const PublicKeyCredentialRpEntity& entity) {
  cbor::Value::MapValue rp_map;
  rp_map.emplace(kEntityIdMapKey, entity.id);
  if (entity.name)
    rp_map.emplace(kEntityNameMapKey, *entity.name);

  if (entity.icon_url)
    rp_map.emplace(kIconUrlMapKey, entity.icon_url->spec());

  return cbor::Value(std::move(rp_map));
}

}  // namespace device
