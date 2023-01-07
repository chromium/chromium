// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/power_monitor/power_monitor_message_broadcaster.h"

#include "base/power_monitor/power_monitor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace device {

PowerMonitorMessageBroadcaster::PowerMonitorMessageBroadcaster() {
  base::PowerMonitor::AddPowerSuspendObserver(this);
  base::PowerMonitor::AddPowerStateObserver(this);
}

PowerMonitorMessageBroadcaster::~PowerMonitorMessageBroadcaster() {
  base::PowerMonitor::RemovePowerSuspendObserver(this);
  base::PowerMonitor::RemovePowerStateObserver(this);
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

  if (!base::PowerMonitor::IsInitialized())
    return;

  bool on_battery_power = base::PowerMonitor::IsOnBatteryPower();

  // If the state has changed since we last checked, update all clients.
  if (on_battery_power != on_battery_power_) {
    OnPowerStateChange(on_battery_power);
    return;
  }

  // New clients default to on_battery_power == false. Only update this new
  // client if on_battery_power_ == true;
  if (on_battery_power_) {
    clients_.Get(element_id)->PowerStateChange(on_battery_power_);
  }
}

void PowerMonitorMessageBroadcaster::OnPowerStateChange(bool on_battery_power) {
  on_battery_power_ = on_battery_power;
  for (auto& client : clients_)
    client->PowerStateChange(on_battery_power_);
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
