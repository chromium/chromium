// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_device_bluez.h"

#include <stdio.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/device_event_log/device_event_log.h"
#include "dbus/bus.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/bluetooth_socket_thread.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"
#include "device/bluetooth/bluez/bluetooth_gatt_connection_bluez.h"
#include "device/bluetooth/bluez/bluetooth_pairing_bluez.h"
#include "device/bluetooth/bluez/bluetooth_remote_gatt_service_bluez.h"
#include "device/bluetooth/bluez/bluetooth_service_record_bluez.h"
#include "device/bluetooth/bluez/bluetooth_socket_bluez.h"
#include "device/bluetooth/dbus/bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/bluetooth_device_client.h"
#include "device/bluetooth/dbus/bluetooth_gatt_service_client.h"
#include "device/bluetooth/dbus/bluetooth_input_client.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using device::BluetoothDevice;
using device::BluetoothRemoteGattService;
using device::BluetoothSocket;
using device::BluetoothUUID;

namespace {

// The unit for connection interval values are in multiples of 1.25ms.
const uint16_t MIN_CONNECTION_INTERVAL_LOW = 6;
const uint16_t MAX_CONNECTION_INTERVAL_LOW = 6;
const uint16_t MIN_CONNECTION_INTERVAL_MEDIUM = 40;
const uint16_t MAX_CONNECTION_INTERVAL_MEDIUM = 56;
const uint16_t MIN_CONNECTION_INTERVAL_HIGH = 80;
const uint16_t MAX_CONNECTION_INTERVAL_HIGH = 100;

// Histogram enumerations for pairing results.
enum UMAPairingResult {
  UMA_PAIRING_RESULT_SUCCESS,
  UMA_PAIRING_RESULT_INPROGRESS,
  UMA_PAIRING_RESULT_FAILED,
  UMA_PAIRING_RESULT_AUTH_FAILED,
  UMA_PAIRING_RESULT_AUTH_CANCELED,
  UMA_PAIRING_RESULT_AUTH_REJECTED,
  UMA_PAIRING_RESULT_AUTH_TIMEOUT,
  UMA_PAIRING_RESULT_UNSUPPORTED_DEVICE,
  UMA_PAIRING_RESULT_UNKNOWN_ERROR,
  // NOTE: Add new pairing results immediately above this line. Make sure to
  // update the enum list in tools/histogram/histograms.xml accordinly.
  UMA_PAIRING_RESULT_COUNT
};

void ParseModalias(const dbus::ObjectPath& object_path,
                   BluetoothDevice::VendorIDSource* vendor_id_source,
                   uint16_t* vendor_id,
                   uint16_t* product_id,
                   uint16_t* device_id) {
  bluez::BluetoothDeviceClient::Properties* properties =
      bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path);
  DCHECK(properties);

  std::string modalias = properties->modalias.value();
  BluetoothDevice::VendorIDSource source_value;
  int vendor_value, product_value, device_value;

  if (sscanf(modalias.c_str(), "bluetooth:v%04xp%04xd%04x", &vendor_value,
             &product_value, &device_value) == 3) {
    source_value = BluetoothDevice::VENDOR_ID_BLUETOOTH;
  } else if (sscanf(modalias.c_str(), "usb:v%04xp%04xd%04x", &vendor_value,
                    &product_value, &device_value) == 3) {
    source_value = BluetoothDevice::VENDOR_ID_USB;
  } else {
    return;
  }

  if (vendor_id_source != nullptr)
    *vendor_id_source = source_value;
  if (vendor_id != nullptr)
    *vendor_id = vendor_value;
  if (product_id != nullptr)
    *product_id = product_value;
  if (device_id != nullptr)
    *device_id = device_value;
}

void RecordPairingResult(BluetoothDevice::ConnectErrorCode error_code) {
  UMAPairingResult pairing_result;
  switch (error_code) {
    case BluetoothDevice::ERROR_INPROGRESS:
      pairing_result = UMA_PAIRING_RESULT_INPROGRESS;
      break;
    case BluetoothDevice::ERROR_FAILED:
      pairing_result = UMA_PAIRING_RESULT_FAILED;
      break;
    case BluetoothDevice::ERROR_AUTH_FAILED:
      pairing_result = UMA_PAIRING_RESULT_AUTH_FAILED;
      break;
    case BluetoothDevice::ERROR_AUTH_CANCELED:
      pairing_result = UMA_PAIRING_RESULT_AUTH_CANCELED;
      break;
    case BluetoothDevice::ERROR_AUTH_REJECTED:
      pairing_result = UMA_PAIRING_RESULT_AUTH_REJECTED;
      break;
    case BluetoothDevice::ERROR_AUTH_TIMEOUT:
      pairing_result = UMA_PAIRING_RESULT_AUTH_TIMEOUT;
      break;
    case BluetoothDevice::ERROR_UNSUPPORTED_DEVICE:
      pairing_result = UMA_PAIRING_RESULT_UNSUPPORTED_DEVICE;
      break;
    default:
      pairing_result = UMA_PAIRING_RESULT_UNKNOWN_ERROR;
  }

  UMA_HISTOGRAM_ENUMERATION("Bluetooth.PairingResult", pairing_result,
                            UMA_PAIRING_RESULT_COUNT);
}

