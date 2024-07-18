// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/bluetooth/dbus/fake_bluetooth_gatt_characteristic_client.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/rand_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_device_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_descriptor_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

namespace {

const int kStartNotifyResponseIntervalMs = 200;
const int kHeartRateMeasurementNotificationIntervalMs = 2000;

}  // namespace

FakeBluetoothGattCharacteristicClient::DelayedCallback::DelayedCallback(
    base::OnceClosure callback,
    size_t delay)
    : callback_(std::move(callback)), delay_(delay) {}

FakeBluetoothGattCharacteristicClient::DelayedCallback::~DelayedCallback() =
    default;

// static
const char FakeBluetoothGattCharacteristicClient::
    kHeartRateMeasurementPathComponent[] = "char0000";
const char
    FakeBluetoothGattCharacteristicClient::kBodySensorLocationPathComponent[] =
        "char0001";
const char FakeBluetoothGattCharacteristicClient::
    kHeartRateControlPointPathComponent[] = "char0002";

// static
const char FakeBluetoothGattCharacteristicClient::kHeartRateMeasurementUUID[] =
    "00002a37-0000-1000-8000-00805f9b34fb";
const char FakeBluetoothGattCharacteristicClient::kBodySensorLocationUUID[] =
    "00002a38-0000-1000-8000-00805f9b34fb";
const char FakeBluetoothGattCharacteristicClient::kHeartRateControlPointUUID[] =
    "00002a39-0000-1000-8000-00805f9b34fb";

FakeBluetoothGattCharacteristicClient::Properties::Properties(
    const PropertyChangedCallback& callback)
    : BluetoothGattCharacteristicClient::Properties(
          NULL,
          bluetooth_gatt_characteristic::kBluetoothGattCharacteristicInterface,
          callback) {}

FakeBluetoothGattCharacteristicClient::Properties::~Properties() = default;

void FakeBluetoothGattCharacteristicClient::Properties::Get(
    dbus::PropertyBase* property,
    dbus::PropertySet::GetCallback callback) {
  DVLOG(1) << "Get " << property->name();
  std::move(callback).Run(true);
}

void FakeBluetoothGattCharacteristicClient::Properties::GetAll() {
  DVLOG(1) << "GetAll";
}

void FakeBluetoothGattCharacteristicClient::Properties::Set(
    dbus::PropertyBase* property,
    dbus::PropertySet::SetCallback callback) {
  DVLOG(1) << "Set " << property->name();
  std::move(callback).Run(false);
}

FakeBluetoothGattCharacteristicClient::FakeBluetoothGattCharacteristicClient()
    : heart_rate_visible_(false),
      authorized_(true),
      authenticated_(true),
      calories_burned_(0),
      extra_requests_(0) {}

FakeBluetoothGattCharacteristicClient::
    ~FakeBluetoothGattCharacteristicClient() {
  for (const auto& it : action_extra_requests_) {
    delete it.second;
  }
  action_extra_requests_.clear();
}

void FakeBluetoothGattCharacteristicClient::Init(
    dbus::Bus* bus,
    const std::string& bluetooth_service_name) {}

void FakeBluetoothGattCharacteristicClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeBluetoothGattCharacteristicClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::vector<dbus::ObjectPath>
FakeBluetoothGattCharacteristicClient::GetCharacteristics() {
  std::vector<dbus::ObjectPath> paths;
  if (IsHeartRateVisible()) {
    paths.push_back(dbus::ObjectPath(heart_rate_measurement_path_));
    paths.push_back(dbus::ObjectPath(body_sensor_location_path_));
    paths.push_back(dbus::ObjectPath(heart_rate_control_point_path_));
  }
  return paths;
}

FakeBluetoothGattCharacteristicClient::Properties*
FakeBluetoothGattCharacteristicClient::GetProperties(
    const dbus::ObjectPath& object_path) {
  if (object_path.value() == heart_rate_measurement_path_) {
    DCHECK(heart_rate_measurement_properties_.get());
    return heart_rate_measurement_properties_.get();
  }
  if (object_path.value() == body_sensor_location_path_) {
    DCHECK(body_sensor_location_properties_.get());
    return body_sensor_location_properties_.get();
  }
  if (object_path.value() == heart_rate_control_point_path_) {
    DCHECK(heart_rate_control_point_properties_.get());
    return heart_rate_control_point_properties_.get();
  }
  return NULL;
}

