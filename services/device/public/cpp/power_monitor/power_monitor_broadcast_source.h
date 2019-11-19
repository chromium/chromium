// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_POWER_MONITOR_POWER_MONITOR_BROADCAST_SOURCE_H_
#define SERVICES_DEVICE_PUBLIC_CPP_POWER_MONITOR_POWER_MONITOR_BROADCAST_SOURCE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
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

  // Client holds the mojo connection. It is created on the main thread, and
  // destroyed on task runner's thread. Unless otherwise noted, all its methods
  // all called on the task runner's thread.
  class Client : public device::mojom::PowerMonitorClient {
   public:
    Client();
    ~Client() override;

    // Called on main thread when the source is destroyed. Prevents data race
    // on the power monitor and source due to use on task runner thread.
    void Shutdown();

    void Init(mojo::PendingRemote<mojom::PowerMonitor> remote_monitor);

    bool last_reported_on_battery_power_state() const {
      return last_reported_on_battery_power_state_;
    }

    // device::mojom::PowerMonitorClient implementation
    void PowerStateChange(bool on_battery_power) override;
    void Suspend() override;
    void Resume() override;

   private:
    mojo::Receiver<device::mojom::PowerMonitorClient> receiver_{this};

    base::Lock is_shutdown_lock_;
    bool is_shutdown_ = false;

    bool last_reported_on_battery_power_state_ = false;

    DISALLOW_COPY_AND_ASSIGN(Client);
  };

  // This constructor is used by test code to mock the Client class.
  PowerMonitorBroadcastSource(
      std::unique_ptr<Client> client,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  Client* client_for_testing() const { return client_.get(); }

  bool IsOnBatteryPowerImpl() override;

  std::unique_ptr<Client> client_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(PowerMonitorBroadcastSource);
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_POWER_MONITOR_POWER_MONITOR_BROADCAST_SOURCE_H_
