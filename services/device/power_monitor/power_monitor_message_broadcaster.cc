// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/power_monitor/power_monitor_message_broadcaster.h"

#include "base/power_monitor/power_monitor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace device {

PowerMonitorMessageBroadcaster::PowerMonitorMessageBroadcaster() {
  auto* power_monitor = base::PowerMonitor::GetInstance();
  power_monitor->AddPowerSuspendObserver(this);
  power_monitor->AddPowerStateObserver(this);
}

PowerMonitorMessageBroadcaster::~PowerMonitorMessageBroadcaster() {
  auto* power_monitor = base::PowerMonitor::GetInstance();
  power_monitor->RemovePowerSuspendObserver(this);
  power_monitor->RemovePowerStateObserver(this);
}

// static
void PowerMonitorMessageBroadcaster::Bind(
    mojo::PendingReceiver<device::mojom::PowerMonitor> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void PowerMonitorMessageBroadcaster::AddClient(
    mojo::PendingRemote<device::mojom::PowerMonitorClient>
        power_monitor_client) {
  mojo::RemoteSetElementId element_id =
      clients_.Add(std::move(power_monitor_client));
  auto* power_monitor = base::PowerMonitor::GetInstance();

  if (!power_monitor->IsInitialized()) {
    return;
  }

  base::PowerStateObserver::BatteryPowerStatus battery_power_status =
      power_monitor->GetBatteryPowerStatus();
  // If the state has changed since we last checked, update all clients.
  if (battery_power_status != battery_power_status_) {
    OnBatteryPowerStatusChange(battery_power_status);
    return;
  }

  // New clients default to battery_power_status_ == kUnknown. Only update this
  // new client if battery power status isn't unknown;
  if (battery_power_status_ !=
      base::PowerStateObserver::BatteryPowerStatus::kUnknown) {
    clients_.Get(element_id)->PowerStateChange(battery_power_status_);
  }
}

void PowerMonitorMessageBroadcaster::OnBatteryPowerStatusChange(
    base::PowerStateObserver::BatteryPowerStatus battery_power_status) {
  battery_power_status_ = battery_power_status;
  for (auto& client : clients_)
    client->PowerStateChange(battery_power_status_);
}

void PowerMonitorMessageBroadcaster::OnSuspend() {
  for (auto& client : clients_)
    client->Suspend();
}

void PowerMonitorMessageBroadcaster::OnResume() {
  for (auto& client : clients_)
    client->Resume();
}

}  // namespace device