BluetoothDevice::ConnectErrorCode DBusErrorToConnectError(
    const std::string& error_name) {
  BluetoothDevice::ConnectErrorCode error_code = BluetoothDevice::ERROR_UNKNOWN;
  if (error_name == bluetooth_device::kErrorConnectionAttemptFailed) {
    error_code = BluetoothDevice::ERROR_FAILED;
  } else if (error_name == bluetooth_device::kErrorFailed) {
    error_code = BluetoothDevice::ERROR_FAILED;
  } else if (error_name == bluetooth_device::kErrorAuthenticationFailed) {
    error_code = BluetoothDevice::ERROR_AUTH_FAILED;
  } else if (error_name == bluetooth_device::kErrorAuthenticationCanceled) {
    error_code = BluetoothDevice::ERROR_AUTH_CANCELED;
  } else if (error_name == bluetooth_device::kErrorAuthenticationRejected) {
    error_code = BluetoothDevice::ERROR_AUTH_REJECTED;
  } else if (error_name == bluetooth_device::kErrorAuthenticationTimeout) {
    error_code = BluetoothDevice::ERROR_AUTH_TIMEOUT;
  }
  return error_code;
}

}  // namespace

namespace bluez {

BluetoothDeviceBlueZ::BluetoothDeviceBlueZ(
    BluetoothAdapterBlueZ* adapter,
    const dbus::ObjectPath& object_path,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<device::BluetoothSocketThread> socket_thread)
    : BluetoothDevice(adapter),
      object_path_(object_path),
      num_connecting_calls_(0),
      ui_task_runner_(ui_task_runner),
      socket_thread_(socket_thread),
      weak_ptr_factory_(this) {
  bluez::BluezDBusManager::Get()->GetBluetoothGattServiceClient()->AddObserver(
      this);

  // If GATT Services have already been discovered update the list of Gatt
  // Services.
  if (IsGattServicesDiscoveryComplete()) {
    UpdateGattServices(object_path_);
  } else {
    BLUETOOTH_LOG(DEBUG)
        << "Gatt services have not been fully resolved for device "
        << object_path_.value();
  }

  // Update all the data that we cache within Chrome and do not pull from
  // properties every time. TODO(xiaoyinh): Add a test for this. See
  // http://crbug.com/688566.
  UpdateServiceData();
  UpdateManufacturerData();
  UpdateAdvertisingDataFlags();
}

BluetoothDeviceBlueZ::~BluetoothDeviceBlueZ() {
  bluez::BluezDBusManager::Get()
      ->GetBluetoothGattServiceClient()
      ->RemoveObserver(this);

  // Copy the GATT services list here and clear the original so that when we
  // send GattServiceRemoved(), GetGattServices() returns no services.
  GattServiceMap gatt_services_swapped;
  gatt_services_swapped.swap(gatt_services_);
  for (const auto& iter : gatt_services_swapped) {
    DCHECK(adapter());
    adapter()->NotifyGattServiceRemoved(
        static_cast<BluetoothRemoteGattServiceBlueZ*>(iter.second.get()));
  }

  // We pause discovery when trying to connect. Ensure discovery is unpaused if
  // we get destroyed during a pending connection.
  if (IsConnecting()) {
    BLUETOOTH_LOG(EVENT) << object_path_.value()
                         << ": Unpausing discovery. Device removed.";
    UnpauseDiscovery();
  }
}

uint32_t BluetoothDeviceBlueZ::GetBluetoothClass() const {
  bluez::BluetoothDeviceClient::Properties* properties =
      bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path_);
  DCHECK(properties);

  return properties->bluetooth_class.value();
}

device::BluetoothTransport BluetoothDeviceBlueZ::GetType() const {
  bluez::BluetoothDeviceClient::Properties* properties =
      bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path_);
  DCHECK(properties);

  if (!properties->type.is_valid())
    return device::BLUETOOTH_TRANSPORT_INVALID;

  std::string type = properties->type.value();
  if (type == bluez::BluetoothDeviceClient::kTypeBredr) {
    return device::BLUETOOTH_TRANSPORT_CLASSIC;
  } else if (type == bluez::BluetoothDeviceClient::kTypeLe) {
    return device::BLUETOOTH_TRANSPORT_LE;
  } else if (type == bluez::BluetoothDeviceClient::kTypeDual) {
    return device::BLUETOOTH_TRANSPORT_DUAL;
  }

  NOTREACHED();
  return device::BLUETOOTH_TRANSPORT_INVALID;
}

void BluetoothDeviceBlueZ::CreateGattConnectionImpl() {
  // BlueZ implementation does not use the default CreateGattConnection
  // implementation.
  NOTIMPLEMENTED();
}

void BluetoothDeviceBlueZ::SetGattServicesDiscoveryComplete(bool complete) {
  // BlueZ implementation already tracks service discovery state.
  NOTIMPLEMENTED();
}

bool BluetoothDeviceBlueZ::IsGattServicesDiscoveryComplete() const {
  bluez::BluetoothDeviceClient::Properties* properties =
      bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path_);
  DCHECK(properties);

  return properties->services_resolved.value();
}

