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
#include "device/fido/ble/fido_ble_discovery_base.h"
#include "device/fido/cable/cable_discovery_data.h"

namespace device {

class FidoCableDevice;
class BluetoothDevice;
class BluetoothAdvertisement;
class FidoCableHandshakeHandler;

class COMPONENT_EXPORT(DEVICE_FIDO) FidoCableDiscovery
    : public FidoBleDiscoveryBase {
 public:
  FidoCableDiscovery(
      std::vector<CableDiscoveryData> discovery_data,
      base::Optional<QRGeneratorKey> qr_generator_key,
      base::Optional<
          base::RepeatingCallback<void(std::unique_ptr<CableDiscoveryData>)>>
          pairing_callback);
  ~FidoCableDiscovery() override;

 protected:
  virtual base::Optional<std::unique_ptr<FidoCableHandshakeHandler>>
  CreateHandshakeHandler(FidoCableDevice* device,
                         const CableDiscoveryData& discovery_data,
                         const CableNonce& nonce,
                         const CableEidArray& eid);

 private:
  // Result represents a successful match of a received EID against a specific
  // |FidoDiscoveryData|.
  struct Result {
    Result();
    Result(const CableDiscoveryData& in_discovery_data,
           const CableNonce& in_nonce,
           const CableEidArray& in_eid,
           base::Optional<int> ticks_back);
    Result(const Result&);
    ~Result();

    CableDiscoveryData discovery_data;
    CableNonce nonce;
    CableEidArray eid;
    // ticks_back is either |base::nullopt|, if the Result is from established
    // discovery pairings, or else contains the number of QR ticks back in time
    // against which the match was found.
    base::Optional<int> ticks_back;
  };

  // ObservedDeviceData contains potential EIDs observed from a BLE device. This
  // information is kept in order to de-duplicate device-log entries and make
  // debugging easier.
  struct ObservedDeviceData {
    ObservedDeviceData();
    ~ObservedDeviceData();

    base::Optional<CableEidArray> service_data;
    std::vector<CableEidArray> uuids;
  };

  FRIEND_TEST_ALL_PREFIXES(FidoCableDiscoveryTest,
                           TestDiscoveryWithAdvertisementFailures);
  FRIEND_TEST_ALL_PREFIXES(FidoCableDiscoveryTest,
                           TestUnregisterAdvertisementUponDestruction);

  // BluetoothAdapter::Observer:
  void DeviceAdded(BluetoothAdapter* adapter, BluetoothDevice* device) override;
  void DeviceChanged(BluetoothAdapter* adapter,
                     BluetoothDevice* device) override;
  void DeviceRemoved(BluetoothAdapter* adapter,
                     BluetoothDevice* device) override;
  void AdapterPoweredChanged(BluetoothAdapter* adapter, bool powered) override;

  // FidoBleDiscoveryBase:
  void OnSetPowered() override;
  void OnStartDiscoverySessionWithFilter(
      std::unique_ptr<BluetoothDiscoverySession>) override;

  void StartCableDiscovery();
  void StartAdvertisement();
  void OnAdvertisementRegistered(
      const CableEidArray& client_eid,
      scoped_refptr<BluetoothAdvertisement> advertisement);
  void OnAdvertisementRegisterError(
      BluetoothAdvertisement::ErrorCode error_code);
  // Keeps a counter of success/failure of advertisements done by the client.
  // If all advertisements fail, then immediately stop discovery process and
  // invoke NotifyDiscoveryStarted(false). Otherwise kick off discovery session
  // once all advertisements has been processed.
  void RecordAdvertisementResult(bool is_success);
  // Attempt to stop all on-going advertisements in best-effort basis.
  // Once all the callbacks for Unregister() function is received, invoke
  // |callback|.
  void StopAdvertisements(base::OnceClosure callback);
  void CableDeviceFound(BluetoothAdapter* adapter, BluetoothDevice* device);
  void ConductEncryptionHandshake(std::unique_ptr<FidoCableDevice> cable_device,
                                  Result discovery_data);
  void ValidateAuthenticatorHandshakeMessage(
      std::unique_ptr<FidoCableDevice> cable_device,
      FidoCableHandshakeHandler* handshake_handler,
      base::Optional<std::vector<uint8_t>> handshake_response);

  base::Optional<Result> GetCableDiscoveryData(
      const BluetoothDevice* device) const;
  static base::Optional<CableEidArray> MaybeGetEidFromServiceData(
      const BluetoothDevice* device);
  static std::vector<CableEidArray> GetUUIDs(const BluetoothDevice* device);
  base::Optional<Result> GetCableDiscoveryDataFromAuthenticatorEid(
      CableEidArray authenticator_eid) const;
  // ResultDebugString returns a containing a hex dump of |eid| and a
  // description of |result|, if present.
  static std::string ResultDebugString(const CableEidArray& eid,
                                       const base::Optional<Result>& result);

  std::vector<CableDiscoveryData> discovery_data_;
  // active_authenticator_eids_ contains authenticator EIDs for which a
  // handshake is currently running. Further advertisements for the same EIDs
  // will be ignored.
  std::set<CableEidArray> active_authenticator_eids_;
  // active_devices_ contains the BLE addresses of devices for which a handshake
  // is already running. Further advertisements from these devices will be
  // ignored. However, devices may rotate their BLE address at will so this is
  // not completely effective.
  std::set<std::string> active_devices_;
  base::Optional<QRGeneratorKey> qr_generator_key_;
  size_t advertisement_success_counter_ = 0;
  size_t advertisement_failure_counter_ = 0;
  std::map<CableEidArray, scoped_refptr<BluetoothAdvertisement>>
      advertisements_;
  std::vector<std::unique_ptr<FidoCableHandshakeHandler>>
      cable_handshake_handlers_;
  base::Optional<
      base::RepeatingCallback<void(std::unique_ptr<CableDiscoveryData>)>>
      pairing_callback_;

  // observed_devices_ caches the information from observed caBLE devices so
  // that the device-log isn't spammed.
  mutable base::flat_map<std::string, std::unique_ptr<ObservedDeviceData>>
      observed_devices_;
  // noted_obsolete_eids_ remembers QR-code EIDs that have been logged as
  // valid-but-expired in order to avoid spamming the device-log.
  mutable base::flat_set<CableEidArray> noted_obsolete_eids_;

  base::WeakPtrFactory<FidoCableDiscovery> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FidoCableDiscovery);
};

}  // namespace device

#endif  // DEVICE_FIDO_CABLE_FIDO_CABLE_DISCOVERY_H_
