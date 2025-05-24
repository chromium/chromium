// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_adapter_client.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_manager.h"
#include "dbus/object_proxy.h"
#include "dbus/values_util.h"
#include "device/bluetooth/bluez/bluetooth_service_attribute_value_bluez.h"
#include "device/bluetooth/bluez/bluetooth_service_record_bluez.h"
#include "device/bluetooth/dbus/bluetooth_metrics_helper.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

// Automatically determine transport mode.
constexpr char kBluezAutoTransport[] = "auto";

constexpr char kBluezAddressTypePublic[] = "public";
constexpr char kBluezAddressTypeRandom[] = "random";

namespace {

// TODO(rkc) Find better way to do this.
void WriteNumberAttribute(dbus::MessageWriter* writer,
                          const BluetoothServiceAttributeValueBlueZ& attribute,
                          bool is_signed) {
  int value = attribute.value().GetInt();

  switch (attribute.size()) {
    case 1:
      if (is_signed)
        writer->AppendVariantOfByte(static_cast<int8_t>(value));
      else
        writer->AppendVariantOfByte(static_cast<uint8_t>(value));
      break;
    case 2:
      if (is_signed)
        writer->AppendVariantOfInt16(static_cast<int16_t>(value));
      else
        writer->AppendVariantOfUint16(static_cast<uint16_t>(value));
      break;
    case 4:
      if (is_signed)
        writer->AppendVariantOfInt32(static_cast<int32_t>(value));
      else
        writer->AppendVariantOfUint32(static_cast<uint32_t>(value));
      break;
    default:
      NOTREACHED();
  }
}

void WriteAttribute(dbus::MessageWriter* writer,
                    const BluetoothServiceAttributeValueBlueZ& attribute) {
  dbus::MessageWriter struct_writer(nullptr);
  writer->OpenStruct(&struct_writer);
  struct_writer.AppendByte(attribute.type());
  struct_writer.AppendUint32(attribute.size());

  switch (attribute.type()) {
    case bluez::BluetoothServiceAttributeValueBlueZ::UINT:
      WriteNumberAttribute(&struct_writer, attribute, false);
      break;
    case bluez::BluetoothServiceAttributeValueBlueZ::INT:
      WriteNumberAttribute(&struct_writer, attribute, true);
      break;
    case bluez::BluetoothServiceAttributeValueBlueZ::BOOL:
    case bluez::BluetoothServiceAttributeValueBlueZ::UUID:
    case bluez::BluetoothServiceAttributeValueBlueZ::STRING:
    case bluez::BluetoothServiceAttributeValueBlueZ::URL:
      dbus::AppendValueDataAsVariant(&struct_writer, attribute.value());
      break;
    case BluetoothServiceAttributeValueBlueZ::SEQUENCE: {
      dbus::MessageWriter variant_writer(nullptr);
      dbus::MessageWriter array_writer(nullptr);
      struct_writer.OpenVariant("a(yuv)", &variant_writer);
      variant_writer.OpenArray("(yuv)", &array_writer);

      for (const auto& v : attribute.sequence())
        WriteAttribute(&array_writer, v);
      variant_writer.CloseContainer(&array_writer);
      struct_writer.CloseContainer(&variant_writer);
      break;
    }
    case bluez::BluetoothServiceAttributeValueBlueZ::NULLTYPE:
    default:
      NOTREACHED();
  }
  writer->CloseContainer(&struct_writer);
}

BluetoothAdapterClient::Error ErrorResponseToError(
    dbus::ErrorResponse* response) {
  BluetoothAdapterClient::Error error(BluetoothAdapterClient::kNoResponseError,
                                      "");
  if (response) {
    dbus::MessageReader reader(response);
    error.name = response->GetErrorName();
    reader.PopString(&error.message);
  }

  return error;
}

void OnResponseAdapter(
    base::OnceClosure callback,
    BluetoothAdapterClient::ErrorCallback error_callback,
    const std::optional<BluetoothAdapterClient::Error>& error) {
  if (!error) {
    std::move(callback).Run();
    return;
  }

  std::move(error_callback).Run(error->name, error->message);
}

}  // namespace

BluetoothAdapterClient::DiscoveryFilter::DiscoveryFilter() = default;

BluetoothAdapterClient::DiscoveryFilter::~DiscoveryFilter() = default;

