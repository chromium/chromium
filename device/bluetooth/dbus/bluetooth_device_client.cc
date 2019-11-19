// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_device_client.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_manager.h"
#include "dbus/object_proxy.h"
#include "device/bluetooth/bluez/bluetooth_service_attribute_value_bluez.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

namespace {

// Value returned for the the RSSI or TX power if it cannot be read.
const int kUnknownPower = 127;

std::unique_ptr<BluetoothServiceAttributeValueBlueZ> ReadAttributeValue(
    dbus::MessageReader* struct_reader) {
  uint8_t type_val;
  if (!struct_reader->PopByte(&type_val))
    return nullptr;
  BluetoothServiceAttributeValueBlueZ::Type type =
      static_cast<BluetoothServiceAttributeValueBlueZ::Type>(type_val);

  uint32_t size;
  if (!struct_reader->PopUint32(&size))
    return nullptr;

  std::unique_ptr<base::Value> value = nullptr;
  switch (type) {
    case bluez::BluetoothServiceAttributeValueBlueZ::NULLTYPE: {
      break;
    }
    case bluez::BluetoothServiceAttributeValueBlueZ::UINT:
    // Fall through.
    case bluez::BluetoothServiceAttributeValueBlueZ::INT: {
      switch (size) {
        // It doesn't matter what 'sign' the number is, only size.
        // Whenever we unpack this value, we will take the raw bits and
        // cast it back to the correct sign anyway.
        case 1:
          uint8_t byte;
          if (!struct_reader->PopVariantOfByte(&byte))
            return nullptr;
          value = std::make_unique<base::Value>(byte);
          break;
        case 2:
          uint16_t short_val;
          if (!struct_reader->PopVariantOfUint16(&short_val))
            return nullptr;
          value = std::make_unique<base::Value>(short_val);
          break;
        case 4:
          uint32_t val;
          if (!struct_reader->PopVariantOfUint32(&val))
            return nullptr;
          value = std::make_unique<base::Value>(static_cast<int32_t>(val));
          break;
        case 8:
        // Fall through.
        // BlueZ should never be sending us this size at the moment since
        // the Android SDP records we will create from these raw records
        // don't have any fields which use this size. If we ever decide to
        // change this, this needs to get fixed.
        default:
          NOTREACHED();
      }
      break;
    }
    case bluez::BluetoothServiceAttributeValueBlueZ::UUID:
    // Fall through.
    case bluez::BluetoothServiceAttributeValueBlueZ::STRING:
    // Fall through.
    case bluez::BluetoothServiceAttributeValueBlueZ::URL: {
      std::string str;
      if (!struct_reader->PopVariantOfString(&str))
        return nullptr;
      value = std::make_unique<base::Value>(str);
      break;
    }
    case bluez::BluetoothServiceAttributeValueBlueZ::BOOL: {
      bool b;
      if (!struct_reader->PopVariantOfBool(&b))
        return nullptr;
      value = std::make_unique<base::Value>(b);
      break;
    }
    case bluez::BluetoothServiceAttributeValueBlueZ::SEQUENCE: {
      dbus::MessageReader variant_reader(nullptr);
      if (!struct_reader->PopVariant(&variant_reader))
        return nullptr;
      dbus::MessageReader array_reader(nullptr);
      if (!variant_reader.PopArray(&array_reader))
        return nullptr;
      std::unique_ptr<BluetoothServiceAttributeValueBlueZ::Sequence> sequence =
          std::make_unique<BluetoothServiceAttributeValueBlueZ::Sequence>();
      while (array_reader.HasMoreData()) {
        dbus::MessageReader sequence_element_struct_reader(nullptr);
        if (!array_reader.PopStruct(&sequence_element_struct_reader))
          return nullptr;
        std::unique_ptr<BluetoothServiceAttributeValueBlueZ> attribute_value =
            ReadAttributeValue(&sequence_element_struct_reader);
        if (!attribute_value)
          return nullptr;
        sequence->emplace_back(*attribute_value);
      }
      return std::make_unique<BluetoothServiceAttributeValueBlueZ>(
          std::move(sequence));
    }
  }
  return std::make_unique<BluetoothServiceAttributeValueBlueZ>(
      type, size, std::move(value));
}

std::unique_ptr<BluetoothServiceRecordBlueZ> ReadRecord(
    dbus::MessageReader* array_reader) {
  std::unique_ptr<BluetoothServiceRecordBlueZ> record =
      std::make_unique<BluetoothServiceRecordBlueZ>();
  while (array_reader->HasMoreData()) {
    dbus::MessageReader dict_entry_reader(nullptr);
    if (!array_reader->PopDictEntry(&dict_entry_reader))
      return nullptr;
    uint16_t id;
    if (!dict_entry_reader.PopUint16(&id))
      return nullptr;
    dbus::MessageReader struct_reader(nullptr);
    if (!dict_entry_reader.PopStruct(&struct_reader))
      return nullptr;
    std::unique_ptr<BluetoothServiceAttributeValueBlueZ> attribute_value =
        ReadAttributeValue(&struct_reader);
    if (!attribute_value)
      return nullptr;
    record->AddRecordEntry(id, *attribute_value);
  }
  //  return std::move(record);
  return record;
}

bool ReadRecordsFromMessage(dbus::MessageReader* reader,
                            BluetoothDeviceClient::ServiceRecordList* records) {
  dbus::MessageReader array_reader(nullptr);
  if (!reader->PopArray(&array_reader)) {
    return false;
    LOG(ERROR) << "Arguments for GetConnInfo invalid.";
  }
  while (array_reader.HasMoreData()) {
    dbus::MessageReader nested_array_reader(nullptr);
    if (!array_reader.PopArray(&nested_array_reader))
      return false;
    std::unique_ptr<BluetoothServiceRecordBlueZ> record =
        ReadRecord(&nested_array_reader);
    if (!record)
      return false;
    records->emplace_back(*record);
  }
  return true;
}

}  // namespace

