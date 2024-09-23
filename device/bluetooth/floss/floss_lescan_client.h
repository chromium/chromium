// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_FLOSS_FLOSS_LESCAN_CLIENT_H_
#define DEVICE_BLUETOOTH_FLOSS_FLOSS_LESCAN_CLIENT_H_

#include <memory>
#include <string>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "dbus/exported_object.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/floss/exported_callback_manager.h"
#include "device/bluetooth/floss/floss_dbus_client.h"
#include "device/bluetooth/floss/floss_gatt_manager_client.h"

namespace dbus {
class ObjectPath;
}  // namespace dbus

namespace floss {

const char kScannerCallbackPath[] =
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    "/org/chromium/bluetooth/scanner/callback/lacros";
#else
    "/org/chromium/bluetooth/scanner/callback";
#endif
const char kScannerCallbackInterfaceName[] =
    "org.chromium.bluetooth.ScannerCallback";
const char kEmptyUuidStr[] = "00000000-0000-0000-0000-000000000000";

// Represents type of a scan.
enum class ScanType {
  kActive = 0,
  kPassive = 1,
};

// Represents scanning configurations.
struct ScanSettings {
  int32_t interval;
  int32_t window;
  ScanType scan_type;
};

struct DEVICE_BLUETOOTH_EXPORT ScanFilterPattern {
  // Specifies the starting byte position of the pattern immediately following
  // AD Type.
  uint8_t start_position = 0;

  // Advertising Data type (https://www.bluetooth.com/specifications/assigned-numbers/).
  uint8_t ad_type = 0;

  // The pattern to be matched for the specified AD Type within the
  // advertisement packet from the specified starting byte.
  std::vector<uint8_t> content;

  ScanFilterPattern();
  ScanFilterPattern(const ScanFilterPattern&);
  ~ScanFilterPattern();
};

struct DEVICE_BLUETOOTH_EXPORT ScanFilterCondition {
  // Match by pattern anywhere in the advertisement data. Multiple patterns are
  // "OR"-ed.
  std::vector<ScanFilterPattern> patterns;

  ScanFilterCondition();
  ScanFilterCondition(const ScanFilterCondition&);
  ~ScanFilterCondition();
};

// Modeled based on MSFT HCI extension spec:
// https://learn.microsoft.com/en-us/windows-hardware/drivers/bluetooth/microsoft-defined-bluetooth-hci-commands-and-events#command_parameters-1
struct DEVICE_BLUETOOTH_EXPORT ScanFilter {
  // Advertisements with RSSI above or equal this value is considered "found".
  uint8_t rssi_high_threshold = 0;

  // Advertisements with RSSI below or equal this value (for a period of
  // rssi_low_timeout) is considered "lost".
  uint8_t rssi_low_threshold = 0;

  // Time in seconds over which the RSSI value should be below
  // rssi_low_threshold before being considered "lost".
  uint8_t rssi_low_timeout = 0;

  // Sampling interval in 100 milliseconds.
  // i.e. The real sampling period in ms = rssi_sampling_period * 100.
  uint8_t rssi_sampling_period = 0;

  //The condition to match advertisements with.
  ScanFilterCondition condition;

  ScanFilter();
  ScanFilter(const ScanFilter&);
  ~ScanFilter();
};

struct DEVICE_BLUETOOTH_EXPORT ScanResult {
  std::string name;
  std::string address;
  uint8_t addr_type = 0;
  uint16_t event_type = 0;
  uint8_t primary_phy = 0;
  uint8_t secondary_phy = 0;
  uint8_t advertising_sid = 0;
  int8_t tx_power = 0;
  int8_t rssi = 0;
  uint16_t periodic_adv_int = 0;
  uint8_t flags = 0;
  std::vector<device::BluetoothUUID> service_uuids;
  std::map<std::string, std::vector<uint8_t>> service_data;
  std::map<uint16_t, std::vector<uint8_t>> manufacturer_data;
  std::vector<uint8_t> adv_data;

