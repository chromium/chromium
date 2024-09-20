// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/power_monitor/power_monitor_broadcast_source.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/base/battery_power_status_traits.h"
#include "mojo/public/cpp/bindings/remote.h"
namespace device {

PowerMonitorBroadcastSource::PowerMonitorBroadcastSource(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : PowerMonitorBroadcastSource(std::make_unique<Client>(),
                                  task_runner) {}

PowerMonitorBroadcastSource::PowerMonitorBroadcastSource(
    std::unique_ptr<Client> client,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : client_(std::move(client)), task_runner_(task_runner) {}

PowerMonitorBroadcastSource::~PowerMonitorBroadcastSource() {
  task_runner_->DeleteSoon(FROM_HERE, client_.release());
}

void PowerMonitorBroadcastSource::Init(
    mojo::PendingRemote<mojom::PowerMonitor> remote_monitor) {
  if (remote_monitor) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&PowerMonitorBroadcastSource::Client::Init,
                                  base::Unretained(client_.get()),
                                  std::move(remote_monitor)));
  }
}

base::PowerStateObserver::BatteryPowerStatus
PowerMonitorBroadcastSource::GetBatteryPowerStatus() const {
  return client_->last_reported_battery_power_status();
}

PowerMonitorBroadcastSource::Client::Client() = default;

PowerMonitorBroadcastSource::Client::~Client() {}

void PowerMonitorBroadcastSource::Client::Init(
    mojo::PendingRemote<mojom::PowerMonitor> remote_monitor) {
  mojo::Remote<mojom::PowerMonitor> power_monitor(std::move(remote_monitor));
  power_monitor->AddClient(receiver_.BindNewPipeAndPassRemote());
}

void PowerMonitorBroadcastSource::Client::PowerStateChange(
    base::PowerStateObserver::BatteryPowerStatus battery_power_status) {
  last_reported_battery_power_status_ = battery_power_status;
  ProcessPowerEvent(PowerMonitorSource::POWER_STATE_EVENT);
}

void PowerMonitorBroadcastSource::Client::Suspend() {
  ProcessPowerEvent(PowerMonitorSource::SUSPEND_EVENT);
}

void PowerMonitorBroadcastSource::Client::Resume() {
  ProcessPowerEvent(PowerMonitorSource::RESUME_EVENT);
}

}  // namespace device
