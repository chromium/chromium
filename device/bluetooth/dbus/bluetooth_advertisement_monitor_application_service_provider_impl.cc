// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_advertisement_monitor_application_service_provider_impl.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

BluetoothAdvertisementMonitorApplicationServiceProviderImpl::
    BluetoothAdvertisementMonitorApplicationServiceProviderImpl(
        dbus::Bus* bus,
        const dbus::ObjectPath& object_path)
    : origin_thread_id_(base::PlatformThread::CurrentId()),
      bus_(bus),
      object_path_(object_path) {
  DVLOG(1) << "Creating Bluetooth Advertisement Monitor application: "
           << object_path_.value();
  DCHECK(bus_);
  DCHECK(object_path_.IsValid());

  exported_object_ = bus_->GetExportedObject(object_path_);
  if (!exported_object_) {
    LOG(WARNING) << "Invalid exported_object";
    return;
  }

  exported_object_->ExportMethod(
      dbus::kDBusObjectManagerInterface,
      dbus::kDBusObjectManagerGetManagedObjects,
      base::BindRepeating(
          &BluetoothAdvertisementMonitorApplicationServiceProviderImpl::
              GetManagedObjects,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(
          &BluetoothAdvertisementMonitorApplicationServiceProviderImpl::
              OnExported,
          weak_ptr_factory_.GetWeakPtr()));
}

BluetoothAdvertisementMonitorApplicationServiceProviderImpl::
    ~BluetoothAdvertisementMonitorApplicationServiceProviderImpl() {
  DVLOG(1) << "Cleaning up Advertisement Monitor Application: "
           << object_path_.value();
  DCHECK(bus_);
  bus_->UnregisterExportedObject(object_path_);
}

void BluetoothAdvertisementMonitorApplicationServiceProviderImpl::AddMonitor(
    std::unique_ptr<BluetoothAdvertisementMonitorServiceProvider>
        advertisement_monitor_service_provider) {
  dbus::ObjectPath monitor_path =
      advertisement_monitor_service_provider->object_path();

  DVLOG(1) << "Adding Advertisement Monitor: " << monitor_path.value();

  advertisement_monitor_providers_.insert(std::make_pair(
      monitor_path.value(), std::move(advertisement_monitor_service_provider)));

  dbus::Signal signal(dbus::kDBusObjectManagerInterface,
                      dbus::kDBusObjectManagerInterfacesAddedSignal);
  dbus::MessageWriter writer(&signal);

  writer.AppendObjectPath(monitor_path);

  dbus::MessageWriter array_writer(nullptr);
  writer.OpenArray("{sa{sv}}", &array_writer);
  WriteInterfaceDict(
      &array_writer,
      bluetooth_advertisement_monitor::kBluetoothAdvertisementMonitorInterface,
      advertisement_monitor_providers_[monitor_path.value()].get());
  writer.CloseContainer(&array_writer);
  exported_object_->SendSignal(&signal);
}

void BluetoothAdvertisementMonitorApplicationServiceProviderImpl::RemoveMonitor(
    const dbus::ObjectPath& monitor_path) {
  if (!base::Contains(advertisement_monitor_providers_, monitor_path.value())) {
    LOG(WARNING)
        << "Failed to remove Advertisement Monitor because it does not exist "
        << monitor_path.value();
    return;
  }
  DVLOG(1) << "Removing Advertisement Monitor: " << monitor_path.value();

  dbus::Signal signal(dbus::kDBusObjectManagerInterface,
                      dbus::kDBusObjectManagerInterfacesRemovedSignal);
  dbus::MessageWriter writer(&signal);

  writer.AppendObjectPath(monitor_path);
  std::vector<std::string> interfaces;
  interfaces.push_back(
      bluetooth_advertisement_monitor::kBluetoothAdvertisementMonitorInterface);
  writer.AppendArrayOfStrings(interfaces);

  exported_object_->SendSignal(&signal);

  advertisement_monitor_providers_.erase(monitor_path.value());
}

bool BluetoothAdvertisementMonitorApplicationServiceProviderImpl::
    OnOriginThread() const {
  return base::PlatformThread::CurrentId() == origin_thread_id_;
}

void BluetoothAdvertisementMonitorApplicationServiceProviderImpl::
    WriteObjectDict(dbus::MessageWriter* writer,
                    const std::string& attribute_interface,
                    BluetoothAdvertisementMonitorServiceProvider*
                        advertisement_monitor_service_provider) {
  // Open a dict entry for { object_path : interface_list }.
  dbus::MessageWriter object_dict_writer(nullptr);
  // [ {oa{sa{sv}} ]
  writer->OpenDictEntry(&object_dict_writer);

  // Key: Object path. [ {o ]
  object_dict_writer.AppendObjectPath(
      advertisement_monitor_service_provider->object_path());

  // Value: Open array for single entry interface_list. [ a{sa{sv}} ]
  dbus::MessageWriter interface_array_writer(nullptr);
  object_dict_writer.OpenArray("(sa{sv})", &interface_array_writer);
  WriteInterfaceDict(&interface_array_writer, attribute_interface,
                     advertisement_monitor_service_provider);
  object_dict_writer.CloseContainer(&interface_array_writer);

  writer->CloseContainer(&object_dict_writer);
}

void BluetoothAdvertisementMonitorApplicationServiceProviderImpl::
    WriteInterfaceDict(dbus::MessageWriter* writer,
                       const std::string& attribute_interface,
                       BluetoothAdvertisementMonitorServiceProvider*
                           advertisement_monitor_service_provider) {
  // Open a dict entry for { interface_name : properties_list }.
  dbus::MessageWriter interface_dict_writer(nullptr);
  // [ {sa{sv}} ]
  writer->OpenDictEntry(&interface_dict_writer);

  // Key: Interface name. [ {s ]
  interface_dict_writer.AppendString(attribute_interface);
  // Value: Open a array for properties_list. [ a{sv}} ]
  advertisement_monitor_service_provider->WriteProperties(
      &interface_dict_writer);
  writer->CloseContainer(&interface_dict_writer);
}

void BluetoothAdvertisementMonitorApplicationServiceProviderImpl::
    GetManagedObjects(dbus::MethodCall* method_call,
                      dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);

  // The expected format by GetManagedObjects is [ a{oa{sa{sv}}} ]
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter array_writer(nullptr);

  // {object path, dict {string, dict {string, variant}}}
  writer.OpenArray("{oa{sa{sv}}}", &array_writer);

  for (const auto& it : advertisement_monitor_providers_) {
    WriteObjectDict(&array_writer,
                    bluetooth_advertisement_monitor::
                        kBluetoothAdvertisementMonitorInterface,
                    it.second.get());
  }

  writer.CloseContainer(&array_writer);
  DVLOG(3) << "Sending response to BlueZ for GetManagedObjects: \n"
           << response->ToString();
  std::move(response_sender).Run(std::move(response));
}

void BluetoothAdvertisementMonitorApplicationServiceProviderImpl::OnExported(
    const std::string& interface_name,
    const std::string& method_name,
    bool success) {
  // Failure case not actionable.
  DVLOG_IF(1, !success) << "Failed to export " << interface_name << "."
                        << method_name;
}

}  // namespace bluez
