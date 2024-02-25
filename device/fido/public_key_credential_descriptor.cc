// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "device/fido/fido_transport_protocol.h"
#include "device/fido/public_key_credential_descriptor.h"

namespace device {

namespace {

// Keys for storing credential descriptor information in CBOR map.
constexpr char kCredentialIdKey[] = "id";
constexpr char kCredentialTypeKey[] = "type";

}  // namespace

// static
std::optional<PublicKeyCredentialDescriptor>
PublicKeyCredentialDescriptor::CreateFromCBORValue(const cbor::Value& cbor) {
  if (!cbor.is_map()) {
    return std::nullopt;
  }

  const cbor::Value::MapValue& map = cbor.GetMap();
  auto type = map.find(cbor::Value(kCredentialTypeKey));
  if (type == map.end() || !type->second.is_string() ||
      type->second.GetString() != kPublicKey)
    return std::nullopt;

  auto id = map.find(cbor::Value(kCredentialIdKey));
  if (id == map.end() || !id->second.is_bytestring())
    return std::nullopt;

  auto ret = PublicKeyCredentialDescriptor(CredentialType::kPublicKey,
                                           id->second.GetBytestring());
  // If the map had other keys then this fact is recorded for testing because
  // some security keys appear to have a parsing bug in this case. See
  // crbug.com/1270757.
  ret.had_other_keys = map.size() > 2;
  return ret;
}

PublicKeyCredentialDescriptor::PublicKeyCredentialDescriptor() = default;

PublicKeyCredentialDescriptor::PublicKeyCredentialDescriptor(
    CredentialType credential_type,
    std::vector<uint8_t> id)
    : PublicKeyCredentialDescriptor(credential_type, std::move(id), {}) {}

PublicKeyCredentialDescriptor::PublicKeyCredentialDescriptor(
    CredentialType credential_type,
    std::vector<uint8_t> id,
    base::flat_set<FidoTransportProtocol> transports)
    : credential_type(credential_type),
      id(std::move(id)),
      transports(std::move(transports)) {}

PublicKeyCredentialDescriptor::PublicKeyCredentialDescriptor(
    const PublicKeyCredentialDescriptor& other) = default;

PublicKeyCredentialDescriptor::PublicKeyCredentialDescriptor(
    PublicKeyCredentialDescriptor&& other) = default;

PublicKeyCredentialDescriptor& PublicKeyCredentialDescriptor::operator=(
    const PublicKeyCredentialDescriptor& other) = default;

PublicKeyCredentialDescriptor& PublicKeyCredentialDescriptor::operator=(
    PublicKeyCredentialDescriptor&& other) = default;

PublicKeyCredentialDescriptor::~PublicKeyCredentialDescriptor() = default;

bool PublicKeyCredentialDescriptor::operator==(
    const PublicKeyCredentialDescriptor& other) const {
  return credential_type == other.credential_type && id == other.id &&
         transports == other.transports;
}

cbor::Value AsCBOR(const PublicKeyCredentialDescriptor& desc) {
  cbor::Value::MapValue cbor_descriptor_map;
  cbor_descriptor_map[cbor::Value(kCredentialIdKey)] = cbor::Value(desc.id);
  cbor_descriptor_map[cbor::Value(kCredentialTypeKey)] =
      cbor::Value(CredentialTypeToString(desc.credential_type));
  // Transports are omitted from CBOR serialization. They aren't useful for
  // security keys to process. Some existing devices even refuse to parse them
  // (see https://crbug.com/1270757).
  return cbor::Value(std::move(cbor_descriptor_map));
}

}  // namespace device