const char BluetoothDeviceClient::kNoResponseError[] =
    "org.chromium.Error.NoResponse";
const char BluetoothDeviceClient::kUnknownDeviceError[] =
    "org.chromium.Error.UnknownDevice";

const char BluetoothDeviceClient::kTypeBredr[] = "BR/EDR";
const char BluetoothDeviceClient::kTypeLe[] = "LE";
const char BluetoothDeviceClient::kTypeDual[] = "DUAL";

BluetoothDeviceClient::Properties::Properties(
    dbus::ObjectProxy* object_proxy,
    const std::string& interface_name,
    const PropertyChangedCallback& callback)
    : dbus::PropertySet(object_proxy, interface_name, callback) {
  RegisterProperty(bluetooth_device::kAddressProperty, &address);
  RegisterProperty(bluetooth_device::kNameProperty, &name);
  RegisterProperty(bluetooth_device::kIconProperty, &icon);
  RegisterProperty(bluetooth_device::kClassProperty, &bluetooth_class);
  RegisterProperty(bluetooth_device::kTypeProperty, &type);
  RegisterProperty(bluetooth_device::kAppearanceProperty, &appearance);
  RegisterProperty(bluetooth_device::kUUIDsProperty, &uuids);
  RegisterProperty(bluetooth_device::kPairedProperty, &paired);
  RegisterProperty(bluetooth_device::kConnectedProperty, &connected);
  RegisterProperty(bluetooth_device::kTrustedProperty, &trusted);
  RegisterProperty(bluetooth_device::kBlockedProperty, &blocked);
  RegisterProperty(bluetooth_device::kAliasProperty, &alias);
  RegisterProperty(bluetooth_device::kAdapterProperty, &adapter);
  RegisterProperty(bluetooth_device::kLegacyPairingProperty, &legacy_pairing);
  RegisterProperty(bluetooth_device::kModaliasProperty, &modalias);
  RegisterProperty(bluetooth_device::kRSSIProperty, &rssi);
  RegisterProperty(bluetooth_device::kTxPowerProperty, &tx_power);
  RegisterProperty(bluetooth_device::kManufacturerDataProperty,
                   &manufacturer_data);
  RegisterProperty(bluetooth_device::kServiceDataProperty, &service_data);
  RegisterProperty(bluetooth_device::kServicesResolvedProperty,
                   &services_resolved);
  RegisterProperty(bluetooth_device::kAdvertisingDataFlagsProperty,
                   &advertising_data_flags);
  RegisterProperty(bluetooth_device::kMTUProperty, &mtu);
  RegisterProperty(bluetooth_device::kEIRProperty, &eir);
}