void FakeBluetoothGattCharacteristicClient::ReadValue(
    const dbus::ObjectPath& object_path,
    ValueCallback callback,
    ErrorCallback error_callback) {
  if (!authenticated_) {
    std::move(error_callback)
        .Run(bluetooth_gatt_service::kErrorNotPaired, "Please login");
    return;
  }

  if (!authorized_) {
    std::move(error_callback)
        .Run(bluetooth_gatt_service::kErrorNotAuthorized, "Authorize first");
    return;
  }

  if (object_path.value() == heart_rate_control_point_path_) {
    std::move(error_callback)
        .Run(bluetooth_gatt_service::kErrorNotPermitted,
             "Reads of this value are not allowed");
    return;
  }

  if (object_path.value() == heart_rate_measurement_path_) {
    std::move(error_callback)
        .Run(bluetooth_gatt_service::kErrorNotSupported,
             "Action not supported on this characteristic");
    return;
  }

  if (object_path.value() != body_sensor_location_path_) {
    std::move(error_callback).Run(kUnknownCharacteristicError, "");
    return;
  }

  if (base::Contains(action_extra_requests_, "ReadValue")) {
    DelayedCallback* delayed = action_extra_requests_["ReadValue"];
    delayed->delay_--;
    std::move(error_callback)
        .Run(bluetooth_gatt_service::kErrorInProgress,
             "Another read is currenty in progress");
    if (delayed->delay_ == 0) {
      std::move(delayed->callback_).Run();
      action_extra_requests_.erase("ReadValue");
      delete delayed;
    }
    return;
  }

  base::OnceClosure completed_callback;
  if (!IsHeartRateVisible()) {
    completed_callback = base::BindOnce(std::move(error_callback),
                                        kUnknownCharacteristicError, "");
  } else {
    std::vector<uint8_t> value = {0x06};  // Location is "foot".
    completed_callback = base::BindOnce(
        &FakeBluetoothGattCharacteristicClient::DelayedReadValueCallback,
        weak_ptr_factory_.GetWeakPtr(), object_path, std::move(callback),
        value);
  }

  if (extra_requests_ > 0) {
    action_extra_requests_["ReadValue"] =
        new DelayedCallback(std::move(completed_callback), extra_requests_);
    return;
  }

  std::move(completed_callback).Run();
}

void FakeBluetoothGattCharacteristicClient::WriteValue(
    const dbus::ObjectPath& object_path,
    const std::vector<uint8_t>& value,
    std::string_view type_option,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  if (!authenticated_) {
    std::move(error_callback)
        .Run(bluetooth_gatt_service::kErrorNotPaired, "Please login");
    return;
  }

  if (!authorized_) {
    std::move(error_callback)
        .Run(bluetooth_gatt_service::kErrorNotAuthorized, "Authorize first");
    return;
  }

  if (!IsHeartRateVisible()) {
    std::move(error_callback).Run(kUnknownCharacteristicError, "");
    return;
  }

  if (object_path.value() == heart_rate_measurement_path_) {
    std::move(error_callback)
        .Run(bluetooth_gatt_service::kErrorNotSupported,
             "Action not supported on this characteristic");
    return;
  }

  if (object_path.value() != heart_rate_control_point_path_) {
    std::move(error_callback)
        .Run(bluetooth_gatt_service::kErrorNotPermitted,
             "Writes of this value are not allowed");
    return;
  }

  DCHECK(heart_rate_control_point_properties_.get());
  if (base::Contains(action_extra_requests_, "WriteValue")) {
    DelayedCallback* delayed = action_extra_requests_["WriteValue"];
    delayed->delay_--;
    std::move(error_callback)
        .Run(bluetooth_gatt_service::kErrorInProgress,
             "Another write is in progress");
    if (delayed->delay_ == 0) {
      std::move(delayed->callback_).Run();
      action_extra_requests_.erase("WriteValue");
      delete delayed;
    }
    return;
  }
  base::OnceClosure completed_callback;
  if (value.size() != 1) {
    completed_callback =
        base::BindOnce(std::move(error_callback),
                       bluetooth_gatt_service::kErrorInvalidValueLength,
                       "Invalid length for write");
  } else if (value[0] > 1) {
    completed_callback = base::BindOnce(std::move(error_callback),
                                        bluetooth_gatt_service::kErrorFailed,
                                        "Invalid value given for write");
  } else if (value[0] == 1) {
    // TODO(jamuraa): make this happen when the callback happens
    calories_burned_ = 0;
    ScheduleHeartRateMeasurementValueChange();
    completed_callback = std::move(callback);
  }

  if (extra_requests_ > 0) {
    action_extra_requests_["WriteValue"] =
        new DelayedCallback(std::move(completed_callback), extra_requests_);
    return;
  }
  std::move(completed_callback).Run();
}

