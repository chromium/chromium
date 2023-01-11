// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADVERTISEMENT_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADVERTISEMENT_H_

#include <stdint.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {

// BluetoothAdvertisement represents an advertisement which advertises over the
// LE channel during its lifetime.
class DEVICE_BLUETOOTH_EXPORT BluetoothAdvertisement
    : public base::RefCounted<BluetoothAdvertisement> {
 public:
  // Possible types of error raised while registering or unregistering
  // advertisements.
  enum ErrorCode {
    ERROR_UNSUPPORTED_PLATFORM,  // Bluetooth advertisement not supported on
                                 // current platform.
    ERROR_ADVERTISEMENT_ALREADY_EXISTS,  // An advertisement is already
                                         // registered.
    ERROR_ADVERTISEMENT_DOES_NOT_EXIST,  // Unregistering an advertisement which
                                         // is not registered.
    ERROR_ADVERTISEMENT_INVALID_LENGTH,  // Advertisement is not of a valid
                                         // length.
    ERROR_STARTING_ADVERTISEMENT,  // Error when starting the advertisement
                                   // through a platform API.
    ERROR_RESET_ADVERTISING,       // Error while resetting advertising.
    ERROR_ADAPTER_POWERED_OFF,     // Error because the adapter is off
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    ERROR_INVALID_ADVERTISEMENT_INTERVAL,  // Advertisement interval specified
                                           // is out of valid range.
#endif
    INVALID_ADVERTISEMENT_ERROR_CODE
  };

  // Type of advertisement.
  enum AdvertisementType {
    // This advertises with the type set to ADV_NONCONN_IND, which indicates
    // to receivers that our device is not connectable.
    ADVERTISEMENT_TYPE_BROADCAST,
    // This advertises with the type set to ADV_IND or ADV_SCAN_IND, which
    // indicates to receivers that our device is connectable.
    ADVERTISEMENT_TYPE_PERIPHERAL
  };

  using UUIDList = std::vector<std::string>;
  using ManufacturerData = std::map<uint16_t, std::vector<uint8_t>>;
  using ServiceData = std::map<std::string, std::vector<uint8_t>>;
  using ScanResponseData = std::map<uint8_t, std::vector<uint8_t>>;

  // Structure that holds the data for an advertisement.
  class DEVICE_BLUETOOTH_EXPORT Data {
   public:
    explicit Data(AdvertisementType type);

    Data(const Data&) = delete;
    Data& operator=(const Data&) = delete;

    ~Data();

    AdvertisementType type() { return type_; }

    absl::optional<UUIDList> service_uuids() {
      return pass_value(service_uuids_);
    }
    absl::optional<ManufacturerData> manufacturer_data() {
      return pass_value(manufacturer_data_);
    }
    absl::optional<UUIDList> solicit_uuids() {
      return pass_value(solicit_uuids_);
    }
    absl::optional<ServiceData> service_data() {
      return pass_value(service_data_);
    }
    absl::optional<ScanResponseData> scan_response_data() {
      return pass_value(scan_response_data_);
    }

    void set_service_uuids(absl::optional<UUIDList> service_uuids) {
      service_uuids_ = std::move(service_uuids);
    }
    void set_manufacturer_data(
        absl::optional<ManufacturerData> manufacturer_data) {
      manufacturer_data_ = std::move(manufacturer_data);
    }
    void set_solicit_uuids(absl::optional<UUIDList> solicit_uuids) {
      solicit_uuids_ = std::move(solicit_uuids);
    }
    void set_service_data(absl::optional<ServiceData> service_data) {
      service_data_ = std::move(service_data);
    }
    void set_scan_response_data(
        absl::optional<ScanResponseData> scan_response_data) {
      scan_response_data_ = std::move(scan_response_data);
    }

    void set_include_tx_power(bool include_tx_power) {
      include_tx_power_ = include_tx_power;
    }

   private:
    Data();

    // Passes the value along held by |from|, and restore the optional moved
    // from to nullopt.
    template <typename T>
    static absl::optional<T> pass_value(absl::optional<T>& from) {
      absl::optional<T> value = std::move(from);
      from = absl::nullopt;
      return value;
    }

    AdvertisementType type_;
    absl::optional<UUIDList> service_uuids_;
    absl::optional<ManufacturerData> manufacturer_data_;
    absl::optional<UUIDList> solicit_uuids_;
    absl::optional<ServiceData> service_data_;
    absl::optional<ScanResponseData> scan_response_data_;
    bool include_tx_power_;
  };

  // Interface for observing changes to this advertisement.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when this advertisement is released and is no longer advertising.
    virtual void AdvertisementReleased(
        BluetoothAdvertisement* advertisement) = 0;
  };

  BluetoothAdvertisement(const BluetoothAdvertisement&) = delete;
  BluetoothAdvertisement& operator=(const BluetoothAdvertisement&) = delete;

  // Adds and removes observers for events for this advertisement.
  void AddObserver(BluetoothAdvertisement::Observer* observer);
  void RemoveObserver(BluetoothAdvertisement::Observer* observer);

  // Unregisters this advertisement. Called on destruction of this object
  // automatically but can be called directly to explicitly unregister this
  // object.
  using SuccessCallback = base::OnceClosure;
  using ErrorCallback = base::OnceCallback<void(ErrorCode)>;
  virtual void Unregister(SuccessCallback success_callback,
                          ErrorCallback error_callback) = 0;

 protected:
  friend class base::RefCounted<BluetoothAdvertisement>;

  BluetoothAdvertisement();

  // The destructor will unregister this advertisement.
  virtual ~BluetoothAdvertisement();

  // List of observers interested in event notifications from us. Objects in
  // |observers_| are expected to outlive a BluetoothAdvertisement object.
  base::ObserverList<BluetoothAdvertisement::Observer>::Unchecked observers_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADVERTISEMENT_H_