void BluetoothAdapterClient::DiscoveryFilter::CopyFrom(
    const DiscoveryFilter& filter) {
  if (filter.rssi.get())
    rssi = std::make_unique<int16_t>(*filter.rssi);
  else
    rssi.reset();

  if (filter.pathloss.get())
    pathloss = std::make_unique<uint16_t>(*filter.pathloss);
  else
    pathloss.reset();

  if (filter.transport.get())
    transport = std::make_unique<std::string>(*filter.transport);
  else
    transport = std::make_unique<std::string>(kBluezAutoTransport);

  if (filter.uuids.get())
    uuids = std::make_unique<std::vector<std::string>>(*filter.uuids);
  else
    uuids.reset();
}

BluetoothAdapterClient::Error::Error(const std::string& name,
                                     const std::string& message)
    : name(name), message(message) {}

const char BluetoothAdapterClient::kNoResponseError[] =
    "org.chromium.Error.NoResponse";
const char BluetoothAdapterClient::kUnknownAdapterError[] =
    "org.chromium.Error.UnknownAdapter";

BluetoothAdapterClient::Properties::Properties(
    dbus::ObjectProxy* object_proxy,
    const std::string& interface_name,
    const PropertyChangedCallback& callback)
    : dbus::PropertySet(object_proxy, interface_name, callback) {
  RegisterProperty(bluetooth_adapter::kAddressProperty, &address);
  RegisterProperty(bluetooth_adapter::kNameProperty, &name);
  RegisterProperty(bluetooth_adapter::kAliasProperty, &alias);
  RegisterProperty(bluetooth_adapter::kClassProperty, &bluetooth_class);
  RegisterProperty(bluetooth_adapter::kPoweredProperty, &powered);
  RegisterProperty(bluetooth_adapter::kDiscoverableProperty, &discoverable);
  RegisterProperty(bluetooth_adapter::kPairableProperty, &pairable);
  RegisterProperty(bluetooth_adapter::kPairableTimeoutProperty,
                   &pairable_timeout);
  RegisterProperty(bluetooth_adapter::kDiscoverableTimeoutProperty,
                   &discoverable_timeout);
  RegisterProperty(bluetooth_adapter::kDiscoveringProperty, &discovering);
  RegisterProperty(bluetooth_adapter::kUUIDsProperty, &uuids);
  RegisterProperty(bluetooth_adapter::kModaliasProperty, &modalias);
  RegisterProperty(bluetooth_adapter::kRolesProperty, &roles);
}

BluetoothAdapterClient::Properties::~Properties() = default;

