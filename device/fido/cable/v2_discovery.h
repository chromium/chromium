// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_V2_DISCOVERY_H_
#define DEVICE_FIDO_CABLE_V2_DISCOVERY_H_

#include <array>
#include <memory>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/fido/cable/pairing.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/fido_device_discovery.h"
#include "device/fido/network_context_factory.h"
#include "device/fido/public/cable_discovery_data.h"
#include "device/fido/public/fido_constants.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "device/bluetooth/bluetooth_low_energy_scan_session.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace device::cablev2 {

struct Pairing;
class FidoTunnelDevice;

// Discovery creates caBLEv2 devices, either based on |pairings|, or when a BLE
// advert is seen that matches |qr_generator_key|.
class COMPONENT_EXPORT(DEVICE_FIDO) Discovery
    : public FidoDeviceDiscovery,
#if BUILDFLAG(IS_CHROMEOS)
      public device::BluetoothLowEnergyScanSession::Delegate,
#endif  // BUILDFLAG(IS_CHROMEOS)
      public BluetoothAdapter::Observer {
 public:
  Discovery(
      FidoRequestType request_type,
      NetworkContextFactory network_context_factory,
      std::optional<base::span<const uint8_t, kQRKeySize>> qr_generator_key,
      // contact_device_stream contains a series of pairings indicating that the
      // given device should be contacted. The pairings may be duplicated. It
      // may be nullptr.
      std::unique_ptr<EventStream<std::unique_ptr<Pairing>>>
          contact_device_stream,
      const std::vector<CableDiscoveryData>& extension_contents,
      // pairing_callback will be called when a QR-initiated connection
      // receives pairing information from the peer.
      std::optional<base::RepeatingCallback<void(std::unique_ptr<Pairing>)>>
          pairing_callback,
      // invalidated_pairing_callback will be called when a pairing is reported
      // to be invalid by the tunnel server.
      std::optional<base::RepeatingCallback<void(std::unique_ptr<Pairing>)>>
          invalidated_pairing_callback,
      // event_callback receives updates on cablev2 events.
      std::optional<base::RepeatingCallback<void(Event)>> event_callback,
      bool must_support_ctap);
  ~Discovery() override;
  Discovery(const Discovery&) = delete;
  Discovery& operator=(const Discovery&) = delete;

  // BluetoothAdapter::Observer:
  void DeviceAdded(BluetoothAdapter* adapter, BluetoothDevice* device) override;
  void DeviceChanged(BluetoothAdapter* adapter,
                     BluetoothDevice* device) override;
  void AdapterPoweredChanged(BluetoothAdapter* adapter, bool powered) override;

#if BUILDFLAG(IS_CHROMEOS)
  // device::BluetoothLowEnergyScanSession::Delegate:
  void OnDeviceFound(device::BluetoothLowEnergyScanSession* scan_session,
                     device::BluetoothDevice* device) override;
  void OnDeviceLost(device::BluetoothLowEnergyScanSession* scan_session,
                    device::BluetoothDevice* device) override;
  void OnSessionStarted(
      device::BluetoothLowEnergyScanSession* scan_session,
      std::optional<device::BluetoothLowEnergyScanSession::ErrorCode>
          error_code) override;
  void OnSessionInvalidated(
      device::BluetoothLowEnergyScanSession* scan_session) override;
#endif  // BUILDFLAG(IS_CHROMEOS)

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
  static std::optional<UnpairedKeys> KeysFromQRGeneratorKey(
      std::optional<base::span<const uint8_t, kQRKeySize>> qr_generator_key);
  static std::vector<UnpairedKeys> KeysFromExtension(
      const std::vector<CableDiscoveryData>& extension_contents);

  static const BluetoothUUID& GoogleCableUUID();
  static const BluetoothUUID& FIDOCableUUID();
  static bool IsCableDevice(const BluetoothDevice* device);

  static std::optional<CableEidArray> MaybeGetEidFromServiceData(
      const BluetoothDevice* device);
  static std::vector<CableEidArray> GetUUIDs(const BluetoothDevice* device);

  void StartCableDiscovery();
  void OnStartDiscoverySession(std::unique_ptr<BluetoothDiscoverySession>);
  void OnStartDiscoverySessionError();

  void OnGetAdapter(scoped_refptr<BluetoothAdapter> adapter);
  void OnSetPowered();

  void SetDiscoverySession(
      std::unique_ptr<BluetoothDiscoverySession> discovery_session);

  void GetDiscoveryData(const BluetoothDevice* device);

  BluetoothAdapter* adapter() { return adapter_.get(); }

  scoped_refptr<BluetoothAdapter> adapter_;
  std::unique_ptr<BluetoothDiscoverySession> discovery_session_;
#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<device::BluetoothLowEnergyScanSession> le_scan_session_;
#endif  // BUILDFLAG(IS_CHROMEOS)

  const FidoRequestType request_type_;
  NetworkContextFactory network_context_factory_;
  const std::optional<UnpairedKeys> qr_keys_;
  const std::vector<UnpairedKeys> extension_keys_;
  std::unique_ptr<EventStream<std::unique_ptr<Pairing>>> contact_device_stream_;
  const std::optional<base::RepeatingCallback<void(std::unique_ptr<Pairing>)>>
      pairing_callback_;
  const std::optional<base::RepeatingCallback<void(std::unique_ptr<Pairing>)>>
      invalidated_pairing_callback_;
  const std::optional<base::RepeatingCallback<void(Event)>> event_callback_;
  const bool must_support_ctap_;
  std::vector<std::unique_ptr<FidoTunnelDevice>> tunnels_pending_advert_;
  base::flat_set<std::array<uint8_t, kAdvertSize>> observed_adverts_;
  bool started_ = false;
  bool device_committed_ = false;

  base::WeakPtrFactory<Discovery> weak_factory_{this};
};

}  // namespace device::cablev2

#endif  // DEVICE_FIDO_CABLE_V2_DISCOVERY_H_
