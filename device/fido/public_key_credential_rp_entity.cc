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
  if (!cbor.is_map() || cbor.GetMap().size() > 3)
    return base::nullopt;

  const auto& rp_map = cbor.GetMap();
  bool is_rp_map_format_correct =
      std::all_of(rp_map.begin(), rp_map.end(), [](const auto& element) {
        if (!element.first.is_string() || !element.second.is_string())
          return false;

        const auto& key = element.first.GetString();
        return (key == kEntityIdMapKey || key == kEntityNameMapKey ||
                key == kIconUrlMapKey);
      });

  if (!is_rp_map_format_correct)
    return base::nullopt;

  const auto& id_it = rp_map.find(cbor::Value(kEntityIdMapKey));
  const auto& name_it = rp_map.find(cbor::Value(kEntityNameMapKey));
  const auto& icon_it = rp_map.find(cbor::Value(kIconUrlMapKey));
  if (id_it == rp_map.end())
    return base::nullopt;
  PublicKeyCredentialRpEntity rp(id_it->second.GetString());

  if (name_it != rp_map.end())
    rp.SetRpName(name_it->second.GetString());

  if (icon_it != rp_map.end())
    rp.SetRpIconUrl(GURL(icon_it->second.GetString()));

  return rp;
}

PublicKeyCredentialRpEntity::PublicKeyCredentialRpEntity(std::string rp_id)
    : rp_id_(std::move(rp_id)) {}

PublicKeyCredentialRpEntity::PublicKeyCredentialRpEntity(
    const PublicKeyCredentialRpEntity& other) = default;

PublicKeyCredentialRpEntity::PublicKeyCredentialRpEntity(
    PublicKeyCredentialRpEntity&& other) = default;

PublicKeyCredentialRpEntity& PublicKeyCredentialRpEntity::operator=(
    const PublicKeyCredentialRpEntity& other) = default;

PublicKeyCredentialRpEntity& PublicKeyCredentialRpEntity::operator=(
    PublicKeyCredentialRpEntity&& other) = default;

PublicKeyCredentialRpEntity::~PublicKeyCredentialRpEntity() = default;

PublicKeyCredentialRpEntity& PublicKeyCredentialRpEntity::SetRpName(
    std::string rp_name) {
  rp_name_ = std::move(rp_name);
  return *this;
}

PublicKeyCredentialRpEntity& PublicKeyCredentialRpEntity::SetRpIconUrl(
    GURL icon_url) {
  rp_icon_url_ = std::move(icon_url);
  return *this;
}

cbor::Value PublicKeyCredentialRpEntity::ConvertToCBOR() const {
  cbor::Value::MapValue rp_map;
  rp_map.emplace(kEntityIdMapKey, rp_id_);
  if (rp_name_)
    rp_map.emplace(kEntityNameMapKey, *rp_name_);

  if (rp_icon_url_)
    rp_map.emplace(kIconUrlMapKey, rp_icon_url_->spec());

  return cbor::Value(std::move(rp_map));
}

}  // namespace device
