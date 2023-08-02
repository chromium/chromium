// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_V2_DISCOVERY_H_
#define DEVICE_FIDO_CABLE_V2_DISCOVERY_H_

#include <array>
#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_device_discovery.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device::cablev2 {

struct Pairing;
class FidoTunnelDevice;

// Discovery creates caBLEv2 devices, either based on |pairings|, or when a BLE
// advert is seen that matches |qr_generator_key|. It does not actively scan for
// BLE adverts itself. Rather it depends on |OnBLEAdvertSeen| getting called.
class COMPONENT_EXPORT(DEVICE_FIDO) Discovery : public FidoDeviceDiscovery {
 public:
  using AdvertEventStream = EventStream<base::span<const uint8_t, kAdvertSize>>;

  Discovery(
      FidoRequestType request_type,
      network::mojom::NetworkContext* network_context,
      absl::optional<base::span<const uint8_t, kQRKeySize>> qr_generator_key,
      std::unique_ptr<AdvertEventStream> advert_stream,
      // contact_device_stream contains a series of pairings indicating that the
      // given device should be contacted. The pairings may be duplicated. It
      // may be nullptr.
      std::unique_ptr<EventStream<std::unique_ptr<Pairing>>>
          contact_device_stream,
      const std::vector<CableDiscoveryData>& extension_contents,
      // pairing_callback will be called when a QR-initiated connection
      // receives pairing information from the peer.
      absl::optional<base::RepeatingCallback<void(std::unique_ptr<Pairing>)>>
          pairing_callback,
      // invalidated_pairing_callback will be called when a pairing is reported
      // to be invalid by the tunnel server.
      absl::optional<base::RepeatingCallback<void(std::unique_ptr<Pairing>)>>
          invalidated_pairing_callback,
      // event_callback receives updates on cablev2 events.
      absl::optional<base::RepeatingCallback<void(Event)>> event_callback);
  ~Discovery() override;
  Discovery(const Discovery&) = delete;
  Discovery& operator=(const Discovery&) = delete;

  // FidoDeviceDiscovery:
  void StartInternal() override;

 private:
  // UnpairedKeys are keys that are conveyed by QR code or that come from the
  // server, i.e. keys that enable interactions with unpaired phones.
  struct UnpairedKeys {
    std::array<uint8_t, kQRSeedSize> local_identity_seed;
    std::array<uint8_t, kQRSecretSize> qr_secret;
    std::array<uint8_t, kEIDKeySize> eid_key;
  };

  void OnBLEAdvertSeen(base::span<const uint8_t, kAdvertSize> advert);
  void OnContactDevice(std::unique_ptr<Pairing> pairing);
  void PairingIsInvalid(std::unique_ptr<Pairing> pairing);
  static absl::optional<UnpairedKeys> KeysFromQRGeneratorKey(
      absl::optional<base::span<const uint8_t, kQRKeySize>> qr_generator_key);
  static std::vector<UnpairedKeys> KeysFromExtension(
      const std::vector<CableDiscoveryData>& extension_contents);

  const FidoRequestType request_type_;
  const raw_ptr<network::mojom::NetworkContext> network_context_;
  const absl::optional<UnpairedKeys> qr_keys_;
  const std::vector<UnpairedKeys> extension_keys_;
  std::unique_ptr<AdvertEventStream> advert_stream_;
  std::unique_ptr<EventStream<std::unique_ptr<Pairing>>> contact_device_stream_;
  const absl::optional<base::RepeatingCallback<void(std::unique_ptr<Pairing>)>>
      pairing_callback_;
  const absl::optional<base::RepeatingCallback<void(std::unique_ptr<Pairing>)>>
      invalidated_pairing_callback_;
  const absl::optional<base::RepeatingCallback<void(Event)>> event_callback_;
  std::vector<std::unique_ptr<FidoTunnelDevice>> tunnels_pending_advert_;
  base::flat_set<std::array<uint8_t, kAdvertSize>> observed_adverts_;
  bool started_ = false;
  bool device_committed_ = false;
  std::vector<std::array<uint8_t, kAdvertSize>> pending_adverts_;
  base::WeakPtrFactory<Discovery> weak_factory_{this};
};

}  // namespace device::cablev2

#endif  // DEVICE_FIDO_CABLE_V2_DISCOVERY_H_
