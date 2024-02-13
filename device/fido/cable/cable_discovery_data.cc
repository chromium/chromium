// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/cable_discovery_data.h"

#include <cstring>

#include "base/check_op.h"
#include "base/i18n/string_compare.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
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
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/i18n/unicode/coll.h"

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

CableDiscoveryData::V2Data::V2Data(std::vector<uint8_t> server_link_data_in,
                                   std::vector<uint8_t> experiments_in)
    : server_link_data(std::move(server_link_data_in)),
      experiments(std::move(experiments_in)) {}

CableDiscoveryData::V2Data::V2Data(const V2Data&) = default;

CableDiscoveryData::V2Data::~V2Data() = default;

bool CableDiscoveryData::V2Data::operator==(const V2Data& other) const {
  return server_link_data == other.server_link_data &&
         experiments == other.experiments;
}

namespace cablev2 {

Pairing::NameComparator::NameComparator(const icu::Locale* locale) {
  UErrorCode error = U_ZERO_ERROR;
  collator_.reset(icu::Collator::createInstance(*locale, error));
}

Pairing::NameComparator::NameComparator(NameComparator&&) = default;

Pairing::NameComparator::~NameComparator() = default;

bool Pairing::NameComparator::operator()(const std::unique_ptr<Pairing>& a,
                                         const std::unique_ptr<Pairing>& b) {
  return base::i18n::CompareString16WithCollator(
             *collator_, base::UTF8ToUTF16(a->name),
             base::UTF8ToUTF16(b->name)) == UCOL_LESS;
}

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
      base::ranges::any_of(
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
  base::ranges::copy(peer_public_key, pairing->peer_public_key_x962.begin());
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
bool Pairing::CompareByMostRecentFirst(const std::unique_ptr<Pairing>& a,
                                       const std::unique_ptr<Pairing>& b) {
  return a->last_updated > b->last_updated;
}

// static
bool Pairing::CompareByLeastStableChannelFirst(
    const std::unique_ptr<Pairing>& a,
    const std::unique_ptr<Pairing>& b) {
  return a->channel_priority > b->channel_priority;
}

// static
bool Pairing::CompareByPublicKey(const std::unique_ptr<Pairing>& a,
                                 const std::unique_ptr<Pairing>& b) {
  return memcmp(a->peer_public_key_x962.data(), b->peer_public_key_x962.data(),
                sizeof(a->peer_public_key_x962)) < 0;
}

// static
Pairing::NameComparator Pairing::CompareByName(const icu::Locale* locale) {
  return NameComparator(locale);
}

// static
bool Pairing::EqualPublicKeys(const std::unique_ptr<Pairing>& a,
                              const std::unique_ptr<Pairing>& b) {
  return a->peer_public_key_x962 == b->peer_public_key_x962;
}

Pairing::Pairing(const Pairing&) = default;
Pairing& Pairing::operator=(const Pairing&) = default;

}  // namespace cablev2

}  // namespace device
