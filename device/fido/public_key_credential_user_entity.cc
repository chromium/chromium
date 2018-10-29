// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/public_key_credential_user_entity.h"

#include <utility>

#include "device/fido/fido_constants.h"

namespace device {

// static
base::Optional<PublicKeyCredentialUserEntity>
PublicKeyCredentialUserEntity::CreateFromCBORValue(const cbor::Value& cbor) {
  if (!cbor.is_map())
    return base::nullopt;

  const cbor::Value::MapValue& cbor_map = cbor.GetMap();

  auto user_id = cbor_map.find(cbor::Value(kEntityIdMapKey));
  if (user_id == cbor_map.end() || !user_id->second.is_bytestring())
    return base::nullopt;

  PublicKeyCredentialUserEntity user(user_id->second.GetBytestring());

  auto user_name = cbor_map.find(cbor::Value(kEntityNameMapKey));
  if (user_name != cbor_map.end() && user_name->second.is_string()) {
    user.SetUserName(user_name->second.GetString());
  }

  auto user_display_name = cbor_map.find(cbor::Value(kDisplayNameMapKey));
  if (user_display_name != cbor_map.end() &&
      user_display_name->second.is_string()) {
    user.SetDisplayName(user_display_name->second.GetString());
  }

  auto user_icon_url = cbor_map.find(cbor::Value(kIconUrlMapKey));
  if (user_icon_url != cbor_map.end() && user_icon_url->second.is_string()) {
    user.SetIconUrl(GURL(user_icon_url->second.GetString()));
  }

  return user;
}

PublicKeyCredentialUserEntity::PublicKeyCredentialUserEntity(
    std::vector<uint8_t> user_id)
    : user_id_(std::move(user_id)) {}

PublicKeyCredentialUserEntity::PublicKeyCredentialUserEntity(
    const PublicKeyCredentialUserEntity& other) = default;

PublicKeyCredentialUserEntity::PublicKeyCredentialUserEntity(
    PublicKeyCredentialUserEntity&& other) = default;

PublicKeyCredentialUserEntity& PublicKeyCredentialUserEntity::operator=(
    const PublicKeyCredentialUserEntity& other) = default;

PublicKeyCredentialUserEntity& PublicKeyCredentialUserEntity::operator=(
    PublicKeyCredentialUserEntity&& other) = default;

PublicKeyCredentialUserEntity::~PublicKeyCredentialUserEntity() = default;

cbor::Value PublicKeyCredentialUserEntity::ConvertToCBOR() const {
  cbor::Value::MapValue user_map;
  user_map.emplace(kEntityIdMapKey, user_id_);
  if (user_name_)
    user_map.emplace(kEntityNameMapKey, *user_name_);
  if (user_icon_url_)
    user_map.emplace(kIconUrlMapKey, user_icon_url_->spec());
  if (user_display_name_) {
    user_map.emplace(kDisplayNameMapKey, *user_display_name_);
  }
  return cbor::Value(std::move(user_map));
}

PublicKeyCredentialUserEntity& PublicKeyCredentialUserEntity::SetUserName(
    std::string user_name) {
  user_name_ = std::move(user_name);
  return *this;
}

PublicKeyCredentialUserEntity& PublicKeyCredentialUserEntity::SetDisplayName(
    std::string user_display_name) {
  user_display_name_ = std::move(user_display_name);
  return *this;
}

PublicKeyCredentialUserEntity& PublicKeyCredentialUserEntity::SetIconUrl(
    GURL icon_url) {
  user_icon_url_ = std::move(icon_url);
  return *this;
}

}  // namespace device
