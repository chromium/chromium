// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_V2_TEST_UTIL_H_
#define DEVICE_FIDO_CABLE_V2_TEST_UTIL_H_

#include <memory>
#include <optional>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "device/fido/cable/v2_authenticator.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/cable/v2_discovery.h"
#include "device/fido/network_context_factory.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"

namespace device {

class VirtualCtap2Device;

namespace cablev2 {

// ContactCallback is called when a mock tunnel server (see
// |NewMockTunnelServer|) is asked to contact a phone. This simulates a tunnel
// server using a cloud messaging solution to wake a device.
using ContactCallback = base::RepeatingCallback<void(
    base::span<const uint8_t, kTunnelIdSize> tunnel_id,
    base::span<const uint8_t, kPairingIDSize> pairing_id,
    base::span<const uint8_t, kClientNonceSize> client_nonce,
    const std::string& request_type_hint)>;

// NewMockTunnelServer returns a |NetworkContext| that implements WebSocket
// requests and simulates a tunnel server. If the given |contact_callback| is
// |nullopt| then all contact requests will be rejected with an HTTP 410 status
// to indicate that the contact ID is disabled.
std::unique_ptr<network::mojom::NetworkContext> NewMockTunnelServer(
    std::optional<ContactCallback> contact_callback,
    bool supports_connect_signal = false);

namespace authenticator {

// Observer is an interface that can be implemented by tests that wish to see
// certain platform events.
class Observer {
 public:
  // See `Platform::OnStatus`.
  virtual void OnStatus(Platform::Status) = 0;

  // See `Platform::OnCompleted`.
  virtual void OnCompleted(std::optional<Platform::Error>) = 0;
};

// NewMockPlatform returns a |Platform| that implements the makeCredential
// operation by forwarding it to |ctap2_device|. Transmitted BLE adverts are
// forwarded to |ble_advert_callback|. |observer| may be |nullptr| but, if not,
// then corresponding calls to the mock `Platform` are forwarded to the
// observer.
std::unique_ptr<Platform> NewMockPlatform(
    Discovery::AdvertEventStream::Callback ble_advert_callback,
    device::VirtualCtap2Device* ctap2_device,
    Observer* observer);

// NewLateLinkingDevice returns a caBLEv2 device that fails all CTAP requests
// but sends linking information after the transaction.
std::unique_ptr<Transaction> NewLateLinkingDevice(
    CtapDeviceResponseCode ctap_error,
    std::unique_ptr<Platform> platform,
    NetworkContextFactory network_context_factory,
    base::span<const uint8_t> qr_secret,
    base::span<const uint8_t, kP256X962Length> peer_identity);

// NewHandshakeErrorDevice returns a caBLEv2 device that produces an invalid
// caBLEv2 handshake.
std::unique_ptr<Transaction> NewHandshakeErrorDevice(
    std::unique_ptr<Platform> platform,
    NetworkContextFactory network_context_factory,
    base::span<const uint8_t> qr_secret);

}  // namespace authenticator

}  // namespace cablev2
}  // namespace device

#endif  // DEVICE_FIDO_CABLE_V2_TEST_UTIL_H_
