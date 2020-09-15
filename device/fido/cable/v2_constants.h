// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_V2_CONSTANTS_H_
#define DEVICE_FIDO_CABLE_V2_CONSTANTS_H_

namespace device {
namespace cablev2 {

// kNonceSize is the number of bytes of nonce in the BLE advert.
constexpr size_t kNonceSize = 8;
// kClientNonceSize is the number of bytes of nonce sent by the client, via the
// tunnel server, for a pairing-based handshake.
constexpr size_t kClientNonceSize = 16;
// kRoutingIdSize is the number of bytes of routing information in the BLE
// advert.
constexpr size_t kRoutingIdSize = 3;
// kTunnelIdSize is the number of bytes of opaque tunnel ID, used to identify a
// specific tunnel to the tunnel service.
constexpr size_t kTunnelIdSize = 16;
// kEIDKeySize is the size of the AES key used to encrypt BLE adverts.
constexpr size_t kEIDKeySize = 32;
// kPSKSize is the size of the Noise pre-shared key used in handshakes.
constexpr size_t kPSKSize = 32;
// kRootSecretSize is the size of the main key maintained by authenticators.
constexpr size_t kRootSecretSize = 32;

}  // namespace cablev2
}  // namespace device

#endif  // DEVICE_FIDO_CABLE_V2_CONSTANTS_H_
