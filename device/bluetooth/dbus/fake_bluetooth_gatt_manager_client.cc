// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/fake_bluetooth_gatt_manager_client.h"

#include <set>
#include <string>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

std::set<dbus::ObjectPath> FakeBluetoothGattManagerClient::FindServiceProviders(
    dbus::ObjectPath application_path) {
  std::set<dbus::ObjectPath> services;
  for (const auto& service : service_map_) {
    if (base::StartsWith(service.first.value(), application_path.value(),
                         base::CompareCase::SENSITIVE))
      services.insert(service.first);
  }
  return services;
}

std::set<dbus::ObjectPath>
FakeBluetoothGattManagerClient::FindCharacteristicProviders(
    dbus::ObjectPath application_path) {
  std::set<dbus::ObjectPath> characteristics;
  for (const auto& characteristic : characteristic_map_) {
    if (base::StartsWith(characteristic.first.value(), application_path.value(),
                         base::CompareCase::SENSITIVE)) {
      characteristics.insert(characteristic.first);
    }
  }
  return characteristics;
}

std::set<dbus::ObjectPath>
FakeBluetoothGattManagerClient::FindDescriptorProviders(
    dbus::ObjectPath application_path) {
  std::set<dbus::ObjectPath> descriptors;
  for (const auto& descriptor : descriptor_map_) {
    if (base::StartsWith(descriptor.first.value(), application_path.value(),
                         base::CompareCase::SENSITIVE)) {
      descriptors.insert(descriptor.first);
    }
  }
  return descriptors;
}

bool FakeBluetoothGattManagerClient::VerifyProviderHierarchy(
    FakeBluetoothGattApplicationServiceProvider* application_provider) {
  dbus::ObjectPath application_path = application_provider->object_path();
  std::set<dbus::ObjectPath> services = FindServiceProviders(application_path);
  std::set<dbus::ObjectPath> characteristics =
      FindCharacteristicProviders(application_path);
  std::set<dbus::ObjectPath> descriptors =
      FindDescriptorProviders(application_path);

  DVLOG(1) << "Verifying " << services.size()
           << " services in application: " << application_path.value();

  for (const auto& descriptor : descriptors) {
    if (characteristics.count(
            descriptor_map_[descriptor]->characteristic_path()) != 1) {
      return false;
    }
    DVLOG(1) << "Descriptor " << descriptor.value()
             << " verified, has parent characteristic ("
             << descriptor_map_[descriptor]->characteristic_path().value()
             << ")  in hierarchy.";
  }

  for (const auto& characteristic : characteristics) {
    if (services.count(characteristic_map_[characteristic]->service_path()) !=
        1) {
      return false;
    }
    DVLOG(1) << "Characteristic " << characteristic.value()
             << " verified, has parent service ("
             << characteristic_map_[characteristic]->service_path().value()
             << ") in hierarchy.";
  }

  return true;
}

FakeBluetoothGattManagerClient::FakeBluetoothGattManagerClient() = default;

FakeBluetoothGattManagerClient::~FakeBluetoothGattManagerClient() = default;

// DBusClient override.
void FakeBluetoothGattManagerClient::Init(
    dbus::Bus* bus,
    const std::string& bluetooth_service_name) {}

// BluetoothGattManagerClient overrides.
void FakeBluetoothGattManagerClient::RegisterApplication(
    const dbus::ObjectPath& adapter_object_path,
    const dbus::ObjectPath& application_path,
    const Options& options,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  DVLOG(1) << "Register GATT application: " << application_path.value();
  ApplicationProvider* provider =
      GetApplicationServiceProvider(application_path);
  if (!provider || provider->second) {
    std::move(error_callback).Run(bluetooth_gatt_service::kErrorFailed, "");
    return;
  }
  if (!VerifyProviderHierarchy(provider->first)) {
    std::move(error_callback).Run(bluetooth_gatt_service::kErrorFailed, "");
    return;
  }
  provider->second = true;
  std::move(callback).Run();
}

// BluetoothGattManagerClient overrides.
void FakeBluetoothGattManagerClient::UnregisterApplication(
    const dbus::ObjectPath& adapter_object_path,
    const dbus::ObjectPath& application_path,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  DVLOG(1) << "Unregister GATT application: " << application_path.value();
  ApplicationProvider* provider =
      GetApplicationServiceProvider(application_path);
  if (!provider || !provider->second) {
    std::move(error_callback).Run(bluetooth_gatt_service::kErrorFailed, "");
    return;
  }
  provider->second = false;
  std::move(callback).Run();
}

