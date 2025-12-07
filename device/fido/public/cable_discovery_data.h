// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_PUBLIC_CABLE_DISCOVERY_DATA_H_
#define DEVICE_FIDO_PUBLIC_CABLE_DISCOVERY_DATA_H_

#include <stdint.h>

#include <array>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "device/fido/public/fido_constants.h"

namespace device {

inline constexpr size_t kCableEphemeralIdSize = 16;
inline constexpr size_t kCableSessionPreKeySize = 32;
inline constexpr size_t kCableNonceSize = 8;

using CableEidArray = std::array<uint8_t, kCableEphemeralIdSize>;
using CableSessionPreKeyArray = std::array<uint8_t, kCableSessionPreKeySize>;
// CableNonce is a nonce used in BLE handshaking.
using CableNonce = std::array<uint8_t, 8>;
// CableEidGeneratorKey is an AES-256 key that is used to encrypt a 64-bit nonce
// and 64-bits of zeros, resulting in a BLE-advertised EID.
using CableEidGeneratorKey = std::array<uint8_t, 32>;
// CablePskGeneratorKey is HKDF input keying material that is used to
// generate a Noise PSK given the nonce decrypted from an EID.
using CablePskGeneratorKey = std::array<uint8_t, 32>;
using CableTunnelIDGeneratorKey = std::array<uint8_t, 32>;
// CableAuthenticatorIdentityKey is a P-256 public value used to authenticate a
// paired phone.
using CableAuthenticatorIdentityKey = std::array<uint8_t, kP256X962Length>;

// Encapsulates information required to discover Cable device per single
// credential. When multiple credentials are enrolled to a single account
// (i.e. more than one phone has been enrolled to an user account as a
// security key), then FidoCableDiscovery must advertise for all of the client
// EID received from the relying party.
// TODO(hongjunchoi): Add discovery data required for MakeCredential request.
// See: https://crbug.com/837088
struct COMPONENT_EXPORT(FIDO_PUBLIC) CableDiscoveryData {
  enum class Version {
    INVALID,
    V1,
    V2,
  };

  CableDiscoveryData(Version version,
                     const CableEidArray& client_eid,
                     const CableEidArray& authenticator_eid,
                     const CableSessionPreKeyArray& session_pre_key);
  CableDiscoveryData();
  CableDiscoveryData(const CableDiscoveryData& data);
  ~CableDiscoveryData();

  CableDiscoveryData& operator=(const CableDiscoveryData& other);
  bool operator==(const CableDiscoveryData& other) const;

  // MatchV1 returns true if `candidate_eid` matches this caBLE discovery
  // instance, which must be version one.
  bool MatchV1(const CableEidArray& candidate_eid) const;

  // version indicates whether v1 or v2 data is contained in this object.
  // `INVALID` is not a valid version but is set as the default to catch any
  // cases where the version hasn't been set explicitly.
  Version version = Version::INVALID;

  struct V1Data {
    CableEidArray client_eid;
    CableEidArray authenticator_eid;
    CableSessionPreKeyArray session_pre_key;
  };
  std::optional<V1Data> v1;

  // For caBLEv2, the payload is the server-link data provided in the extension
  // as the "sessionPreKey".
  struct COMPONENT_EXPORT(FIDO_PUBLIC) V2Data {
    V2Data(std::vector<uint8_t> server_link_data,
           std::vector<uint8_t> experiments);
    V2Data(const V2Data&);
    ~V2Data();
    bool operator==(const V2Data&) const;

    std::vector<uint8_t> server_link_data;
    std::vector<uint8_t> experiments;
  };
  std::optional<V2Data> v2;
};

}  // namespace device

#endif  // DEVICE_FIDO_PUBLIC_CABLE_DISCOVERY_DATA_H_