void FakeBluetoothGattCharacteristicClient::PrepareWriteValue(
    const dbus::ObjectPath& object_path,
    const std::vector<uint8_t>& value,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  if (!authenticated_) {
    std::move(error_callback)
        .Run(bluetooth_gatt_service::kErrorNotPaired, "Please login");
    return;
  }

  if (!authorized_) {
    std::move(error_callback)
        .Run(bluetooth_gatt_service::kErrorNotAuthorized, "Authorize first");
    return;
  }

  if (!IsHeartRateVisible()) {
    std::move(error_callback).Run(kUnknownCharacteristicError, "");
    return;
  }

  if (object_path.value() == heart_rate_measurement_path_) {
    std::move(error_callback)
        .Run(bluetooth_gatt_service::kErrorNotSupported,
             "Action not supported on this characteristic");
    return;
  }

  if (object_path.value() != heart_rate_control_point_path_) {
    std::move(error_callback)
        .Run(bluetooth_gatt_service::kErrorNotPermitted,
             "Writes of this value are not allowed");
    return;
  }

  DCHECK(heart_rate_control_point_properties_.get());
  static_cast<FakeBluetoothDeviceClient*>(
      bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient())
      ->AddPrepareWriteRequest(object_path, value);
  std::move(callback).Run();
}

void FakeBluetoothGattCharacteristicClient::StartNotify(
    const dbus::ObjectPath& object_path,
#if BUILDFLAG(IS_CHROMEOS)
    device::BluetoothGattCharacteristic::NotificationType notification_type,
#endif  // BUILDFLAG(IS_CHROMEOS)
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  if (!IsHeartRateVisible()) {
    std::move(error_callback).Run(kUnknownCharacteristicError, "");
    return;
  }

  if (object_path.value() != heart_rate_measurement_path_) {
    std::move(error_callback)
        .Run(bluetooth_gatt_service::kErrorNotSupported,
             "This characteristic does not support notifications");
    return;
  }

  if (heart_rate_measurement_properties_->notifying.value()) {
    std::move(error_callback)
        .Run(bluetooth_gatt_service::kErrorInProgress,
             "Characteristic already notifying");
    return;
  }

  heart_rate_measurement_properties_->notifying.ReplaceValue(true);
  ScheduleHeartRateMeasurementValueChange();

  // Respond asynchronously.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, std::move(callback),
      base::Milliseconds(kStartNotifyResponseIntervalMs));
}

void FakeBluetoothGattCharacteristicClient::StopNotify(
    const dbus::ObjectPath& object_path,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  if (!IsHeartRateVisible()) {
    std::move(error_callback).Run(kUnknownCharacteristicError, "");
    return;
  }

  if (object_path.value() != heart_rate_measurement_path_) {
    std::move(error_callback)
        .Run(bluetooth_gatt_service::kErrorNotSupported,
             "This characteristic does not support notifications");
    return;
  }

  if (!heart_rate_measurement_properties_->notifying.value()) {
    std::move(error_callback)
        .Run(bluetooth_gatt_service::kErrorFailed, "Not notifying");
    return;
  }

  heart_rate_measurement_properties_->notifying.ReplaceValue(false);

  std::move(callback).Run();
}

