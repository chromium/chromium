// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/public/cable_discovery_data.h"

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/notreached.h"

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
      NOTREACHED();
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

}  // namespace device