void BluetoothDeviceBlueZ::DisconnectGatt() {
  // There isn't currently a good way to manage the ownership of a connection
  // between Chrome and bluetoothd plugins/profiles. Until a proper reference
  // count is kept in bluetoothd, we might unwittingly kill a connection to a
  // device the user is still interested in, e.g. a mouse. A device's paired
  // status is usually a good indication that the device is being used by other
  // parts of the system and therefore we leak these connections.
  // TODO(crbug.com/630586): Call disconnect for all devices.

  // IsPaired() returns true if we've connected to the device before. So we
  // check the dbus property directly.
  // TODO(crbug.com/649651): Use IsPaired once it returns true only for paired
  // devices.
  bluez::BluetoothDeviceClient::Properties* properties =
      bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path_);
  DCHECK(properties);

  if (properties->paired.value()) {
    BLUETOOTH_LOG(ERROR) << "Leaking connection to paired device.";
    return;
  }

  Disconnect(base::DoNothing(), base::DoNothing());
}

std::string BluetoothDeviceBlueZ::GetAddress() const {
  bluez::BluetoothDeviceClient::Properties* properties =
      bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path_);
  DCHECK(properties);

  return CanonicalizeAddress(properties->address.value());
}

BluetoothDevice::VendorIDSource BluetoothDeviceBlueZ::GetVendorIDSource()
    const {
  VendorIDSource vendor_id_source = VENDOR_ID_UNKNOWN;
  ParseModalias(object_path_, &vendor_id_source, nullptr, nullptr, nullptr);
  return vendor_id_source;
}

uint16_t BluetoothDeviceBlueZ::GetVendorID() const {
  uint16_t vendor_id = 0;
  ParseModalias(object_path_, nullptr, &vendor_id, nullptr, nullptr);
  return vendor_id;
}

uint16_t BluetoothDeviceBlueZ::GetProductID() const {
  uint16_t product_id = 0;
  ParseModalias(object_path_, nullptr, nullptr, &product_id, nullptr);
  return product_id;
}

uint16_t BluetoothDeviceBlueZ::GetDeviceID() const {
  uint16_t device_id = 0;
  ParseModalias(object_path_, nullptr, nullptr, nullptr, &device_id);
  return device_id;
}

uint16_t BluetoothDeviceBlueZ::GetAppearance() const {
  bluez::BluetoothDeviceClient::Properties* properties =
      bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path_);
  DCHECK(properties);

  if (!properties->appearance.is_valid())
    return kAppearanceNotPresent;

  return properties->appearance.value();
}

base::Optional<std::string> BluetoothDeviceBlueZ::GetName() const {
  bluez::BluetoothDeviceClient::Properties* properties =
      bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path_);
  DCHECK(properties);

  if (properties->name.is_valid())
    return properties->name.value();
  else
    return base::nullopt;
}

bool BluetoothDeviceBlueZ::IsPaired() const {
  bluez::BluetoothDeviceClient::Properties* properties =
      bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path_);
  DCHECK(properties);

  // The Paired property reflects the successful pairing for BR/EDR/LE. The
  // value of the Paired property is always false for the devices that don't
  // support pairing. Once a device is paired successfully, both Paired and
  // Trusted properties will be set to true.
  return properties->paired.value();
}

bool BluetoothDeviceBlueZ::IsConnected() const {
  bluez::BluetoothDeviceClient::Properties* properties =
      bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path_);
  DCHECK(properties);

  return properties->connected.value();
}

bool BluetoothDeviceBlueZ::IsGattConnected() const {
  // Bluez uses the same attribute for GATT Connections and Classic BT
  // Connections.
  return IsConnected();
}

bool BluetoothDeviceBlueZ::IsConnectable() const {
  bluez::BluetoothInputClient::Properties* input_properties =
      bluez::BluezDBusManager::Get()->GetBluetoothInputClient()->GetProperties(
          object_path_);
  // GetProperties returns nullptr when the device does not implement the given
  // interface. Non HID devices are normally connectable.
  if (!input_properties)
    return true;

  return input_properties->reconnect_mode.value() != "device";
}

bool BluetoothDeviceBlueZ::IsConnecting() const {
  return num_connecting_calls_ > 0;
}

BluetoothDevice::UUIDSet BluetoothDeviceBlueZ::GetUUIDs() const {
  bluez::BluetoothDeviceClient::Properties* properties =
      bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path_);
  DCHECK(properties);

  UUIDSet uuids;
  const std::vector<std::string>& dbus_uuids = properties->uuids.value();
  for (const std::string& dbus_uuid : dbus_uuids) {
    device::BluetoothUUID uuid(dbus_uuid);
    DCHECK(uuid.IsValid());
    uuids.insert(std::move(uuid));
  }
  return uuids;
}

base::Optional<int8_t> BluetoothDeviceBlueZ::GetInquiryRSSI() const {
  bluez::BluetoothDeviceClient::Properties* properties =
      bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path_);
  DCHECK(properties);

  if (!properties->rssi.is_valid())
    return base::nullopt;

  // BlueZ uses int16_t because there is no int8_t for DBus, so we should never
  // get an int16_t that cannot be represented by an int8_t. But just in case
  // clamp the value.
  return ClampPower(properties->rssi.value());
}

base::Optional<int8_t> BluetoothDeviceBlueZ::GetInquiryTxPower() const {
  bluez::BluetoothDeviceClient::Properties* properties =
      bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path_);
  DCHECK(properties);

  if (!properties->tx_power.is_valid())
    return base::nullopt;

  // BlueZ uses int16_t because there is no int8_t for DBus, so we should never
  // get an int16_t that cannot be represented by an int8_t. But just in case
  // clamp the value.
  return ClampPower(properties->tx_power.value());
}

bool BluetoothDeviceBlueZ::ExpectingPinCode() const {
  return pairing_.get() && pairing_->ExpectingPinCode();
}

