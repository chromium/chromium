// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/power_monitor/power_monitor_message_broadcaster.h"

#include "base/power_monitor/power_monitor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace device {

PowerMonitorMessageBroadcaster::PowerMonitorMessageBroadcaster() {
  base::PowerMonitor::AddObserver(this);
}

PowerMonitorMessageBroadcaster::~PowerMonitorMessageBroadcaster() {
  base::PowerMonitor::RemoveObserver(this);
}

// static
void PowerMonitorMessageBroadcaster::Bind(
    mojo::PendingReceiver<device::mojom::PowerMonitor> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void PowerMonitorMessageBroadcaster::AddClient(
    mojo::PendingRemote<device::mojom::PowerMonitorClient>
        power_monitor_client) {
  clients_.Add(std::move(power_monitor_client));
  if (base::PowerMonitor::IsInitialized())
    OnPowerStateChange(base::PowerMonitor::IsOnBatteryPower());
}

void PowerMonitorMessageBroadcaster::OnPowerStateChange(bool on_battery_power) {
  for (auto& client : clients_)
    client->PowerStateChange(on_battery_power);
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
