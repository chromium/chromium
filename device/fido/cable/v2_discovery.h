// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_V2_DISCOVERY_H_
#define DEVICE_FIDO_CABLE_V2_DISCOVERY_H_

#include <array>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/fido_device_discovery.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"

namespace device {
namespace cablev2 {

struct Pairing;
class FidoTunnelDevice;

// Discovery creates caBLEv2 devices, either based on |pairings|, or when a BLE
// advert is seen that matches |qr_generator_key|. It does not actively scan for
// BLE adverts itself. Rather it depends on |OnBLEAdvertSeen| getting called.
class COMPONENT_EXPORT(DEVICE_FIDO) Discovery
    : public FidoDeviceDiscovery,
      public FidoDeviceDiscovery::BLEObserver {
 public:
  Discovery(
      network::mojom::NetworkContext* network_context,
      base::Optional<base::span<const uint8_t, kQRKeySize>> qr_generator_key,
      std::vector<std::unique_ptr<Pairing>> pairings,
      const std::vector<CableDiscoveryData>& extension_contents,
      // pairing_callback will be called when a QR-initiated connection
      // receives pairing information from the peer, or when an existing
      // pairing is found to be invalid.
      base::Optional<base::RepeatingCallback<void(PairingEvent)>>
          pairing_callback);
  ~Discovery() override;
  Discovery(const Discovery&) = delete;
  Discovery& operator=(const Discovery&) = delete;

  // FidoDeviceDiscovery:
  void StartInternal() override;

  // BLEObserver:
  void OnBLEAdvertSeen(const std::array<uint8_t, kAdvertSize>& advert) override;

 private:
  // UnpairedKeys are keys that are conveyed by QR code or that come from the
  // server, i.e. keys that enable interactions with unpaired phones.
  struct UnpairedKeys {
    std::array<uint8_t, kQRSeedSize> local_identity_seed;
    std::array<uint8_t, kQRSecretSize> qr_secret;
    std::array<uint8_t, kEIDKeySize> eid_key;
  };

  void AddPairing(std::unique_ptr<Pairing> pairing);
  void PairingIsInvalid(
      std::array<uint8_t, kP256X962Length> peer_public_key_x962);
  static base::Optional<UnpairedKeys> KeysFromQRGeneratorKey(
      base::Optional<base::span<const uint8_t, kQRKeySize>> qr_generator_key);
  static base::Optional<UnpairedKeys> KeysFromExtension(
      const std::vector<CableDiscoveryData>& extension_contents);

  network::mojom::NetworkContext* const network_context_;
  const base::Optional<UnpairedKeys> qr_keys_;
  const base::Optional<UnpairedKeys> extension_keys_;
  std::vector<std::unique_ptr<Pairing>> pairings_;
  const base::Optional<base::RepeatingCallback<void(PairingEvent)>>
      pairing_callback_;
  std::vector<std::unique_ptr<FidoTunnelDevice>> tunnels_pending_advert_;
  base::flat_set<std::array<uint8_t, kAdvertSize>> observed_adverts_;
  bool started_ = false;
  std::vector<std::array<uint8_t, kAdvertSize>> pending_adverts_;
  base::WeakPtrFactory<Discovery> weak_factory_{this};
};

}  // namespace cablev2
}  // namespace device

#endif  // DEVICE_FIDO_CABLE_V2_DISCOVERY_H_
