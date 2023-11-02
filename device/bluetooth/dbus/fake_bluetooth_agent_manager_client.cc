// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/fake_bluetooth_agent_manager_client.h"

#include "base/logging.h"
#include "base/observer_list.h"
#include "device/bluetooth/dbus/fake_bluetooth_agent_service_provider.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

FakeBluetoothAgentManagerClient::FakeBluetoothAgentManagerClient()
    : service_provider_(nullptr) {}

FakeBluetoothAgentManagerClient::~FakeBluetoothAgentManagerClient() = default;

void FakeBluetoothAgentManagerClient::Init(
    dbus::Bus* bus,
    const std::string& bluetooth_service_name) {}

void FakeBluetoothAgentManagerClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeBluetoothAgentManagerClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FakeBluetoothAgentManagerClient::RegisterAgent(
    const dbus::ObjectPath& agent_path,
    const std::string& capability,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  DVLOG(1) << "RegisterAgent: " << agent_path.value();

  if (service_provider_ == nullptr) {
    std::move(error_callback)
        .Run(bluetooth_agent_manager::kErrorInvalidArguments,
             "No agent created");
  } else if (service_provider_->object_path_ != agent_path) {
    std::move(error_callback)
        .Run(bluetooth_agent_manager::kErrorAlreadyExists,
             "Agent already registered");
  } else {
    std::move(callback).Run();
  }
}

void FakeBluetoothAgentManagerClient::UnregisterAgent(
    const dbus::ObjectPath& agent_path,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  DVLOG(1) << "UnregisterAgent: " << agent_path.value();
  if (service_provider_ == nullptr) {
    std::move(error_callback)
        .Run(bluetooth_agent_manager::kErrorDoesNotExist,
             "No agent registered");
  } else if (service_provider_->object_path_ != agent_path) {
    std::move(error_callback)
        .Run(bluetooth_agent_manager::kErrorDoesNotExist,
             "Agent still registered");
  } else {
    std::move(callback).Run();
  }
}

void FakeBluetoothAgentManagerClient::RequestDefaultAgent(
    const dbus::ObjectPath& agent_path,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  DVLOG(1) << "RequestDefaultAgent: " << agent_path.value();
  std::move(callback).Run();
}

void FakeBluetoothAgentManagerClient::RegisterAgentServiceProvider(
    FakeBluetoothAgentServiceProvider* service_provider) {
  service_provider_ = service_provider;
}

void FakeBluetoothAgentManagerClient::UnregisterAgentServiceProvider(
    FakeBluetoothAgentServiceProvider* service_provider) {
  if (service_provider_ == service_provider)
    service_provider_ = nullptr;
}

FakeBluetoothAgentServiceProvider*
FakeBluetoothAgentManagerClient::GetAgentServiceProvider() {
  return service_provider_;
}

}  // namespace bluez
