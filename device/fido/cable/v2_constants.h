// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_V2_CONSTANTS_H_
#define DEVICE_FIDO_CABLE_V2_CONSTANTS_H_

#include "base/types/strong_alias.h"

namespace device {
namespace cablev2 {

namespace tunnelserver {

// KnownDomainID represents a tunnel server domain ID that maps to a known
// domain. IDs 0..256 are assigned and IDs 256..64K are hashed. Thus this type
// only contains values 256..64K or values that are assigned and the assignment
// is known in the code.
//
// Outside of tests, these values should only be created by |eid::ToComponents|
// or |tunnelserver::ToKnownTunnelID|.
using KnownDomainID =
    base::StrongAlias<class TunnelServerDomainIDTag, uint16_t>;

}  // namespace tunnelserver

// kAdvertSize is the number of bytes in an advert. This consists of a 16-byte
// UUID and a 4-byte UUID.
constexpr size_t kAdvertSize = 16 + 4;
// kNonceSize is the number of bytes of nonce in the BLE advert.
constexpr size_t kNonceSize = 10;
// kClientNonceSize is the number of bytes of nonce sent by the client, via the
// tunnel server, for a pairing-based handshake.
constexpr size_t kClientNonceSize = 16;
// kRoutingIdSize is the number of bytes of routing information in the BLE
// advert.
constexpr size_t kRoutingIdSize = 3;
// kTunnelIdSize is the number of bytes of opaque tunnel ID, used to identify a
// specific tunnel to the tunnel service.
constexpr size_t kTunnelIdSize = 16;
// kEIDKeySize is the size of the key used to encrypt BLE adverts. This is a
// 256-bit AES key and a 256-bit HMAC key.
constexpr size_t kEIDKeySize = 32 + 32;
// kPSKSize is the size of the Noise pre-shared key used in handshakes.
constexpr size_t kPSKSize = 32;
// kRootSecretSize is the size of the main key maintained by authenticators.
constexpr size_t kRootSecretSize = 32;
// kQRKeySize is the size of the private key data that generates a QR code. It
// consists of a 256-bit seed value that's used to genertate the P-256 private
// key and a 128-bit secret.
constexpr size_t kQRSecretSize = 16;
constexpr size_t kQRSeedSize = 32;
constexpr size_t kQRKeySize = kQRSeedSize + kQRSecretSize;
// kCompressedPublicKeySize is the size of a compressed X9.62 public key.
constexpr size_t kCompressedPublicKeySize =
    /* type byte */ 1 + /* field element */ (256 / 8);
// kPairingIDSize is the number of bytes in the pairing ID that is shared after
// scanning a QR code.
constexpr size_t kPairingIDSize = 8;
// kTunnelServer is the hardcoded tunnel server that phones will use for network
// communication. This specifies a Google service and the short domain seed is
// necessary to fit within a BLE advert.
constexpr auto kTunnelServer = tunnelserver::KnownDomainID(0);
// kPostHandshakeMsgPaddingGranularity is the granularity of the padding added
// to the post-handshake message. This should be sufficiently large to pad away
// all information about the contents of this message.
constexpr size_t kPostHandshakeMsgPaddingGranularity = 512;
// kMaxSyncInfoDaysForConsumer is the maximum age, in days, of sync info that
// consumers (i.e. desktops) will accept. Information in Sync's DeviceInfo
// records that is older than this will be ignored. This should be smaller than
// `kMaxSyncInfoDaysForProducer` so that the phone will always accept a
// connection.
constexpr unsigned kMaxSyncInfoDaysForConsumer = 31;
// kMaxSyncInfoDaysForProducer is the maximum age, in days, of sync info that
// producers (i.e. phones) will accept. If a desktop tries to connect using
// information that was published before this time, the request will be
// rejected. This should be larger than `kMaxSyncInfoDaysForConsumer` so that
// this doesn't happen with honest clients.
constexpr unsigned kMaxSyncInfoDaysForProducer =
    kMaxSyncInfoDaysForConsumer + 7;

// MessageType enumerates the types of caBLEv2 messages on the wire.
enum class MessageType : uint8_t {
  kShutdown = 0,
  kCTAP = 1,
  kUpdate = 2,
  kJSON = 3,

  kMaxValue = 3,
};

enum class Event {
  // kPhoneConnected means that the phone has connected to the tunnel server
  // and started BLE advertising.
  kPhoneConnected,
  // kBLEAdvertReceived means that a matching BLE advert has been
  // received and a corresponding "device" has been discovered.
  kBLEAdvertReceived,
  // kReady means that the device is ready to receive a CTAP-level message.
  kReady,
};

// PayloadType enumerates the types of application-level payloads carried over a
// hybrid connection.
enum class PayloadType {
  kCTAP,
  kJSON,
};

// Feature enumerates the features that a hybrid device can support.
enum class Feature {
  kCTAP,
  // Digital identity requests, e.g. mobile driver's licenses.
  kDigitialIdentities,
};

}  // namespace cablev2
}  // namespace device

#endif  // DEVICE_FIDO_CABLE_V2_CONSTANTS_H_