void FakeBluetoothGattManagerClient::RegisterApplicationServiceProvider(
    FakeBluetoothGattApplicationServiceProvider* provider) {
  application_map_[provider->object_path()] =
      std::pair<FakeBluetoothGattApplicationServiceProvider*, bool>(provider,
                                                                    false);
}

void FakeBluetoothGattManagerClient::UnregisterApplicationServiceProvider(
    FakeBluetoothGattApplicationServiceProvider* provider) {
  auto iter = application_map_.find(provider->object_path());
  if (iter != application_map_.end() && iter->second.first == provider)
    application_map_.erase(iter);
}

FakeBluetoothGattManagerClient::ApplicationProvider*
FakeBluetoothGattManagerClient::GetApplicationServiceProvider(
    const dbus::ObjectPath& object_path) {
  auto iter = application_map_.find(object_path);
  return (iter != application_map_.end()) ? &iter->second : nullptr;
}

void FakeBluetoothGattManagerClient::RegisterServiceServiceProvider(
    FakeBluetoothGattServiceServiceProvider* provider) {
  // Ignore, if a service provider is already registered for the object path.
  auto iter = service_map_.find(provider->object_path());
  if (iter != service_map_.end()) {
    DVLOG(1) << "GATT service service provider already registered for "
             << "object path: " << provider->object_path().value();
    return;
  }
  service_map_[provider->object_path()] = provider;
}

void FakeBluetoothGattManagerClient::RegisterCharacteristicServiceProvider(
    FakeBluetoothGattCharacteristicServiceProvider* provider) {
  // Ignore, if a service provider is already registered for the object path.
  auto iter = characteristic_map_.find(provider->object_path());
  if (iter != characteristic_map_.end()) {
    DVLOG(1) << "GATT characteristic service provider already registered for "
             << "object path: " << provider->object_path().value();
    return;
  }
  characteristic_map_[provider->object_path()] = provider;
}

void FakeBluetoothGattManagerClient::RegisterDescriptorServiceProvider(
    FakeBluetoothGattDescriptorServiceProvider* provider) {
  // Ignore, if a service provider is already registered for the object path.
  auto iter = descriptor_map_.find(provider->object_path());
  if (iter != descriptor_map_.end()) {
    DVLOG(1) << "GATT descriptor service provider already registered for "
             << "object path: " << provider->object_path().value();
    return;
  }
  descriptor_map_[provider->object_path()] = provider;
}

void FakeBluetoothGattManagerClient::UnregisterServiceServiceProvider(
    FakeBluetoothGattServiceServiceProvider* provider) {
  auto iter = service_map_.find(provider->object_path());
  if (iter != service_map_.end() && iter->second == provider)
    service_map_.erase(iter);
}

void FakeBluetoothGattManagerClient::UnregisterCharacteristicServiceProvider(
    FakeBluetoothGattCharacteristicServiceProvider* provider) {
  characteristic_map_.erase(provider->object_path());
}

void FakeBluetoothGattManagerClient::UnregisterDescriptorServiceProvider(
    FakeBluetoothGattDescriptorServiceProvider* provider) {
  descriptor_map_.erase(provider->object_path());
}

FakeBluetoothGattServiceServiceProvider*
FakeBluetoothGattManagerClient::GetServiceServiceProvider(
    const dbus::ObjectPath& object_path) const {
  auto iter = service_map_.find(object_path);
  if (iter == service_map_.end())
    return NULL;
  return iter->second;
}

FakeBluetoothGattCharacteristicServiceProvider*
FakeBluetoothGattManagerClient::GetCharacteristicServiceProvider(
    const dbus::ObjectPath& object_path) const {
  auto iter = characteristic_map_.find(object_path);
  if (iter == characteristic_map_.end())
    return NULL;
  return iter->second;
}

FakeBluetoothGattDescriptorServiceProvider*
FakeBluetoothGattManagerClient::GetDescriptorServiceProvider(
    const dbus::ObjectPath& object_path) const {
  auto iter = descriptor_map_.find(object_path);
  if (iter == descriptor_map_.end())
    return NULL;
  return iter->second;
}

bool FakeBluetoothGattManagerClient::IsServiceRegistered(
    const dbus::ObjectPath& service_path) const {
  const auto& service = service_map_.find(service_path);
  if (service == service_map_.end())
    return false;

  for (const auto& application : application_map_) {
    if (base::StartsWith(service_path.value(),
                         application.second.first->object_path().value(),
                         base::CompareCase::SENSITIVE)) {
      return application.second.second;
    }
  }
  return false;
}

}  // namespace bluez
