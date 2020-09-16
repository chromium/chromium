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

namespace {

enum class QRValue : uint8_t {
  QR_SECRET = 0,
  IDENTITY_KEY_SEED = 1,
};

void DeriveQRValue(base::span<const uint8_t, kCableQRDataSize> qr_generator_key,
                   const int64_t tick,
                   QRValue type,
                   base::span<uint8_t> out) {
  uint8_t hkdf_input[sizeof(uint64_t) + 1];
  memcpy(hkdf_input, &tick, sizeof(uint64_t));
  hkdf_input[sizeof(uint64_t)] = base::strict_cast<uint8_t>(type);

  bool ok = HKDF(out.data(), out.size(), EVP_sha256(), qr_generator_key.data(),
                 qr_generator_key.size(),
                 /*salt=*/nullptr, 0, hkdf_input, sizeof(hkdf_input));
  DCHECK(ok);
}

}  // namespace

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

CableDiscoveryData::CableDiscoveryData(
    base::span<const uint8_t, kCableQRSecretSize> qr_secret,
    base::span<const uint8_t, kCableIdentityKeySeedSize> identity_key_seed) {
  InitFromQRSecret(qr_secret);
  v2->local_identity_seed = fido_parsing_utils::Materialize(identity_key_seed);
}

// static
base::Optional<CableDiscoveryData> CableDiscoveryData::FromQRData(
    base::span<const uint8_t,
               kCableCompressedPublicKeySize + kCableQRSecretSize> qr_data) {
  auto qr_secret = qr_data.subspan(kCableCompressedPublicKeySize);
  CableDiscoveryData discovery_data;
  discovery_data.InitFromQRSecret(base::span<const uint8_t, kCableQRSecretSize>(
      qr_secret.data(), qr_secret.size()));

  bssl::UniquePtr<EC_GROUP> p256(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  bssl::UniquePtr<EC_POINT> point(EC_POINT_new(p256.get()));
  if (!EC_POINT_oct2point(p256.get(), point.get(), qr_data.data(),
                          kCableCompressedPublicKeySize, /*ctx=*/nullptr)) {
    return base::nullopt;
  }
  CableAuthenticatorIdentityKey& identity_key =
      discovery_data.v2->peer_identity.emplace();
  CHECK_EQ(identity_key.size(),
           EC_POINT_point2oct(
               p256.get(), point.get(), POINT_CONVERSION_UNCOMPRESSED,
               identity_key.data(), identity_key.size(), /*ctx=*/nullptr));

  return discovery_data;
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
      return v2->eid_gen_key == other.v2->eid_gen_key &&
             v2->psk_gen_key == other.v2->psk_gen_key &&
             v2->peer_identity == other.v2->peer_identity &&
             v2->peer_name == other.v2->peer_name;

    case CableDiscoveryData::Version::INVALID:
      CHECK(false);
      return false;
  }
}

bool CableDiscoveryData::MatchV1(const CableEidArray& eid) const {
  DCHECK_EQ(version, Version::V1);
  return eid == v1->authenticator_eid;
}

bool CableDiscoveryData::MatchV2(const CableEidArray& eid,
                                 CableEidArray* out_eid) const {
  DCHECK_EQ(version, Version::V2);

  // Attempt to decrypt the EID with the EID generator key and check whether
  // it has a valid structure.
  AES_KEY key;
  CableEidArray& out = *out_eid;
  CHECK(AES_set_decrypt_key(v2->eid_gen_key.data(),
                            /*bits=*/8 * v2->eid_gen_key.size(), &key) == 0);
  static_assert(kCableEphemeralIdSize == AES_BLOCK_SIZE,
                "EIDs are not AES blocks");
  AES_decrypt(/*in=*/eid.data(), /*out=*/out.data(), &key);
  return cablev2::eid::IsValid(out);
}

// static
QRGeneratorKey CableDiscoveryData::NewQRKey() {
  QRGeneratorKey key;
  crypto::RandBytes(key.data(), key.size());
  return key;
}

// static
int64_t CableDiscoveryData::CurrentTimeTick() {
  // The ticks are currently 256ms.
  return base::TimeTicks::Now().since_origin().InMilliseconds() >> 8;
}

