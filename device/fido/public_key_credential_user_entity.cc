// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/public_key_credential_user_entity.h"

#include <utility>

#include "device/fido/features.h"
#include "device/fido/fido_constants.h"

namespace device {

// static
std::optional<PublicKeyCredentialUserEntity>
PublicKeyCredentialUserEntity::CreateFromCBORValue(const cbor::Value& cbor) {
  if (!cbor.is_map()) {
    return std::nullopt;
  }

  const cbor::Value::MapValue& cbor_map = cbor.GetMap();

  auto id_it = cbor_map.find(cbor::Value(kEntityIdMapKey));
  if (id_it == cbor_map.end() || !id_it->second.is_bytestring()) {
    return std::nullopt;
  }

  PublicKeyCredentialUserEntity user(id_it->second.GetBytestring());

  // Note: this code treats `name` and `displayName` fields as optional, but
  // they are required in the spec:
  // https://www.w3.org/TR/webauthn-2/#dictionary-user-credential-params
  auto name_it = cbor_map.find(cbor::Value(kEntityNameMapKey));
  if (name_it != cbor_map.end()) {
    if (!name_it->second.is_string()) {
      return std::nullopt;
    }
    user.name = name_it->second.GetString();
  }

  auto display_name_it = cbor_map.find(cbor::Value(kDisplayNameMapKey));
  if (display_name_it != cbor_map.end()) {
    if (!display_name_it->second.is_string()) {
      return std::nullopt;
    }
    user.display_name = display_name_it->second.GetString();
  }

  return user;
}

PublicKeyCredentialUserEntity::PublicKeyCredentialUserEntity() = default;

PublicKeyCredentialUserEntity::PublicKeyCredentialUserEntity(
    std::vector<uint8_t> id_)
    : id(std::move(id_)) {}

PublicKeyCredentialUserEntity::PublicKeyCredentialUserEntity(
    std::vector<uint8_t> id_,
    std::optional<std::string> name_,
    std::optional<std::string> display_name_)
    : id(std::move(id_)),
      name(std::move(name_)),
      display_name(std::move(display_name_)) {}

PublicKeyCredentialUserEntity::PublicKeyCredentialUserEntity(
    const PublicKeyCredentialUserEntity& other) = default;

PublicKeyCredentialUserEntity::PublicKeyCredentialUserEntity(
    PublicKeyCredentialUserEntity&& other) = default;

PublicKeyCredentialUserEntity& PublicKeyCredentialUserEntity::operator=(
    const PublicKeyCredentialUserEntity& other) = default;

PublicKeyCredentialUserEntity& PublicKeyCredentialUserEntity::operator=(
    PublicKeyCredentialUserEntity&& other) = default;

PublicKeyCredentialUserEntity::~PublicKeyCredentialUserEntity() = default;

bool PublicKeyCredentialUserEntity::operator==(
    const PublicKeyCredentialUserEntity& other) const {
  return id == other.id && name == other.name &&
         display_name == other.display_name;
}

cbor::Value AsCBOR(const PublicKeyCredentialUserEntity& user) {
  cbor::Value::MapValue user_map;
  user_map.emplace(kEntityIdMapKey, user.id);
  if (user.name) {
    user_map.emplace(kEntityNameMapKey, *user.name);
  }
  if (user.display_name &&
      (!user.display_name->empty() ||
       user.serialization_options.include_empty_display_name)) {
    user_map.emplace(kDisplayNameMapKey, *user.display_name);
  }
  return cbor::Value(std::move(user_map));
}

}  // namespace device
