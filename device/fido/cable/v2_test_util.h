// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_V2_TEST_UTIL_H_
#define DEVICE_FIDO_CABLE_V2_TEST_UTIL_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/containers/span.h"
#include "base/optional.h"
#include "device/fido/cable/v2_constants.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"

namespace device {

class VirtualCtap2Device;

namespace cablev2 {

class Discovery;

// ContactCallback is called when a mock tunnel server (see
// |NewMockTunnelServer|) is asked to contact a phone. This simulates a tunnel
// server using a cloud messaging solution to wake a device.
using ContactCallback = base::RepeatingCallback<void(
    base::span<const uint8_t, kTunnelIdSize> tunnel_id,
    base::span<const uint8_t> pairing_id,
    base::span<const uint8_t, kClientNonceSize> client_nonce)>;

// NewMockTunnelServer returns a |NetworkContext| that implements WebSocket
// requests and simulates a tunnel server. If the given |contact_callback| is
// |nullopt| then all contact requests will be rejected with an HTTP 410 status
// to indicate that the contact ID is disabled.
std::unique_ptr<network::mojom::NetworkContext> NewMockTunnelServer(
    base::Optional<ContactCallback> contact_callback);

namespace authenticator {

class Platform;

// NewMockPlatform returns a |Platform| that implements the makeCredential
// operation by forwarding it to |ctap2_device|. Transmitted BLE adverts are
// forwarded to |discovery|.
std::unique_ptr<Platform> NewMockPlatform(
    Discovery* discovery,
    device::VirtualCtap2Device* ctap2_device);

}  // namespace authenticator

}  // namespace cablev2
}  // namespace device

#endif  // DEVICE_FIDO_CABLE_V2_TEST_UTIL_H_
