// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_FLOSS_FLOSS_ADVERTISER_CLIENT_H_
#define DEVICE_BLUETOOTH_FLOSS_FLOSS_ADVERTISER_CLIENT_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "device/bluetooth/floss/exported_callback_manager.h"
#include "device/bluetooth/floss/floss_dbus_client.h"
#include "device/bluetooth/floss/floss_gatt_manager_client.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace floss {

constexpr char kAdvertisingSetCallbackPath[] =
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    "/org/chromium/bluetooth/advertising_set/callback/lacros";
#else
    "/org/chromium/bluetooth/advertising_set/callback";
#endif

// Represents type of address to advertise.
enum class OwnAddressType {
  kDefault = -1,
  kPublic = 0,
  kRandom = 2,
};

// Represents the parameters of an advertising set. Supports Floss API version
// 0.4.0 or earlier.
struct AdvertisingSetParametersOld {
  bool connectable;
  bool scannable;
  bool is_legacy;
  bool is_anonymous;
  bool include_tx_power;
  LePhy primary_phy;
  LePhy secondary_phy;
  int32_t interval;        // Advertising interval in 0.625 ms unit.
  int32_t tx_power_level;  // Transmission power of advertising in dBm.
  OwnAddressType own_address_type;
};

// Represents the parameters of an advertising set. Supports Floss API versions
// greater than 0.4.0.
struct AdvertisingSetParameters {
  LeDiscoverableMode discoverable;
  bool connectable;
  bool scannable;
  bool is_legacy;
  bool is_anonymous;
  bool include_tx_power;
  LePhy primary_phy;
  LePhy secondary_phy;
  int32_t interval;        // Advertising interval in 0.625 ms unit.
  int32_t tx_power_level;  // Transmission power of advertising in dBm.
  OwnAddressType own_address_type;
};

// Represents the data to be advertised.
struct DEVICE_BLUETOOTH_EXPORT AdvertiseData {
  std::vector<device::BluetoothUUID> service_uuids;
  std::vector<device::BluetoothUUID> solicit_uuids;
  std::vector<std::vector<uint8_t>> transport_discovery_data;
  std::map<uint16_t, std::vector<uint8_t>> manufacturer_data;
  std::map<std::string, std::vector<uint8_t>> service_data;
  bool include_tx_power_level;
  bool include_device_name;

  AdvertiseData();
  AdvertiseData(const AdvertiseData&);
  ~AdvertiseData();
};

// Represents parameters of the periodic advertising packet.
struct PeriodicAdvertisingParameters {
  bool include_tx_power_level;
  int32_t interval;  // Periodic advertising interval in 1.25 ms unit.
};

// Status of advertising callbacks.
enum class AdvertisingStatus {
  kSuccess = 0,
  kDataTooLarge,
  kTooManyAdvertiser,
  kAlreadyStarted,
  kInternalError,
  kFeatureUnsupported,
};

class FlossAdvertiserClientObserver : public base::CheckedObserver {
 public:
  // Id given after register callbacks.
  using CallbackId = uint32_t;

  // Id for an advertising set registered.
  using RegId = int32_t;

  // Id for an advertising set started.
  using AdvertiserId = int32_t;

  FlossAdvertiserClientObserver(const FlossAdvertiserClientObserver&) = delete;
  FlossAdvertiserClientObserver& operator=(
      const FlossAdvertiserClientObserver&) = delete;

  FlossAdvertiserClientObserver() = default;
  ~FlossAdvertiserClientObserver() override = default;

  // Callback of the D-Bus method StartAdvertisingSet.
  virtual void OnAdvertisingSetStarted(RegId reg_id,
                                       AdvertiserId adv_id,
                                       int32_t tx_power,
                                       AdvertisingStatus status) {}

  // Callback of the D-Bus method GetOwnAddress.
  virtual void OnOwnAddressRead(AdvertiserId adv_id,
                                int32_t address_type,
                                std::string address) {}

  // Callback of the D-Bus method StopAdvertisingSet.
  virtual void OnAdvertisingSetStopped(AdvertiserId adv_id) {}

  // Callback of the D-Bus method EnableAdvertising or state changed.
  virtual void OnAdvertisingEnabled(AdvertiserId adv_id,
                                    bool enable,
                                    AdvertisingStatus status) {}