BluetoothDeviceClient::Properties::~Properties() = default;

// The BluetoothDeviceClient implementation used in production.
class BluetoothDeviceClientImpl : public BluetoothDeviceClient,
                                  public dbus::ObjectManager::Interface {
 public:
  BluetoothDeviceClientImpl() : object_manager_(nullptr) {}

  ~BluetoothDeviceClientImpl() override {
    // There is an instance of this client that is created but not initialized
    // on Linux. See 'Alternate D-Bus Client' note in bluez_dbus_manager.h.
    if (object_manager_) {
      object_manager_->UnregisterInterface(
          bluetooth_adapter::kBluetoothAdapterInterface);
    }
  }

  // BluetoothDeviceClient override.
  void AddObserver(BluetoothDeviceClient::Observer* observer) override {
    DCHECK(observer);
    observers_.AddObserver(observer);
  }

  // BluetoothDeviceClient override.
  void RemoveObserver(BluetoothDeviceClient::Observer* observer) override {
    DCHECK(observer);
    observers_.RemoveObserver(observer);
  }

  // dbus::ObjectManager::Interface override.
  dbus::PropertySet* CreateProperties(
      dbus::ObjectProxy* object_proxy,
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) override {
    Properties* properties =
        new Properties(object_proxy, interface_name,
                       base::Bind(&BluetoothDeviceClientImpl::OnPropertyChanged,
                                  weak_ptr_factory_.GetWeakPtr(), object_path));
    return static_cast<dbus::PropertySet*>(properties);
  }

  // BluetoothDeviceClient override.
  std::vector<dbus::ObjectPath> GetDevicesForAdapter(
      const dbus::ObjectPath& adapter_path) override {
    std::vector<dbus::ObjectPath> object_paths, device_paths;
    device_paths = object_manager_->GetObjectsWithInterface(
        bluetooth_device::kBluetoothDeviceInterface);
    for (auto iter = device_paths.begin(); iter != device_paths.end(); ++iter) {
      Properties* properties = GetProperties(*iter);
      if (properties->adapter.value() == adapter_path)
        object_paths.push_back(*iter);
    }
    return object_paths;
  }

  // BluetoothDeviceClient override.
  Properties* GetProperties(const dbus::ObjectPath& object_path) override {
    return static_cast<Properties*>(object_manager_->GetProperties(
        object_path, bluetooth_device::kBluetoothDeviceInterface));
  }

  // BluetoothDeviceClient override.
  void Connect(const dbus::ObjectPath& object_path,
               base::OnceClosure callback,
               ErrorCallback error_callback) override {
    dbus::MethodCall method_call(bluetooth_device::kBluetoothDeviceInterface,
                                 bluetooth_device::kConnect);

    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(object_path);
    if (!object_proxy) {
      std::move(error_callback).Run(kUnknownDeviceError, "");
      return;
    }

    // Connect may take an arbitrary length of time, so use no timeout.
    object_proxy->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_INFINITE,
        base::BindOnce(&BluetoothDeviceClientImpl::OnSuccess,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        base::BindOnce(&BluetoothDeviceClientImpl::OnError,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(error_callback)));
  }

  // BluetoothDeviceClient override.
  void Disconnect(const dbus::ObjectPath& object_path,
                  base::OnceClosure callback,
                  ErrorCallback error_callback) override {
    dbus::MethodCall method_call(bluetooth_device::kBluetoothDeviceInterface,
                                 bluetooth_device::kDisconnect);

    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(object_path);
    if (!object_proxy) {
      std::move(error_callback).Run(kUnknownDeviceError, "");
      return;
    }

    object_proxy->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&BluetoothDeviceClientImpl::OnSuccess,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        base::BindOnce(&BluetoothDeviceClientImpl::OnError,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(error_callback)));
  }

  // BluetoothDeviceClient override.
  void ConnectProfile(const dbus::ObjectPath& object_path,
                      const std::string& uuid,
                      base::OnceClosure callback,
                      ErrorCallback error_callback) override {
    dbus::MethodCall method_call(bluetooth_device::kBluetoothDeviceInterface,
                                 bluetooth_device::kConnectProfile);

    dbus::MessageWriter writer(&method_call);
    writer.AppendString(uuid);

    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(object_path);
    if (!object_proxy) {
      std::move(error_callback).Run(kUnknownDeviceError, "");
      return;
    }

    // Connect may take an arbitrary length of time, so use no timeout.
    object_proxy->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_INFINITE,
        base::BindOnce(&BluetoothDeviceClientImpl::OnSuccess,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        base::BindOnce(&BluetoothDeviceClientImpl::OnError,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(error_callback)));
  }

  // BluetoothDeviceClient override.
  void DisconnectProfile(const dbus::ObjectPath& object_path,
                         const std::string& uuid,
                         base::OnceClosure callback,
                         ErrorCallback error_callback) override {
    dbus::MethodCall method_call(bluetooth_device::kBluetoothDeviceInterface,
                                 bluetooth_device::kDisconnectProfile);

    dbus::MessageWriter writer(&method_call);
    writer.AppendString(uuid);

    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(object_path);
    if (!object_proxy) {
      std::move(error_callback).Run(kUnknownDeviceError, "");
      return;
    }

    object_proxy->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&BluetoothDeviceClientImpl::OnSuccess,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        base::BindOnce(&BluetoothDeviceClientImpl::OnError,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(error_callback)));
  }

  // BluetoothDeviceClient override.
  void Pair(const dbus::ObjectPath& object_path,
            base::OnceClosure callback,
            ErrorCallback error_callback) override {
    dbus::MethodCall method_call(bluetooth_device::kBluetoothDeviceInterface,
                                 bluetooth_device::kPair);

    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(object_path);
    if (!object_proxy) {
      std::move(error_callback).Run(kUnknownDeviceError, "");
      return;
    }

    // Pairing may take an arbitrary length of time, so use no timeout.
    object_proxy->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_INFINITE,
        base::BindOnce(&BluetoothDeviceClientImpl::OnSuccess,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        base::BindOnce(&BluetoothDeviceClientImpl::OnError,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(error_callback)));
  }

  // BluetoothDeviceClient override.
  void CancelPairing(const dbus::ObjectPath& object_path,
                     base::OnceClosure callback,
                     ErrorCallback error_callback) override {
    dbus::MethodCall method_call(bluetooth_device::kBluetoothDeviceInterface,
                                 bluetooth_device::kCancelPairing);

    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(object_path);
    if (!object_proxy) {
      std::move(error_callback).Run(kUnknownDeviceError, "");
      return;
    }
    object_proxy->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&BluetoothDeviceClientImpl::OnSuccess,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        base::BindOnce(&BluetoothDeviceClientImpl::OnError,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(error_callback)));
  }

  // BluetoothDeviceClient override.
  void GetConnInfo(const dbus::ObjectPath& object_path,
                   ConnInfoCallback callback,
                   ErrorCallback error_callback) override {
    dbus::MethodCall method_call(
        bluetooth_plugin_device::kBluetoothPluginInterface,
        bluetooth_plugin_device::kGetConnInfo);

    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(object_path);
    if (!object_proxy) {
      std::move(error_callback).Run(kUnknownDeviceError, "");
      return;
    }
    object_proxy->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&BluetoothDeviceClientImpl::OnGetConnInfoSuccess,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        base::BindOnce(&BluetoothDeviceClientImpl::OnError,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(error_callback)));
  }

  void SetLEConnectionParameters(const dbus::ObjectPath& object_path,
                                 const ConnectionParameters& conn_params,
                                 base::OnceClosure callback,
                                 ErrorCallback error_callback) override {
    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(object_path);
    if (!object_proxy) {
      std::move(error_callback).Run(kUnknownDeviceError, "");
      return;
    }

    dbus::MethodCall method_call(
        bluetooth_plugin_device::kBluetoothPluginInterface,
        bluetooth_plugin_device::kSetLEConnectionParameters);

    dbus::MessageWriter writer(&method_call);
    dbus::MessageWriter dict_writer(nullptr);
    writer.OpenArray("{sq}", &dict_writer);

    {
      dbus::MessageWriter dict_entry_writer(nullptr);
      dict_writer.OpenDictEntry(&dict_entry_writer);
      dict_entry_writer.AppendString(
          bluetooth_plugin_device::
              kLEConnectionParameterMinimumConnectionInterval);
      dict_entry_writer.AppendUint16(conn_params.min_connection_interval);
      dict_writer.CloseContainer(&dict_entry_writer);
    }

    {
      dbus::MessageWriter dict_entry_writer(nullptr);
      dict_writer.OpenDictEntry(&dict_entry_writer);
      dict_entry_writer.AppendString(
          bluetooth_plugin_device::
              kLEConnectionParameterMaximumConnectionInterval);
      dict_entry_writer.AppendUint16(conn_params.max_connection_interval);
      dict_writer.CloseContainer(&dict_entry_writer);
    }

    writer.CloseContainer(&dict_writer);

    object_proxy->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&BluetoothDeviceClientImpl::OnSuccess,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        base::BindOnce(&BluetoothDeviceClientImpl::OnError,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(error_callback)));
  }

  void GetServiceRecords(const dbus::ObjectPath& object_path,
                         ServiceRecordsCallback callback,
                         ErrorCallback error_callback) override {
    dbus::MethodCall method_call(bluetooth_device::kBluetoothDeviceInterface,
                                 bluetooth_device::kGetServiceRecords);

    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(object_path);
    if (!object_proxy) {
      std::move(error_callback).Run(kUnknownDeviceError, "");
      return;
    }
    object_proxy->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&BluetoothDeviceClientImpl::OnGetServiceRecordsSuccess,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        base::BindOnce(&BluetoothDeviceClientImpl::OnError,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(error_callback)));
  }

  void ExecuteWrite(const dbus::ObjectPath& object_path,
                    base::OnceClosure callback,
                    ErrorCallback error_callback) override {
    dbus::MethodCall method_call(bluetooth_device::kBluetoothDeviceInterface,
                                 bluetooth_device::kExecuteWrite);

    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(object_path);
    if (!object_proxy) {
      std::move(error_callback).Run(kUnknownDeviceError, "");
      return;
    }

    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(true);
    object_proxy->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&BluetoothDeviceClientImpl::OnSuccess,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        base::BindOnce(&BluetoothDeviceClientImpl::OnError,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(error_callback)));
  }

  void AbortWrite(const dbus::ObjectPath& object_path,
                  base::OnceClosure callback,
                  ErrorCallback error_callback) override {
    dbus::MethodCall method_call(bluetooth_device::kBluetoothDeviceInterface,
                                 bluetooth_device::kExecuteWrite);

    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(object_path);
    if (!object_proxy) {
      std::move(error_callback).Run(kUnknownDeviceError, "");
      return;
    }

    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(false);
    object_proxy->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&BluetoothDeviceClientImpl::OnSuccess,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        base::BindOnce(&BluetoothDeviceClientImpl::OnError,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(error_callback)));
  }

 protected:
  void Init(dbus::Bus* bus,
            const std::string& bluetooth_service_name) override {
    object_manager_ = bus->GetObjectManager(
        bluetooth_service_name,
        dbus::ObjectPath(
            bluetooth_object_manager::kBluetoothObjectManagerServicePath));
    object_manager_->RegisterInterface(
        bluetooth_device::kBluetoothDeviceInterface, this);
  }

 private:
  // Called by dbus::ObjectManager when an object with the device interface
  // is created. Informs observers.
  void ObjectAdded(const dbus::ObjectPath& object_path,
                   const std::string& interface_name) override {
    for (auto& observer : observers_)
      observer.DeviceAdded(object_path);
  }

  // Called by dbus::ObjectManager when an object with the device interface
  // is removed. Informs observers.
  void ObjectRemoved(const dbus::ObjectPath& object_path,
                     const std::string& interface_name) override {
    for (auto& observer : observers_)
      observer.DeviceRemoved(object_path);
  }

  // Called by BluetoothPropertySet when a property value is changed,
  // either by result of a signal or response to a GetAll() or Get()
  // call. Informs observers.
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name) {
    for (auto& observer : observers_)
      observer.DevicePropertyChanged(object_path, property_name);
  }

  // Called when a response for successful method call is received.
  void OnSuccess(base::OnceClosure callback, dbus::Response* response) {
    DCHECK(response);
    std::move(callback).Run();
  }

  // Called when a response for the GetConnInfo method is received.
  void OnGetConnInfoSuccess(ConnInfoCallback callback,
                            dbus::Response* response) {
    int16_t rssi = kUnknownPower;
    int16_t transmit_power = kUnknownPower;
    int16_t max_transmit_power = kUnknownPower;

    if (!response) {
      LOG(ERROR) << "GetConnInfo succeeded, but no response received.";
      std::move(callback).Run(rssi, transmit_power, max_transmit_power);
      return;
    }

    dbus::MessageReader reader(response);
    if (!reader.PopInt16(&rssi) || !reader.PopInt16(&transmit_power) ||
        !reader.PopInt16(&max_transmit_power)) {
      LOG(ERROR) << "Arguments for GetConnInfo invalid.";
    }
    std::move(callback).Run(rssi, transmit_power, max_transmit_power);
  }

  void OnGetServiceRecordsSuccess(ServiceRecordsCallback callback,
                                  dbus::Response* response) {
    ServiceRecordList records;
    if (!response) {
      LOG(ERROR) << "GetServiceRecords succeeded, but no response received.";
      std::move(callback).Run(records);
      return;
    }

    dbus::MessageReader reader(response);
    if (!ReadRecordsFromMessage(&reader, &records)) {
      std::move(callback).Run(ServiceRecordList());
    }

    std::move(callback).Run(records);
  }

  // Called when a response for a failed method call is received.
  void OnError(ErrorCallback error_callback, dbus::ErrorResponse* response) {
    // Error response has optional error message argument.
    std::string error_name;
    std::string error_message;
    if (response) {
      dbus::MessageReader reader(response);
      error_name = response->GetErrorName();
      reader.PopString(&error_message);
    } else {
      error_name = kNoResponseError;
      error_message = "";
    }
    std::move(error_callback).Run(error_name, error_message);
  }

  dbus::ObjectManager* object_manager_;

  // List of observers interested in event notifications from us.
  base::ObserverList<BluetoothDeviceClient::Observer>::Unchecked observers_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothDeviceClientImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BluetoothDeviceClientImpl);
};

BluetoothDeviceClient::BluetoothDeviceClient() = default;

BluetoothDeviceClient::~BluetoothDeviceClient() = default;

BluetoothDeviceClient* BluetoothDeviceClient::Create() {
  return new BluetoothDeviceClientImpl();
}

}  // namespace bluez
