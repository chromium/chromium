// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_gatt_application_service_provider_impl.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

BluetoothGattApplicationServiceProviderImpl::
    BluetoothGattApplicationServiceProviderImpl(
        dbus::Bus* bus,
        const dbus::ObjectPath& object_path,
        const std::map<
            dbus::ObjectPath,
            raw_ptr<BluetoothLocalGattServiceBlueZ, CtnExperimental>>& services)
    : origin_thread_id_(base::PlatformThread::CurrentId()),
      bus_(bus),
      object_path_(object_path) {
  DVLOG(1) << "Creating Bluetooth GATT application: " << object_path_.value();
  DCHECK(object_path_.IsValid());
  if (!bus_)
    return;

  exported_object_ = bus_->GetExportedObject(object_path_);

  exported_object_->ExportMethod(
      dbus::kDBusObjectManagerInterface,
      dbus::kDBusObjectManagerGetManagedObjects,
      base::BindRepeating(
          &BluetoothGattApplicationServiceProviderImpl::GetManagedObjects,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&BluetoothGattApplicationServiceProviderImpl::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));

  CreateAttributeServiceProviders(bus, services);
}

BluetoothGattApplicationServiceProviderImpl::
    ~BluetoothGattApplicationServiceProviderImpl() {
  DVLOG(1) << "Cleaning up Bluetooth GATT service: " << object_path_.value();
  if (bus_)
    bus_->UnregisterExportedObject(object_path_);
}

bool BluetoothGattApplicationServiceProviderImpl::OnOriginThread() {
  return base::PlatformThread::CurrentId() == origin_thread_id_;
}

template <typename AttributeProvider>
void BluetoothGattApplicationServiceProviderImpl::WriteObjectDict(
    dbus::MessageWriter* writer,
    const std::string& attribute_interface,
    AttributeProvider* attribute_provider) {
  // Open a dict entry for { object_path : interface_list }.
  dbus::MessageWriter object_dict_writer(NULL);
  // [ {oa{sa{sv}} ]
  writer->OpenDictEntry(&object_dict_writer);

  // Key: Object path. [ {o ]
  object_dict_writer.AppendObjectPath(attribute_provider->object_path());

  // Value: Open array for single entry interface_list. [ a{sa{sv}} ]
  dbus::MessageWriter interface_array_writer(NULL);
  object_dict_writer.OpenArray("(sa{sv})", &interface_array_writer);
  WriteInterfaceDict(&interface_array_writer, attribute_interface,
                     attribute_provider);
  object_dict_writer.CloseContainer(&interface_array_writer);

  writer->CloseContainer(&object_dict_writer);
}

template <typename AttributeProvider>
void BluetoothGattApplicationServiceProviderImpl::WriteInterfaceDict(
    dbus::MessageWriter* writer,
    const std::string& attribute_interface,
    AttributeProvider* attribute_provider) {
  // Open a dict entry for { interface_name : properties_list }.
  dbus::MessageWriter interface_dict_writer(NULL);
  // [ {sa{sv}} ]
  writer->OpenDictEntry(&interface_dict_writer);

  // Key: Interface name. [ {s ]
  interface_dict_writer.AppendString(attribute_interface);
  // Value: Open a array for properties_list. [ a{sv}} ]
  attribute_provider->WriteProperties(&interface_dict_writer);
  writer->CloseContainer(&interface_dict_writer);
}

void BluetoothGattApplicationServiceProviderImpl::GetManagedObjects(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  DVLOG(2) << "BluetoothGattApplicationServiceProvider::GetManagedObjects: "
           << object_path_.value();
  DCHECK(OnOriginThread());

  dbus::MessageReader reader(method_call);

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);

  // The expected format by GetAll is [ a{oa{sa{sv}}} ]
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter array_writer(nullptr);

  writer.OpenArray("{oa{sa{sv}}}", &array_writer);

  for (const auto& service_provider : service_providers_) {
    WriteObjectDict(&array_writer,
                    bluetooth_gatt_service::kBluetoothGattServiceInterface,
                    service_provider.get());
  }
  for (const auto& characteristic_provider : characteristic_providers_) {
    WriteObjectDict(
        &array_writer,
        bluetooth_gatt_characteristic::kBluetoothGattCharacteristicInterface,
        characteristic_provider.get());
  }
  for (const auto& descriptor_provider : descriptor_providers_) {
    WriteObjectDict(
        &array_writer,
        bluetooth_gatt_descriptor::kBluetoothGattDescriptorInterface,
        descriptor_provider.get());
  }

  writer.CloseContainer(&array_writer);
  DVLOG(3) << "Sending response to BlueZ for GetManagedObjects: \n"
           << response->ToString();
  std::move(response_sender).Run(std::move(response));
}

// Called by dbus:: when a method is exported.
void BluetoothGattApplicationServiceProviderImpl::OnExported(
    const std::string& interface_name,
    const std::string& method_name,
    bool success) {
  DVLOG_IF(1, !success) << "Failed to export " << interface_name << "."
                        << method_name;
}

}  // namespace bluez
