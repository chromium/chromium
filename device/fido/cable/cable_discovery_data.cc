// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/cable_discovery_data.h"

#include <cstring>

#include "base/time/time.h"
#include "components/cbor/values.h"
#include "crypto/random.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/fido_parsing_utils.h"
#include "third_party/boringssl/src/include/openssl/aes.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/hkdf.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/obj.h"

namespace device {

CableDiscoveryData::CableDiscoveryData() = default;

CableDiscoveryData::CableDiscoveryData(
    CableDiscoveryData::Version version,
    const CableEidArray& client_eid,
    const CableEidArray& authenticator_eid,
    const CableSessionPreKeyArray& session_pre_key)
    : version(version) {
  CHECK_EQ(Version::V1, version);
  v1.emplace();
  v1->client_eid = client_eid;
  v1->authenticator_eid = authenticator_eid;
  v1->session_pre_key = session_pre_key;
}

CableDiscoveryData::CableDiscoveryData(const CableDiscoveryData& data) =
    default;

CableDiscoveryData& CableDiscoveryData::operator=(
    const CableDiscoveryData& other) = default;

CableDiscoveryData::~CableDiscoveryData() = default;

bool CableDiscoveryData::operator==(const CableDiscoveryData& other) const {
  if (version != other.version) {
    return false;
  }

  switch (version) {
    case CableDiscoveryData::Version::V1:
      return v1->client_eid == other.v1->client_eid &&
             v1->authenticator_eid == other.v1->authenticator_eid &&
             v1->session_pre_key == other.v1->session_pre_key;

    case CableDiscoveryData::Version::V2:
      return v2.value() == other.v2.value();

    case CableDiscoveryData::Version::INVALID:
      CHECK(false);
      return false;
  }
}

bool CableDiscoveryData::MatchV1(const CableEidArray& eid) const {
  DCHECK_EQ(version, Version::V1);
  return eid == v1->authenticator_eid;
}

namespace cablev2 {

Pairing::Pairing() = default;
Pairing::~Pairing() = default;

// static
base::Optional<std::unique_ptr<Pairing>> Pairing::Parse(
    const cbor::Value& cbor,
    uint32_t tunnel_server_domain,
    base::span<const uint8_t, kQRSeedSize> local_identity_seed,
    base::span<const uint8_t, 32> handshake_hash) {
  if (!cbor.is_map()) {
    return base::nullopt;
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
      std::any_of(
          its.begin(), its.end(),
          [&map](const cbor::Value::MapValue::const_iterator& it) -> bool {
            return it == map.end() || !it->second.is_bytestring();
          }) ||
      its[3]->second.GetBytestring().size() !=
          std::tuple_size<decltype(pairing->peer_public_key_x962)>::value) {
  }

  pairing->tunnel_server_domain =
      tunnelserver::DecodeDomain(tunnel_server_domain),
  pairing->contact_id = its[0]->second.GetBytestring();
  pairing->id = its[1]->second.GetBytestring();
  pairing->secret = its[2]->second.GetBytestring();
  const std::vector<uint8_t>& peer_public_key = its[3]->second.GetBytestring();
  std::copy(peer_public_key.begin(), peer_public_key.end(),
            pairing->peer_public_key_x962.begin());
  pairing->name = name_it->second.GetString();

  if (!VerifyPairingSignature(local_identity_seed,
                              pairing->peer_public_key_x962, handshake_hash,
                              its[4]->second.GetBytestring())) {
    return base::nullopt;
  }

  return pairing;
}

}  // namespace cablev2

}  // namespace device