// static
std::array<uint8_t, kCableQRSecretSize> CableDiscoveryData::DeriveQRSecret(
    base::span<const uint8_t, kCableQRDataSize> qr_generator_key,
    const int64_t tick) {
  std::array<uint8_t, kCableQRSecretSize> ret;
  DeriveQRValue(qr_generator_key, tick, QRValue::QR_SECRET, ret);
  return ret;
}

// static
CableIdentityKeySeed CableDiscoveryData::DeriveIdentityKeySeed(
    base::span<const uint8_t, kCableQRDataSize> qr_generator_key,
    const int64_t tick) {
  std::array<uint8_t, kCableIdentityKeySeedSize> ret;
  DeriveQRValue(qr_generator_key, tick, QRValue::IDENTITY_KEY_SEED, ret);
  return ret;
}

// static
CableQRData CableDiscoveryData::DeriveQRData(
    base::span<const uint8_t, kCableQRDataSize> qr_generator_key,
    const int64_t tick) {
  auto identity_key_seed = DeriveIdentityKeySeed(qr_generator_key, tick);
  bssl::UniquePtr<EC_GROUP> p256(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  bssl::UniquePtr<EC_KEY> identity_key(EC_KEY_derive_from_secret(
      p256.get(), identity_key_seed.data(), identity_key_seed.size()));
  const EC_POINT* public_key = EC_KEY_get0_public_key(identity_key.get());
  CableQRData qr_data;
  static_assert(
      qr_data.size() == kCableCompressedPublicKeySize + kCableQRSecretSize,
      "this code needs to be updated");
  CHECK_EQ(kCableCompressedPublicKeySize,
           EC_POINT_point2oct(p256.get(), public_key,
                              POINT_CONVERSION_COMPRESSED, qr_data.data(),
                              kCableCompressedPublicKeySize, /*ctx=*/nullptr));

  auto qr_secret = CableDiscoveryData::DeriveQRSecret(qr_generator_key, tick);
  memcpy(&qr_data.data()[kCableCompressedPublicKeySize], qr_secret.data(),
         qr_secret.size());

  return qr_data;
}

CableDiscoveryData::V2Data::V2Data() = default;
CableDiscoveryData::V2Data::V2Data(const V2Data&) = default;
CableDiscoveryData::V2Data::~V2Data() = default;

void CableDiscoveryData::InitFromQRSecret(
    base::span<const uint8_t, kCableQRSecretSize> qr_secret) {
  version = Version::V2;
  v2.emplace();

  static const char kEIDGen[] = "caBLE QR to EID generator key";
  bool ok =
      HKDF(v2->eid_gen_key.data(), v2->eid_gen_key.size(), EVP_sha256(),
           qr_secret.data(), qr_secret.size(), /*salt=*/nullptr, 0,
           reinterpret_cast<const uint8_t*>(kEIDGen), sizeof(kEIDGen) - 1);
  DCHECK(ok);

  static const char kPSKGen[] = "caBLE QR to PSK generator key";
  ok = HKDF(v2->psk_gen_key.data(), v2->psk_gen_key.size(), EVP_sha256(),
            qr_secret.data(), qr_secret.size(), /*salt=*/nullptr, 0,
            reinterpret_cast<const uint8_t*>(kPSKGen), sizeof(kPSKGen) - 1);
  DCHECK(ok);

  static const char kTunnelIDGen[] = "caBLE QR to tunnel ID generator key";
  ok = HKDF(v2->tunnel_id_gen_key.data(), v2->tunnel_id_gen_key.size(),
            EVP_sha256(), qr_secret.data(), qr_secret.size(), /*salt=*/nullptr,
            0, reinterpret_cast<const uint8_t*>(kTunnelIDGen),
            sizeof(kTunnelIDGen) - 1);
  DCHECK(ok);
}

namespace cablev2 {

Pairing::Pairing() = default;
Pairing::~Pairing() = default;

// static
base::Optional<std::unique_ptr<Pairing>> Pairing::Parse(
    const cbor::Value& cbor,
    uint32_t tunnel_server_domain,
    base::span<const uint8_t, kCableIdentityKeySeedSize> local_identity_seed,
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
          &its[0], &its[its.size()],
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
