// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_FIDO_CABLE_DISCOVERY_H_
#define DEVICE_FIDO_CABLE_FIDO_CABLE_DISCOVERY_H_

#include <stdint.h>

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/fido_device_discovery.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "device/bluetooth/bluetooth_low_energy_scan_session.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace device {

class BluetoothDevice;
class BluetoothAdvertisement;
class FidoCableDevice;
class FidoCableHandshakeHandler;

class COMPONENT_EXPORT(DEVICE_FIDO) FidoCableDiscovery
    : public FidoDeviceDiscovery,
#if BUILDFLAG(IS_CHROMEOS)
      public device::BluetoothLowEnergyScanSession::Delegate,
#endif  // BUILDFLAG(IS_CHROMEOS)
      public BluetoothAdapter::Observer {
 public:
  explicit FidoCableDiscovery(std::vector<CableDiscoveryData> discovery_data);

  FidoCableDiscovery(const FidoCableDiscovery&) = delete;
  FidoCableDiscovery& operator=(const FidoCableDiscovery&) = delete;

  ~FidoCableDiscovery() override;

  // FidoDeviceDiscovery:
  void Stop() override;

  // GetV2AdvertStream returns a stream of caBLEv2 BLE adverts. Only a single
  // stream is supported.
  std::unique_ptr<EventStream<base::span<const uint8_t, cablev2::kAdvertSize>>>
  GetV2AdvertStream();

  const std::map<CableEidArray, scoped_refptr<BluetoothAdvertisement>>&
  AdvertisementsForTesting() const {
    return advertisements_;
  }

 protected:
  virtual std::unique_ptr<FidoCableHandshakeHandler> CreateV1HandshakeHandler(
      FidoCableDevice* device,
      const CableDiscoveryData& discovery_data,
      const CableEidArray& authenticator_eid);

 private:
  // V1DiscoveryDataAndEID represents a match against caBLEv1 pairing data. It
  // contains the CableDiscoveryData that matched and the BLE EID that triggered
  // the match.
  using V1DiscoveryDataAndEID = std::pair<CableDiscoveryData, CableEidArray>;

  // ObservedDeviceData contains potential EIDs observed from a BLE device. This
  // information is kept in order to de-duplicate device-log entries and make
  // debugging easier.
  struct ObservedDeviceData {
    ObservedDeviceData();
    ~ObservedDeviceData();

    std::optional<CableEidArray> service_data;
    std::vector<CableEidArray> uuids;
  };

  static const BluetoothUUID& GoogleCableUUID();
  static const BluetoothUUID& FIDOCableUUID();
  static bool IsCableDevice(const BluetoothDevice* device);

  // ResultDebugString returns a string containing a hex dump of |eid| and a
  // description of |result|, if present.
  static std::string ResultDebugString(
      const CableEidArray& eid,
      const std::optional<V1DiscoveryDataAndEID>& result);
  static std::optional<CableEidArray> MaybeGetEidFromServiceData(
      const BluetoothDevice* device);
  static std::vector<CableEidArray> GetUUIDs(const BluetoothDevice* device);

  void StartCableDiscovery();
  void OnStartDiscoverySession(std::unique_ptr<BluetoothDiscoverySession>);
  void OnStartDiscoverySessionError();
  void StartAdvertisement();
  void OnAdvertisementRegistered(
      const CableEidArray& client_eid,
      scoped_refptr<BluetoothAdvertisement> advertisement);

  void OnGetAdapter(scoped_refptr<BluetoothAdapter> adapter);
  void OnSetPowered();

  void SetDiscoverySession(
      std::unique_ptr<BluetoothDiscoverySession> discovery_session);

  BluetoothAdapter* adapter() { return adapter_.get(); }

  // Attempt to stop all on-going advertisements in best-effort basis.
  // Once all the callbacks for Unregister() function is received, invoke
  // |callback|.
  void StopAdvertisements(base::OnceClosure callback);
  void OnAdvertisementsStopped(base::OnceClosure callback);
  void CableDeviceFound(BluetoothAdapter* adapter, BluetoothDevice* device);
  void ConductEncryptionHandshake(FidoCableHandshakeHandler* handshake_handler,
                                  CableDiscoveryData::Version cable_version);
  void ValidateAuthenticatorHandshakeMessage(
      CableDiscoveryData::Version cable_version,
      FidoCableHandshakeHandler* handshake_handler,
      std::optional<std::vector<uint8_t>> handshake_response);

  std::optional<V1DiscoveryDataAndEID> GetCableDiscoveryData(
      const BluetoothDevice* device);
  std::optional<V1DiscoveryDataAndEID>
  GetCableDiscoveryDataFromAuthenticatorEid(CableEidArray authenticator_eid);

  // FidoDeviceDiscovery:
  void StartInternal() override;

  // BluetoothAdapter::Observer:
  void DeviceAdded(BluetoothAdapter* adapter, BluetoothDevice* device) override;
  void DeviceChanged(BluetoothAdapter* adapter,
                     BluetoothDevice* device) override;
  void DeviceRemoved(BluetoothAdapter* adapter,
                     BluetoothDevice* device) override;
  void AdapterPoweredChanged(BluetoothAdapter* adapter, bool powered) override;
  void AdapterDiscoveringChanged(BluetoothAdapter* adapter,
                                 bool discovering) override;

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

  scoped_refptr<BluetoothAdapter> adapter_;
  std::unique_ptr<BluetoothDiscoverySession> discovery_session_;
#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<device::BluetoothLowEnergyScanSession> le_scan_session_;
#endif  // BUILDFLAG(IS_CHROMEOS)

  std::vector<CableDiscoveryData> discovery_data_;
  base::RepeatingCallback<void(base::span<const uint8_t, cablev2::kAdvertSize>)>
      advert_callback_;

  // active_authenticator_eids_ contains authenticator EIDs for which a
  // handshake is currently running. Further advertisements for the same EIDs
  // will be ignored.
  std::set<CableEidArray> active_authenticator_eids_;

  // active_devices_ contains the BLE addresses of devices for which a
  // handshake is already running. Further advertisements from these devices
  // will be ignored. However, devices may rotate their BLE address at will so
  // this is not completely effective.
  std::set<std::string> active_devices_;

  // Note that on Windows, |advertisements_| is the only reference holder of
  // BluetoothAdvertisement.
  std::map<CableEidArray, scoped_refptr<BluetoothAdvertisement>>
      advertisements_;

  std::vector<std::pair<std::unique_ptr<FidoCableDevice>,
                        std::unique_ptr<FidoCableHandshakeHandler>>>
      active_handshakes_;

  // observed_devices_ caches the information from observed caBLE devices so
  // that the device-log isn't spammed.
  base::flat_map<std::string, std::unique_ptr<ObservedDeviceData>>
      observed_devices_;

  bool has_v1_discovery_data_ = false;

  base::WeakPtrFactory<FidoCableDiscovery> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_FIDO_CABLE_FIDO_CABLE_DISCOVERY_H_
