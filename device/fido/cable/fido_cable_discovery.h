// Copyright 2018 The Chromium Authors. All rights reserved.
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
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/cable/fido_cable_device.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/fido_device_discovery.h"

namespace device {

class BluetoothDevice;
class BluetoothAdvertisement;
class FidoCableHandshakeHandler;

class COMPONENT_EXPORT(DEVICE_FIDO) FidoCableDiscovery
    : public FidoDeviceDiscovery,
      public BluetoothAdapter::Observer,
      public FidoCableDevice::Observer {
 public:
  FidoCableDiscovery(std::vector<CableDiscoveryData> discovery_data,
                     FidoDeviceDiscovery::BLEObserver* ble_observer);
  ~FidoCableDiscovery() override;

  // FidoDeviceDiscovery:
  bool MaybeStop() override;

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
  enum class CableV1DiscoveryEvent : int;

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

    base::Optional<CableEidArray> service_data;
    std::vector<CableEidArray> uuids;
  };

  static const BluetoothUUID& CableAdvertisementUUID();
  static bool IsCableDevice(const BluetoothDevice* device);

  // ResultDebugString returns a string containing a hex dump of |eid| and a
  // description of |result|, if present.
  static std::string ResultDebugString(
      const CableEidArray& eid,
      const base::Optional<V1DiscoveryDataAndEID>& result);
  static base::Optional<CableEidArray> MaybeGetEidFromServiceData(
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
      base::Optional<std::vector<uint8_t>> handshake_response);

  base::Optional<V1DiscoveryDataAndEID> GetCableDiscoveryData(
      const BluetoothDevice* device);
  base::Optional<V1DiscoveryDataAndEID>
  GetCableDiscoveryDataFromAuthenticatorEid(CableEidArray authenticator_eid);
  void RecordCableV1DiscoveryEventOnce(CableV1DiscoveryEvent event);

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

  // FidoCableDevice::Observer:
  void FidoCableDeviceConnected(FidoCableDevice* device, bool success) override;
  void FidoCableDeviceTimeout(FidoCableDevice* device) override;

  scoped_refptr<BluetoothAdapter> adapter_;
  std::unique_ptr<BluetoothDiscoverySession> discovery_session_;

  std::vector<CableDiscoveryData> discovery_data_;
  FidoDeviceDiscovery::BLEObserver* const ble_observer_;

  // active_authenticator_eids_ contains authenticator EIDs for which a
  // handshake is currently running. Further advertisements for the same EIDs
  // will be ignored.
  std::set<CableEidArray> active_authenticator_eids_;

  // active_devices_ contains the BLE addresses of devices for which a
  // handshake is already running. Further advertisements from these devices
  // will be ignored. However, devices may rotate their BLE address at will so
  // this is not completely effective.
  std::set<std::string> active_devices_;
  base::Optional<std::array<uint8_t, cablev2::kQRKeySize>> qr_generator_key_;

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
  base::flat_set<CableV1DiscoveryEvent> recorded_events_;

  base::WeakPtrFactory<FidoCableDiscovery> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FidoCableDiscovery);
};

}  // namespace device

#endif  // DEVICE_FIDO_CABLE_FIDO_CABLE_DISCOVERY_H_
