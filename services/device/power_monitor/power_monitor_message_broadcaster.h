// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_POWER_MONITOR_POWER_MONITOR_MESSAGE_BROADCASTER_H_
#define SERVICES_DEVICE_POWER_MONITOR_POWER_MONITOR_MESSAGE_BROADCASTER_H_

#include "base/power_monitor/power_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/device/public/mojom/power_monitor.mojom.h"

namespace device {

// A class used to monitor the power state change and communicate it to child
// processes via IPC.
class PowerMonitorMessageBroadcaster : public base::PowerStateObserver,
                                       public base::PowerSuspendObserver,
                                       public device::mojom::PowerMonitor {
 public:
  PowerMonitorMessageBroadcaster();

  PowerMonitorMessageBroadcaster(const PowerMonitorMessageBroadcaster&) =
      delete;
  PowerMonitorMessageBroadcaster& operator=(
      const PowerMonitorMessageBroadcaster&) = delete;

  ~PowerMonitorMessageBroadcaster() override;

  void Bind(mojo::PendingReceiver<device::mojom::PowerMonitor> receiver);

  // device::mojom::PowerMonitor:
  void AddClient(mojo::PendingRemote<device::mojom::PowerMonitorClient>
                     power_monitor_client) override;

  // base::PowerStateObserver:
  void OnBatteryPowerStatusChange(base::PowerStateObserver::BatteryPowerStatus
                                      battery_power_status) override;

  // base::PowerSuspendObserver:
  void OnSuspend() override;
  void OnResume() override;

 private:
  mojo::ReceiverSet<device::mojom::PowerMonitor> receivers_;
  mojo::RemoteSet<device::mojom::PowerMonitorClient> clients_;
  base::PowerStateObserver::BatteryPowerStatus battery_power_status_ =
      base::PowerStateObserver::BatteryPowerStatus::kUnknown;
};

}  // namespace device

#endif  // SERVICES_DEVICE_POWER_MONITOR_POWER_MONITOR_MESSAGE_BROADCASTER_H_
