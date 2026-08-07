// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/pairing.h"

#include <algorithm>
#include <tuple>

#include "components/cbor/values.h"
#include "device/fido/cable/v2_handshake.h"

namespace device::cablev2 {

Pairing::Pairing() = default;
Pairing::~Pairing() = default;

// static
std::optional<std::unique_ptr<Pairing>> Pairing::Parse(
    const cbor::Value& cbor,
    tunnelserver::KnownDomainID domain,
    base::span<const uint8_t, kQRSeedSize> local_identity_seed,
    base::span<const uint8_t, 32> handshake_hash) {
  if (!cbor.is_map()) {
    return std::nullopt;
  }

  const cbor::Value::MapValue& map = cbor.GetMap();
  auto pairing = std::make_unique<Pairing>();

  const std::array<cbor::Value::MapValue::const_iterator, 5> its = {
      map.find(cbor::Value(1)), map.find(cbor::Value(2)),
      map.find(cbor::Value(3)), map.find(cbor::Value(4)),
      map.find(cbor::Value(6))};
  const cbor::Value::MapValue::const_iterator name_it =
      map.find(cbor::Value(5));
  if (name_it == map.end() || !name_it->second.is_string() ||
      std::ranges::any_of(
          its,
          [&map](const cbor::Value::MapValue::const_iterator& it) {
            return it == map.end() || !it->second.is_bytestring();
          }) ||
      its[3]->second.GetBytestring().size() !=
          std::tuple_size<decltype(pairing->peer_public_key_x962)>::value) {
    return std::nullopt;
  }

  pairing->tunnel_server_domain = domain;
  pairing->contact_id = its[0]->second.GetBytestring();
  pairing->id = its[1]->second.GetBytestring();
  pairing->secret = its[2]->second.GetBytestring();
  const std::vector<uint8_t>& peer_public_key = its[3]->second.GetBytestring();
  std::ranges::copy(peer_public_key, pairing->peer_public_key_x962.begin());
  pairing->name = name_it->second.GetString();

  if (!VerifyPairingSignature(local_identity_seed,
                              pairing->peer_public_key_x962, handshake_hash,
                              its[4]->second.GetBytestring())) {
    return std::nullopt;
  }

  const auto play_services_tag_it = map.find(cbor::Value(999));
  if (play_services_tag_it != map.end() &&
      play_services_tag_it->second.is_bool() &&
      play_services_tag_it->second.GetBool()) {
    pairing->from_new_implementation = true;
  }

  return pairing;
}

// static
bool Pairing::EqualPublicKeys(const std::unique_ptr<Pairing>& a,
                              const std::unique_ptr<Pairing>& b) {
  return a->peer_public_key_x962 == b->peer_public_key_x962;
}

Pairing::Pairing(const Pairing&) = default;
Pairing& Pairing::operator=(const Pairing&) = default;

}  // namespace device::cablev2