// The BluetoothAdapterClient implementation used in production.
class BluetoothAdapterClientImpl : public BluetoothAdapterClient,
                                   public dbus::ObjectManager::Interface {
 public:
  BluetoothAdapterClientImpl() = default;

  BluetoothAdapterClientImpl(const BluetoothAdapterClientImpl&) = delete;
  BluetoothAdapterClientImpl& operator=(const BluetoothAdapterClientImpl&) =
      delete;

  ~BluetoothAdapterClientImpl() override {
    // There is an instance of this client that is created but not initialized
    // on Linux. See 'Alternate D-Bus Client' note in bluez_dbus_manager.h.
    if (object_manager_) {
      object_manager_->UnregisterInterface(
          bluetooth_adapter::kBluetoothAdapterInterface);
    }
  }

  // BluetoothAdapterClient override.
  void AddObserver(BluetoothAdapterClient::Observer* observer) override {
    DCHECK(observer);
    observers_.AddObserver(observer);
  }

  // BluetoothAdapterClient override.
  void RemoveObserver(BluetoothAdapterClient::Observer* observer) override {
    DCHECK(observer);
    observers_.RemoveObserver(observer);
  }

  // Returns the list of adapter object paths known to the system.
  std::vector<dbus::ObjectPath> GetAdapters() override {
    return object_manager_->GetObjectsWithInterface(
        bluetooth_adapter::kBluetoothAdapterInterface);
  }

  // dbus::ObjectManager::Interface override.
  dbus::PropertySet* CreateProperties(
      dbus::ObjectProxy* object_proxy,
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) override {
    return new Properties(
        object_proxy, interface_name,
        base::BindRepeating(&BluetoothAdapterClientImpl::OnPropertyChanged,
                            weak_ptr_factory_.GetWeakPtr(), object_path));
  }

  // BluetoothAdapterClient override.
  Properties* GetProperties(const dbus::ObjectPath& object_path) override {
    return static_cast<Properties*>(object_manager_->GetProperties(
        object_path, bluetooth_adapter::kBluetoothAdapterInterface));
  }

  // BluetoothAdapterClient override.
  void StartDiscovery(const dbus::ObjectPath& object_path,
                      ResponseCallback callback) override {
    dbus::MethodCall method_call(bluetooth_adapter::kBluetoothAdapterInterface,
                                 bluetooth_adapter::kStartDiscovery);

    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(object_path);
    if (!object_proxy) {
      std::move(callback).Run(Error(kUnknownAdapterError, ""));
      return;
    }

    object_proxy->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&BluetoothAdapterClientImpl::OnResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // BluetoothAdapterClient override.
  void StopDiscovery(const dbus::ObjectPath& object_path,
                     ResponseCallback callback) override {
    dbus::MethodCall method_call(bluetooth_adapter::kBluetoothAdapterInterface,
                                 bluetooth_adapter::kStopDiscovery);

    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(object_path);
    if (!object_proxy) {
      std::move(callback).Run(Error(kUnknownAdapterError, ""));
      return;
    }

    object_proxy->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&BluetoothAdapterClientImpl::OnResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // BluetoothAdapterClient override.
  void RemoveDevice(const dbus::ObjectPath& object_path,
                    const dbus::ObjectPath& device_path,
                    base::OnceClosure callback,
                    ErrorCallback error_callback) override {
    dbus::MethodCall method_call(bluetooth_adapter::kBluetoothAdapterInterface,
                                 bluetooth_adapter::kRemoveDevice);

    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(device_path);

    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(object_path);
    if (!object_proxy) {
      std::move(error_callback).Run(kUnknownAdapterError, "");
      return;
    }

    object_proxy->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&BluetoothAdapterClientImpl::OnSuccess,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        base::BindOnce(&BluetoothAdapterClientImpl::OnError,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(error_callback)));
  }

  // BluetoothAdapterClient override.
  void SetDiscoveryFilter(const dbus::ObjectPath& object_path,
                          const DiscoveryFilter& discovery_filter,
                          base::OnceClosure callback,
                          ErrorCallback error_callback) override {
    dbus::MethodCall method_call(bluetooth_adapter::kBluetoothAdapterInterface,
                                 bluetooth_adapter::kSetDiscoveryFilter);

    dbus::MessageWriter writer(&method_call);
    dbus::MessageWriter dict_writer(nullptr);

    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(object_path);
    if (!object_proxy) {
      std::move(error_callback).Run(kUnknownAdapterError, "");
      return;
    }

    writer.OpenArray("{sv}", &dict_writer);

    if (discovery_filter.uuids.get()) {
      std::vector<std::string>* uuids = discovery_filter.uuids.get();
      dbus::MessageWriter uuids_entry_writer(nullptr);
      dict_writer.OpenDictEntry(&uuids_entry_writer);
      uuids_entry_writer.AppendString(
          bluetooth_adapter::kDiscoveryFilterParameterUUIDs);

      dbus::MessageWriter uuids_array_variant(nullptr);
      uuids_entry_writer.OpenVariant("as", &uuids_array_variant);
      dbus::MessageWriter uuids_array(nullptr);
      uuids_array_variant.OpenArray("s", &uuids_array);

      for (auto& it : *uuids)
        uuids_array.AppendString(it);

      uuids_array_variant.CloseContainer(&uuids_array);
      uuids_entry_writer.CloseContainer(&uuids_array_variant);
      dict_writer.CloseContainer(&uuids_entry_writer);
    }

    if (discovery_filter.rssi.get()) {
      dbus::MessageWriter rssi_entry_writer(nullptr);
      dict_writer.OpenDictEntry(&rssi_entry_writer);
      rssi_entry_writer.AppendString(
          bluetooth_adapter::kDiscoveryFilterParameterRSSI);
      rssi_entry_writer.AppendVariantOfInt16(*discovery_filter.rssi.get());
      dict_writer.CloseContainer(&rssi_entry_writer);
    }

    if (discovery_filter.pathloss.get()) {
      dbus::MessageWriter pathloss_entry_writer(nullptr);
      dict_writer.OpenDictEntry(&pathloss_entry_writer);
      pathloss_entry_writer.AppendString(
          bluetooth_adapter::kDiscoveryFilterParameterPathloss);
      pathloss_entry_writer.AppendVariantOfUint16(
          *discovery_filter.pathloss.get());
      dict_writer.CloseContainer(&pathloss_entry_writer);
    }

    if (discovery_filter.transport.get()) {
      dbus::MessageWriter transport_entry_writer(nullptr);
      dict_writer.OpenDictEntry(&transport_entry_writer);
      transport_entry_writer.AppendString(
          bluetooth_adapter::kDiscoveryFilterParameterTransport);
      transport_entry_writer.AppendVariantOfString(
          *discovery_filter.transport.get());
      dict_writer.CloseContainer(&transport_entry_writer);
    }

    writer.CloseContainer(&dict_writer);

    object_proxy->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&BluetoothAdapterClientImpl::OnSuccess,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        base::BindOnce(&BluetoothAdapterClientImpl::OnError,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(error_callback)));
  }

  // BluetoothAdapterClient override.
  void CreateServiceRecord(const dbus::ObjectPath& object_path,
                           const bluez::BluetoothServiceRecordBlueZ& record,
                           ServiceRecordCallback callback,
                           ErrorCallback error_callback) override {
    dbus::MethodCall method_call(bluetooth_adapter::kBluetoothAdapterInterface,
                                 bluetooth_adapter::kCreateServiceRecord);

    dbus::MessageWriter writer(&method_call);
    dbus::MessageWriter array_writer(&method_call);
    dbus::MessageWriter dict_entry_writer(nullptr);
    writer.OpenArray("{q(yuv)}", &array_writer);
    for (auto attribute_id : record.GetAttributeIds()) {
      array_writer.OpenDictEntry(&dict_entry_writer);
      dict_entry_writer.AppendUint16(attribute_id);
      const BluetoothServiceAttributeValueBlueZ& attribute_value =
          record.GetAttributeValue(attribute_id);
      WriteAttribute(&dict_entry_writer, attribute_value);
      array_writer.CloseContainer(&dict_entry_writer);
    }
    writer.CloseContainer(&array_writer);

    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(object_path);
    if (!object_proxy) {
      std::move(error_callback).Run(kUnknownAdapterError, "");
      return;
    }

    object_proxy->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&BluetoothAdapterClientImpl::OnCreateServiceRecord,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        base::BindOnce(&BluetoothAdapterClientImpl::OnError,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(error_callback)));
  }

  // BluetoothAdapterClient override.
  void RemoveServiceRecord(const dbus::ObjectPath& object_path,
                           uint32_t handle,
                           base::OnceClosure callback,
                           ErrorCallback error_callback) override {
    dbus::MethodCall method_call(bluetooth_adapter::kBluetoothAdapterInterface,
                                 bluetooth_adapter::kRemoveServiceRecord);

    dbus::MessageWriter writer(&method_call);
    writer.AppendUint32(handle);
    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(object_path);
    if (!object_proxy) {
      std::move(error_callback).Run(kUnknownAdapterError, "");
      return;
    }

    object_proxy->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&BluetoothAdapterClientImpl::OnSuccess,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        base::BindOnce(&BluetoothAdapterClientImpl::OnError,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(error_callback)));
  }

  // BluetoothAdapterClient override.
  void ConnectDevice(const dbus::ObjectPath& object_path,
                     const std::string& address,
                     const std::optional<AddressType>& address_type,
                     ConnectDeviceCallback callback,
                     ErrorCallback error_callback) override {
    dbus::MethodCall method_call(bluetooth_adapter::kBluetoothAdapterInterface,
                                 bluetooth_adapter::kConnectDevice);

    dbus::MessageWriter writer(&method_call);
    base::Value::Dict dict;
    dict.Set(bluetooth_device::kAddressProperty, address);
    if (address_type) {
      std::string address_type_value;
      switch (*address_type) {
        case AddressType::kPublic:
          address_type_value = kBluezAddressTypePublic;
          break;
        case AddressType::kRandom:
          address_type_value = kBluezAddressTypeRandom;
          break;
        default:
          NOTREACHED();
      };
      dict.Set(bluetooth_device::kAddressTypeProperty, address_type_value);
    }
    dbus::AppendValueData(&writer, dict);

    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(object_path);
    if (!object_proxy) {
      std::move(error_callback).Run(kUnknownAdapterError, "");
      return;
    }

    object_proxy->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&BluetoothAdapterClientImpl::OnConnectDevice,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       /*start_time=*/base::Time::Now()),
        base::BindOnce(&BluetoothAdapterClientImpl::OnConnectDeviceError,
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
        bluetooth_adapter::kBluetoothAdapterInterface, this);
  }

 private:
  // Called by dbus::ObjectManager when an object with the adapter interface
  // is created. Informs observers.
  void ObjectAdded(const dbus::ObjectPath& object_path,
                   const std::string& interface_name) override {
    for (auto& observer : observers_)
      observer.AdapterAdded(object_path);
  }

  // Called by dbus::ObjectManager when an object with the adapter interface
  // is removed. Informs observers.
  void ObjectRemoved(const dbus::ObjectPath& object_path,
                     const std::string& interface_name) override {
    for (auto& observer : observers_)
      observer.AdapterRemoved(object_path);
  }

  // Called by dbus::PropertySet when a property value is changed,
  // either by result of a signal or response to a GetAll() or Get()
  // call. Informs observers.
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name) {
    for (auto& observer : observers_)
      observer.AdapterPropertyChanged(object_path, property_name);
  }

  // Called when a response for successful method call is received.
  void OnSuccess(base::OnceClosure callback, dbus::Response* response) {
    DCHECK(response);
    std::move(callback).Run();
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

  void OnResponse(ResponseCallback callback,
                  dbus::Response* response,
                  dbus::ErrorResponse* error_response) {
    if (response) {
      std::move(callback).Run(std::nullopt);
      return;
    }

    std::move(callback).Run(ErrorResponseToError(error_response));
  }

  // Called when CreateServiceRecord() succeeds.
  void OnCreateServiceRecord(ServiceRecordCallback callback,
                             dbus::Response* response) {
    DCHECK(response);
    dbus::MessageReader reader(response);
    uint32_t handle = 0;
    if (!reader.PopUint32(&handle))
      LOG(ERROR) << "Invalid response from CreateServiceRecord.";
    std::move(callback).Run(handle);
  }

  // Called when ConnectDevice() succeeds.
  void OnConnectDevice(ConnectDeviceCallback callback,
                       base::Time start_time,
                       dbus::Response* response) {
    DCHECK(response);
    dbus::MessageReader reader(response);
    dbus::ObjectPath device_path;
    if (!reader.PopObjectPath(&device_path))
      LOG(ERROR) << "Invalid response from ConnectDevice.";

    RecordSuccess(kConnectDeviceMethod, start_time);
    std::move(callback).Run(device_path);
  }

  void OnConnectDeviceError(ErrorCallback error_callback,
                            dbus::ErrorResponse* response) {
    RecordFailure(kConnectDeviceMethod, response);
    OnError(std::move(error_callback), response);
  }

  raw_ptr<dbus::ObjectManager> object_manager_ = nullptr;

  // List of observers interested in event notifications from us.
  base::ObserverList<BluetoothAdapterClient::Observer>::Unchecked observers_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAdapterClientImpl> weak_ptr_factory_{this};
};

BluetoothAdapterClient::BluetoothAdapterClient() = default;

BluetoothAdapterClient::~BluetoothAdapterClient() = default;

BluetoothAdapterClient* BluetoothAdapterClient::Create() {
  return new BluetoothAdapterClientImpl;
}

void BluetoothAdapterClient::StartDiscovery(const dbus::ObjectPath& object_path,
                                            base::OnceClosure callback,
                                            ErrorCallback error_callback) {
  StartDiscovery(object_path,
                 base::BindOnce(&OnResponseAdapter, std::move(callback),
                                std::move(error_callback)));
}

void BluetoothAdapterClient::StopDiscovery(const dbus::ObjectPath& object_path,
                                           base::OnceClosure callback,
                                           ErrorCallback error_callback) {
  StopDiscovery(object_path,
                base::BindOnce(&OnResponseAdapter, std::move(callback),
                               std::move(error_callback)));
}

}  // namespace bluez
