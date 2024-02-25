// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_CABLE_DISCOVERY_DATA_H_
#define DEVICE_FIDO_CABLE_CABLE_DISCOVERY_DATA_H_

#include <stdint.h>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/time/time.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/fido_constants.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/icu/source/common/unicode/uversion.h"

namespace cbor {
class Value;
}

// third_party/icu/source/common/unicode/uversion.h will set namespace icu.
namespace U_ICU_NAMESPACE {
class Collator;
class Locale;
}  // namespace U_ICU_NAMESPACE

namespace device {

constexpr size_t kCableEphemeralIdSize = 16;
constexpr size_t kCableSessionPreKeySize = 32;
constexpr size_t kCableNonceSize = 8;

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
struct COMPONENT_EXPORT(DEVICE_FIDO) CableDiscoveryData {
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
  struct COMPONENT_EXPORT(DEVICE_FIDO) V2Data {
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

namespace cablev2 {

// Pairing represents information previously received from a caBLEv2
// authenticator that enables future interactions to skip scanning a QR code.
struct COMPONENT_EXPORT(DEVICE_FIDO) Pairing {
  // NameComparator is a less-than operation for sorting `Pairing` by name.
  // See `CompareByName`.
  class COMPONENT_EXPORT(DEVICE_FIDO) NameComparator {
   public:
    explicit NameComparator(const icu::Locale* locale);
    NameComparator(NameComparator&&);
    NameComparator(const NameComparator&) = delete;
    NameComparator& operator=(const NameComparator&) = delete;
    ~NameComparator();

    bool operator()(const std::unique_ptr<Pairing>&,
                    const std::unique_ptr<Pairing>&);

   private:
    std::unique_ptr<icu::Collator> collator_;
  };

  Pairing();
  ~Pairing();
  Pairing(const Pairing&);
  Pairing& operator=(const Pairing&);

  // Parse builds a `Pairing` from an authenticator message. The signature
  // within the structure is validated by using `local_identity_seed` and
  // `handshake_hash`.
  static std::optional<std::unique_ptr<Pairing>> Parse(
      const cbor::Value& cbor,
      tunnelserver::KnownDomainID domain,
      base::span<const uint8_t, kQRSeedSize> local_identity_seed,
      base::span<const uint8_t, 32> handshake_hash);

  static bool CompareByMostRecentFirst(const std::unique_ptr<Pairing>&,
                                       const std::unique_ptr<Pairing>&);
  static bool CompareByLeastStableChannelFirst(const std::unique_ptr<Pairing>&,
                                               const std::unique_ptr<Pairing>&);
  static bool CompareByPublicKey(const std::unique_ptr<Pairing>&,
                                 const std::unique_ptr<Pairing>&);
  static NameComparator CompareByName(const icu::Locale* locale);
  static bool EqualPublicKeys(const std::unique_ptr<Pairing>&,
                              const std::unique_ptr<Pairing>&);

  // tunnel_server_domain is the encoded 16-bit value in the BLE advert.
  tunnelserver::KnownDomainID tunnel_server_domain = kTunnelServer;
  // contact_id is an opaque value that is sent to the tunnel service in order
  // to identify the caBLEv2 authenticator.
  std::vector<uint8_t> contact_id;
  // id is an opaque identifier that is sent via the tunnel service, to the
  // authenticator, to identify this specific pairing.
  std::vector<uint8_t> id;
  // secret is the shared secret that authenticates the desktop to the
  // authenticator.
  std::vector<uint8_t> secret;
  // peer_public_key_x962 is the authenticator's public key.
  std::array<uint8_t, kP256X962Length> peer_public_key_x962{};
  // name is a human-friendly name for the authenticator, specified by that
  // authenticator. (For example "Pixel 3".)
  std::string name;
  // last_updated is populated for pairings learned from Sync.
  base::Time last_updated;
  // from_sync_deviceinfo is true iff this `Pairing` was derived from a
  // DeviceInfo record in Sync, rather than from scanning a QR code. (Note that
  // the results of QR scanning may also be distributed via Sync, but that
  // wouldn't cause this value to be true.)
  bool from_sync_deviceinfo = false;
  // channel_priority is populated when `from_sync_deviceinfo` is true. It
  // contains a higher number for less stable release channels (i.e. Canary is
  // high, development builds are highest).
  int channel_priority = 0;
  // from_new_implementation is true if this Pairing was generated by the new
  // hybrid implementation on Android.
  bool from_new_implementation = false;
};

}  // namespace cablev2

}  // namespace device

#endif  // DEVICE_FIDO_CABLE_CABLE_DISCOVERY_DATA_H_
