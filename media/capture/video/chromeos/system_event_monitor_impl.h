// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_SYSTEM_EVENT_MONITOR_IMPL_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_SYSTEM_EVENT_MONITOR_IMPL_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/mojo_service_manager/mojom/mojo_service_manager.mojom.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "media/capture/capture_export.h"
#include "media/capture/video/chromeos/mojom/system_event_monitor.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"

namespace media {

// Constructor, destructor and all of the member functions have to be invoked on
// the ui thread.
class CAPTURE_EXPORT SystemEventMonitorImpl final
    : public cros::mojom::CrosSystemEventMonitor,
      public chromeos::mojo_service_manager::mojom::ServiceProvider,
      public chromeos::PowerManagerClient::Observer {
 public:
  SystemEventMonitorImpl();

  ~SystemEventMonitorImpl() override;

  SystemEventMonitorImpl(const SystemEventMonitorImpl&) = delete;
  SystemEventMonitorImpl& operator=(const SystemEventMonitorImpl&) = delete;

  // Implementation of cros::mojom::CrosSystemEventMonitor
  void AddDisplayObserver(
      mojo::PendingRemote<cros::mojom::CrosDisplayObserver> observer) override;

  void AddLidObserver(
      mojo::PendingRemote<cros::mojom::CrosLidObserver> observer) override;

  void AddPowerObserver(
      const std::string& info,
      mojo::PendingRemote<cros::mojom::CrosPowerObserver> observer) override;

  void NotifyDeviceChanged(cros::mojom::DeviceType type) override;

 private:
  class DisplayObserver;
  class PowerObserver;

  void Request(
      chromeos::mojo_service_manager::mojom::ProcessIdentityPtr identity,
      mojo::ScopedMessagePipeHandle receiver) override;

  void LidEventReceived(chromeos::PowerManagerClient::LidState state,
                        base::TimeTicks timestamp) final;

  void RemoveDisplayObserver(DisplayObserver* observer);

  void RemoveLidObserver(mojo::RemoteSetElementId id);

  void RemovePowerObserver(PowerObserver* observer);

  cros::mojom::LidState lid_state_ = cros::mojom::LidState::kNotPresent;

  const scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  base::flat_map<DisplayObserver*, std::unique_ptr<DisplayObserver>>
      display_observers_;

  mojo::RemoteSet<cros::mojom::CrosLidObserver> lid_observers_;

  base::flat_map<PowerObserver*, std::unique_ptr<PowerObserver>>
      power_observers_;

  mojo::ReceiverSet<cros::mojom::CrosSystemEventMonitor> receiver_set_;

  // Receiver for mojo service manager service provider.
  mojo::Receiver<chromeos::mojo_service_manager::mojom::ServiceProvider>
      provider_receiver_{this};

  base::WeakPtrFactory<SystemEventMonitorImpl> weak_factory_{this};
};

// Constructor, destructor and all of the member functions have to be invoked on
// the ui thread.
class SystemEventMonitorImpl::DisplayObserver
    : public display::DisplayObserver {
 public:
  DisplayObserver(
      mojo::PendingRemote<cros::mojom::CrosDisplayObserver> observer,
      base::OnceCallback<void(DisplayObserver*)> cleanup_callback);

  ~DisplayObserver() override;

  DisplayObserver(const DisplayObserver&) = delete;
  DisplayObserver& operator=(const DisplayObserver&) = delete;

 private:
  // DisplayObserver implementations.
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  void OnRemoteObserverDisconnected(
      base::OnceCallback<void(DisplayObserver*)> cleanup_callback);

  display::ScopedOptionalDisplayObserver display_observer_{this};

  mojo::Remote<cros::mojom::CrosDisplayObserver> display_remote_observer_;

  base::WeakPtrFactory<DisplayObserver> weak_ptr_factory_{this};
};

// Constructor, destructor and all of the member functions have to be invoked on
// the ui thread.
class SystemEventMonitorImpl::PowerObserver
    : public chromeos::PowerManagerClient::Observer {
 public:
  PowerObserver(const std::string& client_name,
                mojo::PendingRemote<cros::mojom::CrosPowerObserver> observer,
                base::OnceCallback<void(PowerObserver*)> cleanup_callback);
  ~PowerObserver() override;

  PowerObserver(const PowerObserver&) = delete;
  PowerObserver& operator=(const PowerObserver&) = delete;

  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void SuspendDone(base::TimeDelta sleep_duration) override;

 private:
  void UnblockSuspend(base::UnguessableToken token);

  void OnRemoteObserverDisconnected(
      base::OnceCallback<void(PowerObserver*)> cleanup_callback);

  std::string client_name_;

  base::flat_set<base::UnguessableToken> block_suspend_tokens_;

  mojo::Remote<cros::mojom::CrosPowerObserver> power_remote_observer_;

  base::WeakPtrFactory<PowerObserver> weak_ptr_factory_{this};
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_SYSTEM_EVENT_MONITOR_IMPL_H_