void FakeBluetoothGattCharacteristicClient::ExposeHeartRateCharacteristics(
    const dbus::ObjectPath& service_path) {
  if (IsHeartRateVisible()) {
    DVLOG(2) << "Fake Heart Rate characteristics are already visible.";
    return;
  }

  DVLOG(2) << "Exposing fake Heart Rate characteristics.";

  std::vector<std::string> flags;

  // ==== Heart Rate Measurement Characteristic ====
  heart_rate_measurement_path_ =
      service_path.value() + "/" + kHeartRateMeasurementPathComponent;
  heart_rate_measurement_properties_ =
      std::make_unique<Properties>(base::BindRepeating(
          &FakeBluetoothGattCharacteristicClient::OnPropertyChanged,
          weak_ptr_factory_.GetWeakPtr(),
          dbus::ObjectPath(heart_rate_measurement_path_)));
  heart_rate_measurement_properties_->uuid.ReplaceValue(
      kHeartRateMeasurementUUID);
  heart_rate_measurement_properties_->service.ReplaceValue(service_path);
  flags.push_back(bluetooth_gatt_characteristic::kFlagNotify);
  flags.push_back(bluetooth_gatt_characteristic::kFlagIndicate);
  heart_rate_measurement_properties_->flags.ReplaceValue(flags);

  // ==== Body Sensor Location Characteristic ====
  body_sensor_location_path_ =
      service_path.value() + "/" + kBodySensorLocationPathComponent;
  body_sensor_location_properties_ =
      std::make_unique<Properties>(base::BindRepeating(
          &FakeBluetoothGattCharacteristicClient::OnPropertyChanged,
          weak_ptr_factory_.GetWeakPtr(),
          dbus::ObjectPath(body_sensor_location_path_)));
  body_sensor_location_properties_->uuid.ReplaceValue(kBodySensorLocationUUID);
  body_sensor_location_properties_->service.ReplaceValue(service_path);
  flags.clear();
  flags.push_back(bluetooth_gatt_characteristic::kFlagRead);
  body_sensor_location_properties_->flags.ReplaceValue(flags);

  // ==== Heart Rate Control Point Characteristic ====
  heart_rate_control_point_path_ =
      service_path.value() + "/" + kHeartRateControlPointPathComponent;
  heart_rate_control_point_properties_ =
      std::make_unique<Properties>(base::BindRepeating(
          &FakeBluetoothGattCharacteristicClient::OnPropertyChanged,
          weak_ptr_factory_.GetWeakPtr(),
          dbus::ObjectPath(heart_rate_control_point_path_)));
  heart_rate_control_point_properties_->uuid.ReplaceValue(
      kHeartRateControlPointUUID);
  heart_rate_control_point_properties_->service.ReplaceValue(service_path);
  flags.clear();
  flags.push_back(bluetooth_gatt_characteristic::kFlagWrite);
  heart_rate_control_point_properties_->flags.ReplaceValue(flags);

  heart_rate_visible_ = true;

  NotifyCharacteristicAdded(dbus::ObjectPath(heart_rate_measurement_path_));
  NotifyCharacteristicAdded(dbus::ObjectPath(body_sensor_location_path_));
  NotifyCharacteristicAdded(dbus::ObjectPath(heart_rate_control_point_path_));

  // Expose CCC descriptor for Heart Rate Measurement characteristic.
  FakeBluetoothGattDescriptorClient* descriptor_client =
      static_cast<FakeBluetoothGattDescriptorClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothGattDescriptorClient());
  dbus::ObjectPath ccc_path(descriptor_client->ExposeDescriptor(
      dbus::ObjectPath(heart_rate_measurement_path_),
      FakeBluetoothGattDescriptorClient::
          kClientCharacteristicConfigurationUUID));
  DCHECK(ccc_path.IsValid());
  heart_rate_measurement_ccc_desc_path_ = ccc_path.value();
}

void FakeBluetoothGattCharacteristicClient::HideHeartRateCharacteristics() {
  DVLOG(2) << "Hiding fake Heart Rate characteristics.";

  // Hide the descriptors.
  FakeBluetoothGattDescriptorClient* descriptor_client =
      static_cast<FakeBluetoothGattDescriptorClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothGattDescriptorClient());
  descriptor_client->HideDescriptor(
      dbus::ObjectPath(heart_rate_measurement_ccc_desc_path_));

  // Notify the observers before deleting the properties structures so that they
  // can be accessed from the observer method.
  NotifyCharacteristicRemoved(dbus::ObjectPath(heart_rate_measurement_path_));
  NotifyCharacteristicRemoved(dbus::ObjectPath(body_sensor_location_path_));
  NotifyCharacteristicRemoved(dbus::ObjectPath(heart_rate_control_point_path_));

  heart_rate_measurement_properties_.reset();
  body_sensor_location_properties_.reset();
  heart_rate_control_point_properties_.reset();

  heart_rate_measurement_path_.clear();
  body_sensor_location_path_.clear();
  heart_rate_control_point_path_.clear();
  heart_rate_visible_ = false;
}

void FakeBluetoothGattCharacteristicClient::SetExtraProcessing(
    size_t requests) {
  extra_requests_ = requests;
  if (extra_requests_ == 0) {
    for (const auto& it : action_extra_requests_) {
      std::move(it.second->callback_).Run();
      delete it.second;
    }
    action_extra_requests_.clear();
    return;
  }
  DVLOG(2) << "Requests SLOW now, " << requests << " InProgress errors each.";
}

size_t FakeBluetoothGattCharacteristicClient::GetExtraProcessing() const {
  return extra_requests_;
}

dbus::ObjectPath
FakeBluetoothGattCharacteristicClient::GetHeartRateMeasurementPath() const {
  return dbus::ObjectPath(heart_rate_measurement_path_);
}

dbus::ObjectPath
FakeBluetoothGattCharacteristicClient::GetBodySensorLocationPath() const {
  return dbus::ObjectPath(body_sensor_location_path_);
}