  // Callback of the D-Bus method SetAdvertisingData.
  virtual void OnAdvertisingDataSet(AdvertiserId adv_id,
                                    AdvertisingStatus status) {}

  // Callback of the D-Bus method SetScanResponseData.
  virtual void OnScanResponseDataSet(AdvertiserId adv_id,
                                     AdvertisingStatus status) {}

  // Callback of the D-Bus method SetAdvertisingParameters.
  virtual void OnAdvertisingParametersUpdated(AdvertiserId adv_id,
                                              int32_t tx_power,
                                              AdvertisingStatus status) {}

  // Callback of the D-Bus method SetPeriodicAdvertisingParameters.
  virtual void OnPeriodicAdvertisingParametersUpdated(
      AdvertiserId adv_id,
      AdvertisingStatus status) {}

  // Callback of the D-Bus method SetPeriodicAdvertisingData.
  virtual void OnPeriodicAdvertisingDataSet(AdvertiserId adv_id,
                                            AdvertisingStatus status) {}

  // Callback of the D-Bus method SetPeriodicAdvertisingEnable.
  virtual void OnPeriodicAdvertisingEnabled(AdvertiserId adv_id,
                                            bool enable,
                                            AdvertisingStatus status) {}
};

// FlossAdvertiserClient is a D-Bus client that talks to Floss daemon to
// perform BLE advertise operations, such as BLE advertising sets creation,
// parameters and data configuration, information query.
// It is managed by FlossClientBundle and will be initialized with an adapter
// when one is powered on.
class DEVICE_BLUETOOTH_EXPORT FlossAdvertiserClient
    : public FlossDBusClient,
      public FlossAdvertiserClientObserver {
 public:
  // Valid callback ids are always greater than 0.
  static const CallbackId kInvalidCallbackId = 0;

  // Creates the instance.
  static std::unique_ptr<FlossAdvertiserClient> Create();

  FlossAdvertiserClient(const FlossAdvertiserClient&) = delete;
  FlossAdvertiserClient& operator=(const FlossAdvertiserClient&) = delete;

  FlossAdvertiserClient();
  ~FlossAdvertiserClient() override;

  // Initializes the advertising manager with given adapter.
  void Init(dbus::Bus* bus,
            const std::string& service_name,
            const int adapter_index,
            base::Version version,
            base::OnceClosure on_ready) override;

  // Manages observers.
  void AddObserver(FlossAdvertiserClientObserver* observer);
  void RemoveObserver(FlossAdvertiserClientObserver* observer);

  using ErrorCallback = device::BluetoothAdvertisement::ErrorCallback;
  using StartSuccessCallback = base::OnceCallback<void(AdvertiserId)>;
  using StopSuccessCallback = base::OnceClosure;
  using SetAdvParamsSuccessCallback = base::OnceClosure;

  // Calls the D-Bus method StartAdvertisingSet.
  virtual void StartAdvertisingSet(
      const AdvertisingSetParameters& params,
      const AdvertiseData& adv_data,
      const std::optional<AdvertiseData> scan_rsp,
      const std::optional<PeriodicAdvertisingParameters> periodic_params,
      const std::optional<AdvertiseData> periodic_data,
      const int32_t duration,
      const int32_t max_ext_adv_events,
      StartSuccessCallback success_callback,
      ErrorCallback error_callback);

  // Calls the D-Bus method StopAdvertisingSet.
  virtual void StopAdvertisingSet(const AdvertiserId adv_id,
                                  StopSuccessCallback success_callback,
                                  ErrorCallback error_callback);

  // Calls the D-Bus method SetAdvertisingParameters.
  virtual void SetAdvertisingParameters(
      const AdvertiserId adv_id,
      const AdvertisingSetParameters& params,
      SetAdvParamsSuccessCallback success_callback,
      ErrorCallback error_callback);

 protected:
  // Registers callback to daemon after all callback methods are exported.
  void OnMethodsExported();

  // Completes the method call for RegisterAdvertiserCallback.
  void CompleteRegisterCallback(dbus::Response* response,
                                dbus::ErrorResponse* error_response);

  // Completes the method call for UnregisterAdvertiserCallback.
  void CompleteUnregisterCallback(DBusResult<bool> ret);

  // Completes the method call for |StartAdvertisingSet|.
  void CompleteStartAdvertisingSetCallback(
      StartSuccessCallback success_callback,
      ErrorCallback error_callback,
      DBusResult<RegId> ret);

  // Completes the method call for |StopAdvertisingSet|.
  void CompleteStopAdvertisingSetCallback(StopSuccessCallback success_callback,
                                          ErrorCallback error_callback,
                                          const AdvertiserId adv_id,
                                          DBusResult<Void> ret);

  // Completes the method call for |SetAdvertisingParameters|.
  void CompleteSetAdvertisingParametersCallback(
      SetAdvParamsSuccessCallback success_callback,
      ErrorCallback error_callback,
      const AdvertiserId adv_id,
      DBusResult<Void> ret);

  // FlossAdvertiserClientObserver overrides.
  void OnAdvertisingSetStarted(RegId reg_id,
                               AdvertiserId adv_id,
                               int32_t tx_power,
                               AdvertisingStatus status) override;
  void OnOwnAddressRead(AdvertiserId adv_id,
                        int32_t address_type,
                        std::string address) override;
  void OnAdvertisingSetStopped(AdvertiserId adv_id) override;
  void OnAdvertisingEnabled(AdvertiserId adv_id,
                            bool enable,
                            AdvertisingStatus status) override;
  void OnAdvertisingDataSet(AdvertiserId adv_id,
                            AdvertisingStatus status) override;
  void OnScanResponseDataSet(AdvertiserId adv_id,
                             AdvertisingStatus status) override;
  void OnAdvertisingParametersUpdated(AdvertiserId adv_id,
                                      int32_t tx_power,
                                      AdvertisingStatus status) override;
  void OnPeriodicAdvertisingParametersUpdated(
      AdvertiserId adv_id,
      AdvertisingStatus status) override;
  void OnPeriodicAdvertisingDataSet(AdvertiserId adv_id,
                                    AdvertisingStatus status) override;
  void OnPeriodicAdvertisingEnabled(AdvertiserId adv_id,
                                    bool enable,
                                    AdvertisingStatus status) override;

  // Converts advertising status to error code.
  device::BluetoothAdvertisement::ErrorCode GetErrorCode(
      AdvertisingStatus status);

  // Managed by FlossDBusManager - we keep local pointer to access object proxy.
  raw_ptr<dbus::Bus> bus_ = nullptr;

  // Path used for gatt api calls by this class.
  dbus::ObjectPath gatt_adapter_path_;

  // List of observers interested in event notifications from this client.
  base::ObserverList<FlossAdvertiserClientObserver> observers_;

  // Service which implements the BluetoothGatt interface.
  std::string service_name_;

 private:
  template <typename R, typename... Args>
  void CallAdvertisingMethod(ResponseCallback<R> callback,
                             const char* member,
                             Args... args) {
    CallMethod(std::move(callback), bus_, service_name_, kGattInterface,
               gatt_adapter_path_, member, args...);
  }

  // Exported callbacks for interacting with daemon.
  ExportedCallbackManager<FlossAdvertiserClientObserver>
      exported_callback_manager_{advertiser::kCallbackInterface};

  // Id of the callbacks registered for BLE advertising. A value of zero is
  // invalid.
  CallbackId callback_id_ = 0;

  // Keeps callbacks for |StartAdvertisingSet|.
  std::unordered_map<RegId, std::pair<StartSuccessCallback, ErrorCallback>>
      start_advertising_set_callbacks_;

  // Keeps callbacks for |StopAdvertisingSet|.
  std::unordered_map<AdvertiserId,
                     std::pair<StopSuccessCallback, ErrorCallback>>
      stop_advertising_set_callbacks_;

  // Keeps callbacks for |SetAdvertisingParameters|.
  std::unordered_map<AdvertiserId,
                     std::pair<SetAdvParamsSuccessCallback, ErrorCallback>>
      set_advertising_params_callbacks_;

  // Signal when client is ready to be used.
  base::OnceClosure on_ready_;

  base::WeakPtrFactory<FlossAdvertiserClient> weak_ptr_factory_{this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FLOSS_ADVERTISER_CLIENT_H_