  ScanResult();
  ScanResult(const ScanResult&);
  ~ScanResult();
};

class ScannerClientObserver : public base::CheckedObserver {
 public:
  ScannerClientObserver() = default;
  ~ScannerClientObserver() override = default;

  // A scanner has been registered
  virtual void ScannerRegistered(device::BluetoothUUID uuid,
                                 uint8_t scanner_id,
                                 GattStatus status) {}

  // A scan result has been received
  virtual void ScanResultReceived(ScanResult scan_result) {}

  // An advertisement has been found
  virtual void AdvertisementFound(uint8_t scanner_id, ScanResult scan_result) {}

  // A scan result has been lost
  virtual void AdvertisementLost(uint8_t scanner_id, ScanResult scan_result) {}
};

// Low-level interface to Floss's LE Scan API.
class DEVICE_BLUETOOTH_EXPORT FlossLEScanClient : public FlossDBusClient,
                                                  public ScannerClientObserver {
 public:
  // Error: No such adapter.
  static const char kErrorUnknownAdapter[];

  // Creates the instance.
  static std::unique_ptr<FlossLEScanClient> Create();

  FlossLEScanClient(const FlossLEScanClient&) = delete;
  FlossLEScanClient& operator=(const FlossLEScanClient&) = delete;

  FlossLEScanClient();
  ~FlossLEScanClient() override;

  // Manage observers.
  void AddObserver(ScannerClientObserver* observer);
  void RemoveObserver(ScannerClientObserver* observer);

  // Initialize the LE Scan client.
  void Init(dbus::Bus* bus,
            const std::string& service_name,
            const int adapter_index,
            base::Version version,
            base::OnceClosure on_ready) override;

  virtual void RegisterScanner(
      ResponseCallback<device::BluetoothUUID> callback);
  virtual void UnregisterScanner(ResponseCallback<bool> callback,
                                 uint8_t scanner_id);
  virtual void StartScan(ResponseCallback<BtifStatus> callback,
                         uint8_t scanner_id,
                         const std::optional<ScanSettings>& scan_settings,
                         const std::optional<ScanFilter>& filter);
  virtual void StopScan(ResponseCallback<BtifStatus> callback,
                        uint8_t scanner_id);

 protected:
  // ScannerClientObserver overrides
  void ScannerRegistered(device::BluetoothUUID uuid,
                         uint8_t scanner_id,
                         GattStatus status) override;
  void ScanResultReceived(ScanResult scan_result) override;
  void AdvertisementFound(uint8_t scanner_id, ScanResult scan_result) override;
  void AdvertisementLost(uint8_t scanner_id, ScanResult scan_result) override;

  // Managed by FlossDBusManager - we keep local pointer to access object proxy.
  raw_ptr<dbus::Bus> bus_ = nullptr;

  // Adapter managed by this client.
  dbus::ObjectPath object_path_;

  // List of observers interested in event notifications from this client.
  base::ObserverList<ScannerClientObserver> observers_;

  // Service which implements the adapter interface.
  std::string service_name_;

 private:
  std::optional<uint32_t> le_scan_callback_id_;

  ExportedCallbackManager<ScannerClientObserver>
      exported_scanner_callback_manager_{kScannerCallbackInterfaceName};

  // List of callbacks to register a scanner for once |RegisterScannerCallback|
  // completes.
  base::queue<ResponseCallback<device::BluetoothUUID>>
      pending_register_scanners_;

  void RegisterScannerCallback();

  void OnRegisterScannerCallback(DBusResult<uint32_t> ret);

  void OnUnregisterScannerCallback(DBusResult<bool> ret);

  template <typename R, typename... Args>
  void CallLEScanMethod(ResponseCallback<R> callback,
                        const char* member,
                        Args... args) {
    CallMethod(std::move(callback), bus_, service_name_, kGattInterface,
               object_path_, member, args...);
  }

  // Signal that the client is ready to be used.
  base::OnceClosure on_ready_;

  base::WeakPtrFactory<FlossLEScanClient> weak_ptr_factory_{this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FLOSS_LESCAN_CLIENT_H_