dbus::ObjectPath
FakeBluetoothGattCharacteristicClient::GetHeartRateControlPointPath() const {
  return dbus::ObjectPath(heart_rate_control_point_path_);
}

void FakeBluetoothGattCharacteristicClient::OnPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  DVLOG(2) << "Characteristic property changed: " << object_path.value() << ": "
           << property_name;

  for (auto& observer : observers_)
    observer.GattCharacteristicPropertyChanged(object_path, property_name);
}

void FakeBluetoothGattCharacteristicClient::NotifyCharacteristicAdded(
    const dbus::ObjectPath& object_path) {
  DVLOG(2) << "GATT characteristic added: " << object_path.value();
  for (auto& observer : observers_)
    observer.GattCharacteristicAdded(object_path);
}

void FakeBluetoothGattCharacteristicClient::NotifyCharacteristicRemoved(
    const dbus::ObjectPath& object_path) {
  DVLOG(2) << "GATT characteristic removed: " << object_path.value();
  for (auto& observer : observers_)
    observer.GattCharacteristicRemoved(object_path);
}

void FakeBluetoothGattCharacteristicClient::
    ScheduleHeartRateMeasurementValueChange() {
  if (!IsHeartRateVisible())
    return;

  // Don't send updates if the characteristic is not notifying.
  if (!heart_rate_measurement_properties_->notifying.value())
    return;

  DVLOG(2) << "Updating heart rate value.";
  std::vector<uint8_t> measurement = GetHeartRateMeasurementValue();
  heart_rate_measurement_properties_->value.ReplaceValue(measurement);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeBluetoothGattCharacteristicClient::
                         ScheduleHeartRateMeasurementValueChange,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Milliseconds(kHeartRateMeasurementNotificationIntervalMs));
}

void FakeBluetoothGattCharacteristicClient::DelayedReadValueCallback(
    const dbus::ObjectPath& object_path,
    ValueCallback callback,
    const std::vector<uint8_t>& value) {
  Properties* properties = GetProperties(object_path);
  DCHECK(properties);

  properties->value.ReplaceValue(value);
  std::move(callback).Run(/*error_code=*/std::nullopt, value);
}

std::vector<uint8_t>
FakeBluetoothGattCharacteristicClient::GetHeartRateMeasurementValue() {
  // TODO(armansito): We should make sure to properly pack this struct to ensure
  // correct byte alignment and endianness. It doesn't matter too much right now
  // as this is a fake and GCC on Linux seems to do the right thing.
  struct {
    uint8_t flags;
    uint8_t bpm;
    uint16_t energy_expanded;
    uint16_t rr_interval;
  } value;

  // Flags in LSB:     0       11   1 1 000
  //                   |       |    | | |
  // 8-bit bpm format --       |    | | |
  // Sensor contact supported --    | | |
  // Energy expanded field present -- | |
  // RR-Interval values present ------- |
  // Reserved for future use ------------
  value.flags = 0x0;
  value.flags |= (0x03 << 1);
  value.flags |= (0x01 << 3);
  value.flags |= (0x01 << 4);

  // Pick a value between 117 bpm and 153 bpm for heart rate.
  value.bpm = static_cast<uint8_t>(base::RandInt(117, 153));

  // Total calories burned in kJoules since the last reset. Increment this by 1
  // every time. It's fine if it overflows: it becomes 0 when the user resets
  // the heart rate monitor (or pretend that they had a lot of cheeseburgers).
  value.energy_expanded = calories_burned_++;

  // Include one RR-Interval value, in seconds.
  value.rr_interval = 60 / value.bpm;

  // Return the bytes in an array.
  uint8_t* bytes = reinterpret_cast<uint8_t*>(&value);
  std::vector<uint8_t> return_value;
  return_value.assign(bytes, bytes + sizeof(value));
  return return_value;
}

bool FakeBluetoothGattCharacteristicClient::IsHeartRateVisible() const {
  DCHECK(heart_rate_visible_ != heart_rate_measurement_path_.empty());
  DCHECK(heart_rate_visible_ != body_sensor_location_path_.empty());
  DCHECK(heart_rate_visible_ != heart_rate_control_point_path_.empty());
  DCHECK(heart_rate_visible_ == !!heart_rate_measurement_properties_.get());
  DCHECK(heart_rate_visible_ == !!body_sensor_location_properties_.get());
  DCHECK(heart_rate_visible_ == !!heart_rate_control_point_properties_.get());
  return heart_rate_visible_;
}

}  // namespace bluez