bool BluetoothDeviceBlueZ::ExpectingPasskey() const {
  return pairing_.get() && pairing_->ExpectingPasskey();
}

bool BluetoothDeviceBlueZ::ExpectingConfirmation() const {
  return pairing_.get() && pairing_->ExpectingConfirmation();
}

void BluetoothDeviceBlueZ::GetConnectionInfo(
    const ConnectionInfoCallback& callback) {
  // DBus method call should gracefully return an error if the device is not
  // currently connected.
  bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetConnInfo(
      object_path_,
      base::Bind(&BluetoothDeviceBlueZ::OnGetConnInfo,
                 weak_ptr_factory_.GetWeakPtr(), callback),
      base::Bind(&BluetoothDeviceBlueZ::OnGetConnInfoError,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void BluetoothDeviceBlueZ::SetConnectionLatency(
    ConnectionLatency connection_latency,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  uint16_t min_connection_interval = MIN_CONNECTION_INTERVAL_MEDIUM;
  uint16_t max_connection_interval = MAX_CONNECTION_INTERVAL_MEDIUM;
  switch (connection_latency) {
    case ConnectionLatency::CONNECTION_LATENCY_LOW:
      min_connection_interval = MIN_CONNECTION_INTERVAL_LOW;
      max_connection_interval = MAX_CONNECTION_INTERVAL_LOW;
      break;
    case ConnectionLatency::CONNECTION_LATENCY_MEDIUM:
      min_connection_interval = MIN_CONNECTION_INTERVAL_MEDIUM;
      max_connection_interval = MAX_CONNECTION_INTERVAL_MEDIUM;
      break;
    case ConnectionLatency::CONNECTION_LATENCY_HIGH:
      min_connection_interval = MIN_CONNECTION_INTERVAL_HIGH;
      max_connection_interval = MAX_CONNECTION_INTERVAL_HIGH;
      break;
    default:
      NOTREACHED();
      break;
  }

  BLUETOOTH_LOG(EVENT) << "Setting LE connection parameters: min="
                       << min_connection_interval
                       << ", max=" << max_connection_interval;
  bluez::BluetoothDeviceClient::ConnectionParameters connection_parameters;
  connection_parameters.min_connection_interval = min_connection_interval;
  connection_parameters.max_connection_interval = max_connection_interval;

  bluez::BluetoothDeviceClient* client =
      bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient();
  client->SetLEConnectionParameters(
      object_path_, connection_parameters,
      base::Bind(&BluetoothDeviceBlueZ::OnSetLEConnectionParameters,
                 weak_ptr_factory_.GetWeakPtr(), callback),
      base::Bind(&BluetoothDeviceBlueZ::OnSetLEConnectionParametersError,
                 weak_ptr_factory_.GetWeakPtr(), error_callback));
}

void BluetoothDeviceBlueZ::Connect(
    BluetoothDevice::PairingDelegate* pairing_delegate,
    const base::Closure& callback,
    const ConnectErrorCallback& error_callback) {
  if (num_connecting_calls_++ == 0)
    adapter()->NotifyDeviceChanged(this);

  BLUETOOTH_LOG(EVENT) << object_path_.value() << ": Connecting, "
                       << num_connecting_calls_ << " in progress";

  if (IsPaired() || !pairing_delegate) {
    BLUETOOTH_LOG(EVENT) << object_path_.value() << ": Pausing discovery";
    // No need to pair, or unable to, skip straight to connection.
    bluez::BluezDBusManager::Get()->GetBluetoothAdapterClient()->PauseDiscovery(
        adapter()->object_path(),
        base::Bind(
            [](base::WeakPtr<BluetoothDeviceBlueZ> weak_ptr,
               const base::Closure& callback,
               const ConnectErrorCallback& error_callback) {
              BLUETOOTH_LOG(EVENT) << "Successfully paused discovery";
              if (weak_ptr)
                weak_ptr->ConnectInternal(false /* after_pairing */, callback,
                                          error_callback);
            },
            weak_ptr_factory_.GetWeakPtr(), callback, error_callback),
        base::Bind(
            [](base::WeakPtr<BluetoothDeviceBlueZ> weak_ptr,
               const base::Closure& callback,
               const ConnectErrorCallback& error_callback,
               const std::string& error_name,
               const std::string& error_message) {
              BLUETOOTH_LOG(EVENT) << "Failed to pause discovery";
              if (weak_ptr)
                weak_ptr->ConnectInternal(false, callback, error_callback);
            },
            weak_ptr_factory_.GetWeakPtr(), callback, error_callback));
  } else {
    // Initiate high-security connection with pairing.
    BeginPairing(pairing_delegate);

    bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->Pair(
        object_path_,
        base::Bind(&BluetoothDeviceBlueZ::OnPairDuringConnect,
                   weak_ptr_factory_.GetWeakPtr(), callback, error_callback),
        base::Bind(&BluetoothDeviceBlueZ::OnPairDuringConnectError,
                   weak_ptr_factory_.GetWeakPtr(), error_callback));
  }
}

void BluetoothDeviceBlueZ::Pair(
    BluetoothDevice::PairingDelegate* pairing_delegate,
    const base::Closure& callback,
    const ConnectErrorCallback& error_callback) {
  DCHECK(pairing_delegate);
  BeginPairing(pairing_delegate);

  bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->Pair(
      object_path_,
      base::Bind(&BluetoothDeviceBlueZ::OnPair, weak_ptr_factory_.GetWeakPtr(),
                 callback),
      base::Bind(&BluetoothDeviceBlueZ::OnPairError,
                 weak_ptr_factory_.GetWeakPtr(), error_callback));
}

void BluetoothDeviceBlueZ::SetPinCode(const std::string& pincode) {
  if (!pairing_.get())
    return;

  pairing_->SetPinCode(pincode);
}

void BluetoothDeviceBlueZ::SetPasskey(uint32_t passkey) {
  if (!pairing_.get())
    return;

  pairing_->SetPasskey(passkey);
}

void BluetoothDeviceBlueZ::ConfirmPairing() {
  if (!pairing_.get())
    return;

  pairing_->ConfirmPairing();
}

void BluetoothDeviceBlueZ::RejectPairing() {
  if (!pairing_.get())
    return;

  pairing_->RejectPairing();
}

void BluetoothDeviceBlueZ::CancelPairing() {
  bool canceled = false;

  BLUETOOTH_LOG(EVENT) << object_path_.value() << ": CancelPairing";

  // If there is a callback in progress that we can reply to then use that
  // to cancel the current pairing request.
  if (pairing_.get() && pairing_->CancelPairing())
    canceled = true;

  // If not we have to send an explicit CancelPairing() to the device instead.
  if (!canceled) {
    BLUETOOTH_LOG(DEBUG) << object_path_.value()
                         << ": No pairing context or callback. "
                         << "Sending explicit cancel";
    bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->CancelPairing(
        object_path_, base::DoNothing(),
        base::Bind(&BluetoothDeviceBlueZ::OnCancelPairingError,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  // Since there is no callback to this method it's possible that the pairing
  // delegate is going to be freed before things complete (indeed it's
  // documented that this is the method you should call while freeing the
  // pairing delegate), so clear our the context holding on to it.
  EndPairing();
}

void BluetoothDeviceBlueZ::Disconnect(const base::Closure& callback,
                                      const ErrorCallback& error_callback) {
  BLUETOOTH_LOG(EVENT) << object_path_.value() << ": Disconnecting";
  bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->Disconnect(
      object_path_,
      base::Bind(&BluetoothDeviceBlueZ::OnDisconnect,
                 weak_ptr_factory_.GetWeakPtr(), callback),
      base::Bind(&BluetoothDeviceBlueZ::OnDisconnectError,
                 weak_ptr_factory_.GetWeakPtr(), error_callback));
}

void BluetoothDeviceBlueZ::Forget(const base::Closure& callback,
                                  const ErrorCallback& error_callback) {
  BLUETOOTH_LOG(EVENT) << object_path_.value() << ": Removing device";
  bluez::BluezDBusManager::Get()->GetBluetoothAdapterClient()->RemoveDevice(
      adapter()->object_path(), object_path_, callback,
      base::Bind(&BluetoothDeviceBlueZ::OnForgetError,
                 weak_ptr_factory_.GetWeakPtr(), error_callback));
}

void BluetoothDeviceBlueZ::ConnectToService(
    const BluetoothUUID& uuid,
    const ConnectToServiceCallback& callback,
    const ConnectToServiceErrorCallback& error_callback) {
  BLUETOOTH_LOG(EVENT) << object_path_.value()
                       << ": Connecting to service: " << uuid.canonical_value();
  scoped_refptr<BluetoothSocketBlueZ> socket =
      BluetoothSocketBlueZ::CreateBluetoothSocket(ui_task_runner_,
                                                  socket_thread_);
  socket->Connect(this, uuid, BluetoothSocketBlueZ::SECURITY_LEVEL_MEDIUM,
                  base::Bind(callback, socket), error_callback);
}

void BluetoothDeviceBlueZ::ConnectToServiceInsecurely(
    const BluetoothUUID& uuid,
    const ConnectToServiceCallback& callback,
    const ConnectToServiceErrorCallback& error_callback) {
  BLUETOOTH_LOG(EVENT) << object_path_.value()
                       << ": Connecting insecurely to service: "
                       << uuid.canonical_value();
  scoped_refptr<BluetoothSocketBlueZ> socket =
      BluetoothSocketBlueZ::CreateBluetoothSocket(ui_task_runner_,
                                                  socket_thread_);
  socket->Connect(this, uuid, BluetoothSocketBlueZ::SECURITY_LEVEL_LOW,
                  base::Bind(callback, socket), error_callback);
}

void BluetoothDeviceBlueZ::CreateGattConnection(
    const GattConnectionCallback& callback,
    const ConnectErrorCallback& error_callback) {
  // TODO(sacomoto): Workaround to retrieve the connection for already connected
  // devices. Currently, BluetoothGattConnection::Disconnect doesn't do
  // anything, the unique underlying physical GATT connection is kept. This
  // should be removed once the correct behavour is implemented and the GATT
  // connections are reference counted (see todo below).
  if (IsConnected()) {
    OnCreateGattConnection(callback);
    return;
  }

  // TODO(crbug.com/630586): Until there is a way to create a reference counted
  // GATT connection in bluetoothd, simply do a regular connect.
  Connect(nullptr,
          base::Bind(&BluetoothDeviceBlueZ::OnCreateGattConnection,
                     weak_ptr_factory_.GetWeakPtr(), callback),
          error_callback);
}

void BluetoothDeviceBlueZ::GetServiceRecords(
    const GetServiceRecordsCallback& callback,
    const GetServiceRecordsErrorCallback& error_callback) {
  bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetServiceRecords(
      object_path_, callback,
      base::Bind(&BluetoothDeviceBlueZ::OnGetServiceRecordsError,
                 weak_ptr_factory_.GetWeakPtr(), error_callback));
}

#if defined(OS_CHROMEOS)
void BluetoothDeviceBlueZ::ExecuteWrite(
    const base::Closure& callback,
    const ExecuteWriteErrorCallback& error_callback) {
  bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->ExecuteWrite(
      object_path_, callback,
      base::Bind(&BluetoothDeviceBlueZ::OnExecuteWriteError,
                 weak_ptr_factory_.GetWeakPtr(), error_callback));
}

void BluetoothDeviceBlueZ::AbortWrite(
    const base::Closure& callback,
    const AbortWriteErrorCallback& error_callback) {
  bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->AbortWrite(
      object_path_, callback,
      base::Bind(&BluetoothDeviceBlueZ::OnAbortWriteError,
                 weak_ptr_factory_.GetWeakPtr(), error_callback));
}
#endif

void BluetoothDeviceBlueZ::UpdateServiceData() {
  bluez::BluetoothDeviceClient::Properties* properties =
      bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path_);
  if (!properties || !properties->service_data.is_valid())
    return;

  service_data_.clear();
  for (const auto& pair : properties->service_data.value())
    service_data_[BluetoothUUID(pair.first)] = pair.second;
}

void BluetoothDeviceBlueZ::UpdateManufacturerData() {
  bluez::BluetoothDeviceClient::Properties* properties =
      bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path_);
  if (!properties || !properties->manufacturer_data.is_valid())
    return;
  manufacturer_data_.clear();

  if (properties->manufacturer_data.is_valid()) {
    for (const auto& pair : properties->manufacturer_data.value())
      manufacturer_data_[pair.first] = pair.second;
  }
}

void BluetoothDeviceBlueZ::UpdateAdvertisingDataFlags() {
  bluez::BluetoothDeviceClient::Properties* properties =
      bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path_);
  if (!properties || !properties->advertising_data_flags.is_valid())
    return;
  // The advertising data flags property is a vector<uint8> because the
  // Supplement to Bluetooth Core Specification Version 6 page 13 said that
  // "The Flags field may be zero or more octets long." However, only the first
  // byte of that is needed because there is only 5 bits of data defined there.
  advertising_data_flags_ = properties->advertising_data_flags.value()[0];
}

BluetoothPairingBlueZ* BluetoothDeviceBlueZ::BeginPairing(
    BluetoothDevice::PairingDelegate* pairing_delegate) {
  pairing_.reset(new BluetoothPairingBlueZ(this, pairing_delegate));
  return pairing_.get();
}

void BluetoothDeviceBlueZ::EndPairing() {
  pairing_.reset();
}

BluetoothPairingBlueZ* BluetoothDeviceBlueZ::GetPairing() const {
  return pairing_.get();
}

BluetoothAdapterBlueZ* BluetoothDeviceBlueZ::adapter() const {
  return static_cast<BluetoothAdapterBlueZ*>(adapter_);
}

void BluetoothDeviceBlueZ::GattServiceAdded(
    const dbus::ObjectPath& object_path) {
  if (GetGattService(object_path.value())) {
    BLUETOOTH_LOG(DEBUG) << "Remote GATT service already exists: "
                         << object_path.value();
    return;
  }

  bluez::BluetoothGattServiceClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothGattServiceClient()
          ->GetProperties(object_path);
  DCHECK(properties);
  if (properties->device.value() != object_path_) {
    BLUETOOTH_LOG(DEBUG)
        << "Remote GATT service does not belong to this device.";
    return;
  }

  BLUETOOTH_LOG(EVENT) << "Adding new remote GATT service for device: "
                       << GetAddress();

  BluetoothRemoteGattServiceBlueZ* service =
      new BluetoothRemoteGattServiceBlueZ(adapter(), this, object_path);

  gatt_services_[service->GetIdentifier()] = base::WrapUnique(service);
  DCHECK(service->object_path() == object_path);
  DCHECK(service->GetUUID().IsValid());

  DCHECK(adapter());
  adapter()->NotifyGattServiceAdded(service);
}

void BluetoothDeviceBlueZ::GattServiceRemoved(
    const dbus::ObjectPath& object_path) {
  auto iter = gatt_services_.find(object_path.value());
  if (iter == gatt_services_.end()) {
    VLOG(3) << "Unknown GATT service removed: " << object_path.value();
    return;
  }

  BluetoothRemoteGattServiceBlueZ* service =
      static_cast<BluetoothRemoteGattServiceBlueZ*>(iter->second.get());

  BLUETOOTH_LOG(EVENT) << "Removing remote GATT service with UUID: '"
                       << service->GetUUID().canonical_value()
                       << "' from device: " << GetAddress();

  DCHECK(service->object_path() == object_path);
  std::unique_ptr<BluetoothRemoteGattService> scoped_service =
      std::move(gatt_services_[object_path.value()]);
  gatt_services_.erase(iter);

  DCHECK(adapter());
  discovery_complete_notified_.erase(service);
  adapter()->NotifyGattServiceRemoved(service);
}

void BluetoothDeviceBlueZ::UpdateGattServices(
    const dbus::ObjectPath& object_path) {
  if (object_path != object_path_) {
    // No need to update map if update is for a different device.
    return;
  }

  DCHECK(IsGattServicesDiscoveryComplete());

  VLOG(3) << "Updating the list of GATT services associated with device "
          << object_path_.value();

  const std::vector<dbus::ObjectPath> service_paths =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothGattServiceClient()
          ->GetServices();
  for (const auto& service_path : service_paths) {
    // Add all previously unknown GATT services associated with the device.
    GattServiceAdded(service_path);

    // If the service does not belong in this device, there is nothing left to
    // do.
    BluetoothRemoteGattService* service = GetGattService(service_path.value());
    if (service == nullptr) {
      return;
    }

    // Notify of GATT discovery complete if we haven't before.
    auto notified_pair = discovery_complete_notified_.insert(service);
    if (notified_pair.second) {
      adapter()->NotifyGattDiscoveryComplete(service);
    }
  }
}

void BluetoothDeviceBlueZ::OnGetConnInfo(const ConnectionInfoCallback& callback,
                                         int16_t rssi,
                                         int16_t transmit_power,
                                         int16_t max_transmit_power) {
  callback.Run(ConnectionInfo(rssi, transmit_power, max_transmit_power));
}

void BluetoothDeviceBlueZ::OnGetConnInfoError(
    const ConnectionInfoCallback& callback,
    const std::string& error_name,
    const std::string& error_message) {
  BLUETOOTH_LOG(ERROR) << object_path_.value()
                       << ": Failed to get connection info: " << error_name
                       << ": " << error_message;
  callback.Run(ConnectionInfo());
}

void BluetoothDeviceBlueZ::OnSetLEConnectionParameters(
    const base::Closure& callback) {
  callback.Run();
}

void BluetoothDeviceBlueZ::OnSetLEConnectionParametersError(
    const ErrorCallback& callback,
    const std::string& error_name,
    const std::string& error_message) {
  BLUETOOTH_LOG(ERROR) << object_path_.value()
                       << ": Failed to set connection parameters: "
                       << error_name << ": " << error_message;
  callback.Run();
}

void BluetoothDeviceBlueZ::OnGetServiceRecordsError(
    const GetServiceRecordsErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  BLUETOOTH_LOG(EVENT) << object_path_.value()
                       << ": Failed to get service records: " << error_name
                       << ": " << error_message;
  BluetoothServiceRecordBlueZ::ErrorCode code =
      BluetoothServiceRecordBlueZ::ErrorCode::UNKNOWN;
  if (error_name == bluetooth_device::kErrorNotConnected) {
    code = BluetoothServiceRecordBlueZ::ErrorCode::ERROR_DEVICE_DISCONNECTED;
  }
  error_callback.Run(code);
}

#if defined(OS_CHROMEOS)
void BluetoothDeviceBlueZ::OnExecuteWriteError(
    const ExecuteWriteErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  BLUETOOTH_LOG(EVENT) << object_path_.value()
                       << ": Failed to execute write: " << error_name << ": "
                       << error_message;
  error_callback.Run(
      BluetoothGattServiceBlueZ::DBusErrorToServiceError(error_name));
}

void BluetoothDeviceBlueZ::OnAbortWriteError(
    const AbortWriteErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  BLUETOOTH_LOG(EVENT) << object_path_.value()
                       << ": Failed to abort write: " << error_name << ": "
                       << error_message;
  error_callback.Run(
      BluetoothGattServiceBlueZ::DBusErrorToServiceError(error_name));
}
#endif

void BluetoothDeviceBlueZ::ConnectInternal(
    bool after_pairing,
    const base::Closure& callback,
    const ConnectErrorCallback& error_callback) {
  BLUETOOTH_LOG(EVENT) << object_path_.value() << ": Connecting";
  bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->Connect(
      object_path_,
      base::Bind(&BluetoothDeviceBlueZ::OnConnect,
                 weak_ptr_factory_.GetWeakPtr(), after_pairing, callback),
      base::Bind(&BluetoothDeviceBlueZ::OnConnectError,
                 weak_ptr_factory_.GetWeakPtr(), after_pairing,
                 error_callback));
}

void BluetoothDeviceBlueZ::OnConnect(bool after_pairing,
                                     const base::Closure& callback) {
  BLUETOOTH_LOG(EVENT) << object_path_.value()
                       << ": Unpausing discovery after connection";
  UnpauseDiscovery();
  if (--num_connecting_calls_ == 0)
    adapter()->NotifyDeviceChanged(this);

  DCHECK(num_connecting_calls_ >= 0);
  BLUETOOTH_LOG(EVENT) << object_path_.value() << ": Connected, "
                       << num_connecting_calls_ << " still in progress";

  SetTrusted();

  if (after_pairing)
    UMA_HISTOGRAM_ENUMERATION("Bluetooth.PairingResult",
                              UMA_PAIRING_RESULT_SUCCESS,
                              UMA_PAIRING_RESULT_COUNT);

  callback.Run();
}

void BluetoothDeviceBlueZ::OnCreateGattConnection(
    const GattConnectionCallback& callback) {
  std::unique_ptr<device::BluetoothGattConnection> conn(
      new BluetoothGattConnectionBlueZ(adapter_, GetAddress(), object_path_));
  callback.Run(std::move(conn));
}

void BluetoothDeviceBlueZ::OnConnectError(
    bool after_pairing,
    const ConnectErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  BLUETOOTH_LOG(EVENT) << object_path_.value()
                       << ": Unpausing discovery after failed connection";
  UnpauseDiscovery();

  if (--num_connecting_calls_ == 0)
    adapter()->NotifyDeviceChanged(this);

  DCHECK(num_connecting_calls_ >= 0);
  BLUETOOTH_LOG(ERROR) << object_path_.value()
                       << ": Failed to connect device: " << error_name << ": "
                       << error_message;
  BLUETOOTH_LOG(DEBUG) << object_path_.value() << ": " << num_connecting_calls_
                       << " still in progress";

  // Determine the error code from error_name.
  ConnectErrorCode error_code = ERROR_UNKNOWN;
  if (error_name == bluetooth_device::kErrorFailed) {
    error_code = ERROR_FAILED;
  } else if (error_name == bluetooth_device::kErrorInProgress) {
    error_code = ERROR_INPROGRESS;
  } else if (error_name == bluetooth_device::kErrorNotSupported) {
    error_code = ERROR_UNSUPPORTED_DEVICE;
  }

  if (after_pairing)
    RecordPairingResult(error_code);
  error_callback.Run(error_code);
}

void BluetoothDeviceBlueZ::OnPairDuringConnect(
    const base::Closure& callback,
    const ConnectErrorCallback& error_callback) {
  BLUETOOTH_LOG(EVENT) << object_path_.value() << ": Paired";

  EndPairing();

  ConnectInternal(true, callback, error_callback);
}

void BluetoothDeviceBlueZ::OnPairDuringConnectError(
    const ConnectErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  if (--num_connecting_calls_ == 0)
    adapter()->NotifyDeviceChanged(this);

  DCHECK(num_connecting_calls_ >= 0);
  BLUETOOTH_LOG(ERROR) << object_path_.value()
                       << ": Failed to pair device: " << error_name << ": "
                       << error_message;
  BLUETOOTH_LOG(DEBUG) << object_path_.value() << ": " << num_connecting_calls_
                       << " still in progress";

  EndPairing();

  // Determine the error code from error_name.
  ConnectErrorCode error_code = DBusErrorToConnectError(error_name);

  RecordPairingResult(error_code);
  error_callback.Run(error_code);
}

void BluetoothDeviceBlueZ::OnPair(const base::Closure& callback) {
  BLUETOOTH_LOG(EVENT) << object_path_.value() << ": Paired";
  EndPairing();
  callback.Run();
}

void BluetoothDeviceBlueZ::OnPairError(
    const ConnectErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  BLUETOOTH_LOG(ERROR) << object_path_.value()
                       << ": Failed to pair device: " << error_name << ": "
                       << error_message;
  EndPairing();
  ConnectErrorCode error_code = DBusErrorToConnectError(error_name);
  RecordPairingResult(error_code);
  error_callback.Run(error_code);
}

void BluetoothDeviceBlueZ::OnCancelPairingError(
    const std::string& error_name,
    const std::string& error_message) {
  BLUETOOTH_LOG(ERROR) << object_path_.value()
                       << ": Failed to cancel pairing: " << error_name << ": "
                       << error_message;
}

void BluetoothDeviceBlueZ::SetTrusted() {
  // Unconditionally send the property change, rather than checking the value
  // first; there's no harm in doing this and it solves any race conditions
  // with the property becoming true or false and this call happening before
  // we get the D-Bus signal about the earlier change.
  bluez::BluezDBusManager::Get()
      ->GetBluetoothDeviceClient()
      ->GetProperties(object_path_)
      ->trusted.Set(true, base::Bind(&BluetoothDeviceBlueZ::OnSetTrusted,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothDeviceBlueZ::OnSetTrusted(bool success) {
  device_event_log::LogLevel log_level =
      success ? device_event_log::LOG_LEVEL_DEBUG
              : device_event_log::LOG_LEVEL_ERROR;
  DEVICE_LOG(device_event_log::LOG_TYPE_BLUETOOTH, log_level)
      << object_path_.value() << ": OnSetTrusted: " << success;
}

void BluetoothDeviceBlueZ::OnDisconnect(const base::Closure& callback) {
  BLUETOOTH_LOG(EVENT) << object_path_.value() << ": Disconnected";
  callback.Run();
}

void BluetoothDeviceBlueZ::OnDisconnectError(
    const ErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  BLUETOOTH_LOG(ERROR) << object_path_.value()
                       << ": Failed to disconnect device: " << error_name
                       << ": " << error_message;
  error_callback.Run();
}

void BluetoothDeviceBlueZ::OnForgetError(const ErrorCallback& error_callback,
                                         const std::string& error_name,
                                         const std::string& error_message) {
  BLUETOOTH_LOG(ERROR) << object_path_.value()
                       << ": Failed to remove device: " << error_name << ": "
                       << error_message;
  error_callback.Run();
}

void BluetoothDeviceBlueZ::UnpauseDiscovery() {
  bluez::BluezDBusManager::Get()->GetBluetoothAdapterClient()->UnpauseDiscovery(
      adapter()->object_path(), base::Bind([]() {
        BLUETOOTH_LOG(EVENT) << "Successfully un-paused discovery";
      }),
      base::Bind(
          [](const std::string& error_name, const std::string& error_message) {
            BLUETOOTH_LOG(EVENT) << "Failed to un-pause discovery";
          }));
}

}  // namespace bluez
