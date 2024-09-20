// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_POWER_MONITOR_POWER_MONITOR_BROADCAST_SOURCE_H_
#define SERVICES_DEVICE_PUBLIC_CPP_POWER_MONITOR_POWER_MONITOR_BROADCAST_SOURCE_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/power_monitor/power_monitor_source.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/device/public/mojom/power_monitor.mojom.h"

namespace base {
class SequencedTaskRunner;
}

namespace device {

// Receives state changes from Power Monitor through mojo, and relays them to
// the PowerMonitor of the current process.
class PowerMonitorBroadcastSource : public base::PowerMonitorSource {
 public:
  PowerMonitorBroadcastSource(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  PowerMonitorBroadcastSource(const PowerMonitorBroadcastSource&) = delete;
  PowerMonitorBroadcastSource& operator=(const PowerMonitorBroadcastSource&) =
      delete;

  ~PowerMonitorBroadcastSource() override;

  // Completes initialization by setting up the connection with the Device
  // Service. Split out from the constructor in order to enable the client to
  // ensure that the process-wide PowerMonitor instance is initialized before
  // the Mojo connection is set up.
  void Init(mojo::PendingRemote<mojom::PowerMonitor> remote_monitor);

 private:
  friend class PowerMonitorBroadcastSourceTest;
  friend class MockClient;
  FRIEND_TEST_ALL_PREFIXES(PowerMonitorBroadcastSourceTest,
                           PowerMessageReceiveBroadcast);
  FRIEND_TEST_ALL_PREFIXES(PowerMonitorMessageBroadcasterTest,
                           PowerMessageBroadcast);
  FRIEND_TEST_ALL_PREFIXES(PowerMonitorMessageBroadcasterTest,
                           PowerClientUpdateWhenOnBattery);

  // Client holds the mojo connection. It is created on the main thread, and
  // destroyed on task runner's thread. Unless otherwise noted, all its methods
  // are called on the task runner's thread.
  class Client : public device::mojom::PowerMonitorClient {
   public:
    Client();

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    ~Client() override;

    void Init(mojo::PendingRemote<mojom::PowerMonitor> remote_monitor);

    base::PowerStateObserver::BatteryPowerStatus
    last_reported_battery_power_status() const {
      return last_reported_battery_power_status_;
    }

    // device::mojom::PowerMonitorClient implementation
    void PowerStateChange(base::PowerStateObserver::BatteryPowerStatus
                              battery_power_status) override;
    void Suspend() override;
    void Resume() override;

   private:
    mojo::Receiver<device::mojom::PowerMonitorClient> receiver_{this};

    base::PowerStateObserver::BatteryPowerStatus
        last_reported_battery_power_status_ =
            base::PowerStateObserver::BatteryPowerStatus::kUnknown;
  };

  // This constructor is used by test code to mock the Client class.
  PowerMonitorBroadcastSource(
      std::unique_ptr<Client> client,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  Client* client_for_testing() const { return client_.get(); }

  base::PowerStateObserver::BatteryPowerStatus GetBatteryPowerStatus()
      const override;

  std::unique_ptr<Client> client_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_POWER_MONITOR_POWER_MONITOR_BROADCAST_SOURCE_H_
