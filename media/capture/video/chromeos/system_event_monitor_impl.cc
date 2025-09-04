// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/system_event_monitor_impl.h"

#include "base/system/system_monitor.h"
#include "base/task/bind_post_task.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "third_party/cros_system_api/mojo/service_constants.h"

namespace media {
namespace {

cros::mojom::LidState ConvertLidState(
    chromeos::PowerManagerClient::LidState state) {
  switch (state) {
    case chromeos::PowerManagerClient::LidState::OPEN:
      return cros::mojom::LidState::kOpen;
    case chromeos::PowerManagerClient::LidState::CLOSED:
      return cros::mojom::LidState::kClosed;
    case chromeos::PowerManagerClient::LidState::NOT_PRESENT:
      return cros::mojom::LidState::kNotPresent;
  }
  return cros::mojom::LidState::kNotPresent;
}

cros::mojom::ClockwiseRotation ConvertRotation(
    display::Display::Rotation rotation) {
  switch (rotation) {
    case display::Display::Rotation::ROTATE_0:
      return cros::mojom::ClockwiseRotation::kRotate0;
    case display::Display::Rotation::ROTATE_90:
      return cros::mojom::ClockwiseRotation::kRotate90;
    case display::Display::Rotation::ROTATE_180:
      return cros::mojom::ClockwiseRotation::kRotate180;
    case display::Display::Rotation::ROTATE_270:
      return cros::mojom::ClockwiseRotation::kRotate270;
  }
  return cros::mojom::ClockwiseRotation::kRotate0;
}

}  // namespace

SystemEventMonitorImpl::SystemEventMonitorImpl() {
  CHECK(ash::mojo_service_manager::IsServiceManagerBound());
  auto* proxy = ash::mojo_service_manager::GetServiceManagerProxy();
  proxy->Register(
      /*service_name=*/chromeos::mojo_services::kCrosSystemEventMonitor,
      provider_receiver_.BindNewPipeAndPassRemote());

  lid_observers_.set_disconnect_handler(base::BindRepeating(
      &SystemEventMonitorImpl::RemoveLidObserver, weak_factory_.GetWeakPtr()));

  chromeos::PowerManagerClient* power_manager_client =
      chromeos::PowerManagerClient::Get();
  if (!power_manager_client) {
    // power_manager_client may be NULL in unittests.
    return;
  }
  power_manager_client->AddObserver(this);
}

SystemEventMonitorImpl::~SystemEventMonitorImpl() {
  chromeos::PowerManagerClient* power_manager_client =
      chromeos::PowerManagerClient::Get();
  // power_manager_client may be NULL in unittests.
  if (!power_manager_client) {
    return;
  }
  power_manager_client->RemoveObserver(this);
}

void SystemEventMonitorImpl::Request(
    chromeos::mojo_service_manager::mojom::ProcessIdentityPtr identity,
    mojo::ScopedMessagePipeHandle receiver) {
  receiver_set_.Add(this,
                    mojo::PendingReceiver<cros::mojom::CrosSystemEventMonitor>(
                        std::move(receiver)));
}

void SystemEventMonitorImpl::AddDisplayObserver(
    mojo::PendingRemote<cros::mojom::CrosDisplayObserver> observer) {
  std::unique_ptr<DisplayObserver> display_observer =
      std::make_unique<DisplayObserver>(
          std::move(observer),
          base::BindOnce(&SystemEventMonitorImpl::RemoveDisplayObserver,
                         weak_factory_.GetWeakPtr()));
  display_observers_.emplace(display_observer.get(),
                             std::move(display_observer));
}

void SystemEventMonitorImpl::AddLidObserver(
    mojo::PendingRemote<cros::mojom::CrosLidObserver> observer) {
  auto id = lid_observers_.Add(std::move(observer));
  lid_observers_.Get(id)->OnLidStateChanged(lid_state_);
}

void SystemEventMonitorImpl::AddPowerObserver(
    const std::string& info,
    mojo::PendingRemote<cros::mojom::CrosPowerObserver> observer) {
  std::unique_ptr<PowerObserver> power_observer =
      std::make_unique<PowerObserver>(
          info, std::move(observer),
          base::BindOnce(&SystemEventMonitorImpl::RemovePowerObserver,
                         weak_factory_.GetWeakPtr()));
  power_observers_.emplace(power_observer.get(), std::move(power_observer));
}

void SystemEventMonitorImpl::NotifyDeviceChanged(cros::mojom::DeviceType type) {
  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  // |monitor| might be nullptr in unittest.
  if (!monitor) {
    return;
  }
  base::SystemMonitor::DeviceType monitor_type =
      base::SystemMonitor::DeviceType::DEVTYPE_UNKNOWN;
  switch (type) {
    case cros::mojom::DeviceType::kAudio:
      monitor_type = base::SystemMonitor::DeviceType::DEVTYPE_AUDIO;
      break;
    case cros::mojom::DeviceType::kVideoCapture:
      monitor_type = base::SystemMonitor::DeviceType::DEVTYPE_VIDEO_CAPTURE;
      break;
    case cros::mojom::DeviceType::kUnkown:
      monitor_type = base::SystemMonitor::DeviceType::DEVTYPE_UNKNOWN;
      break;
  }
  monitor->ProcessDevicesChanged(monitor_type);
}

void SystemEventMonitorImpl::LidEventReceived(
    chromeos::PowerManagerClient::LidState state,
    base::TimeTicks timestamp) {
  cros::mojom::LidState new_state = ConvertLidState(state);
  if (lid_state_ == new_state) {
    return;
  }
  lid_state_ = new_state;
  for (auto& observer : lid_observers_) {
    observer->OnLidStateChanged(new_state);
  }
}

void SystemEventMonitorImpl::RemoveDisplayObserver(DisplayObserver* observer) {
  display_observers_.erase(observer);
}

void SystemEventMonitorImpl::RemoveLidObserver(mojo::RemoteSetElementId id) {
  lid_observers_.Remove(id);
}

void SystemEventMonitorImpl::RemovePowerObserver(PowerObserver* observer) {
  power_observers_.erase(observer);
}

SystemEventMonitorImpl::DisplayObserver::DisplayObserver(
    mojo::PendingRemote<cros::mojom::CrosDisplayObserver> observer,
    base::OnceCallback<void(DisplayObserver*)> cleanup_callback)
    : display_remote_observer_(std::move(observer)) {
  display_remote_observer_.set_disconnect_handler(base::BindOnce(
      &DisplayObserver::OnRemoteObserverDisconnected,
      weak_ptr_factory_.GetWeakPtr(), std::move(cleanup_callback)));
  // |display::Screen| is not ready when |SystemEventMonitorImpl| initializes.
  // Therefore, we defer the observation and assume that |display::Screen| is
  // ready when |SystemEventMonitorImpl::AddDisplayObserver| is invoked and let
  // |DisplayObserver| observe it.
  display::Screen* screen = display::Screen::GetScreen();
  if (!screen) {
    // screen may be NULL in unittests.
    LOG(WARNING) << "Screen has not been initialized yet.";
    return;
  }
  display::Display display = screen->GetPrimaryDisplay();
  if (!display.IsInternal()) {
    return;
  }
  display_remote_observer_->OnDisplayRotationChanged(
      ConvertRotation(display.rotation()));
}

SystemEventMonitorImpl::DisplayObserver::~DisplayObserver() = default;

void SystemEventMonitorImpl::DisplayObserver::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t metrics) {
  if (!(metrics & DISPLAY_METRIC_ROTATION)) {
    return;
  }
  if (!display.IsInternal()) {
    return;
  }
  display_remote_observer_->OnDisplayRotationChanged(
      ConvertRotation(display.rotation()));
}

void SystemEventMonitorImpl::DisplayObserver::OnRemoteObserverDisconnected(
    base::OnceCallback<void(DisplayObserver*)> cleanup_callback) {
  std::move(cleanup_callback).Run(this);
}

SystemEventMonitorImpl::PowerObserver::PowerObserver(
    const std::string& client_name,
    mojo::PendingRemote<cros::mojom::CrosPowerObserver> observer,
    base::OnceCallback<void(PowerObserver*)> cleanup_callback)
    : client_name_(client_name), power_remote_observer_(std::move(observer)) {
  power_remote_observer_.set_disconnect_handler(base::BindOnce(
      &PowerObserver::OnRemoteObserverDisconnected,
      weak_ptr_factory_.GetWeakPtr(), std::move(cleanup_callback)));

  chromeos::PowerManagerClient* power_manager_client =
      chromeos::PowerManagerClient::Get();
  if (!power_manager_client) {
    // power_manager_client may be NULL in unittests.
    return;
  }
  power_manager_client->AddObserver(this);
}

SystemEventMonitorImpl::PowerObserver::~PowerObserver() {
  chromeos::PowerManagerClient* power_manager_client =
      chromeos::PowerManagerClient::Get();
  if (!power_manager_client) {
    // power_manager_client may be NULL in unittests.
    return;
  }
  for (auto& token : block_suspend_tokens_) {
    chromeos::PowerManagerClient::Get()->UnblockSuspend(token);
  }
  power_manager_client->RemoveObserver(this);
}

void SystemEventMonitorImpl::PowerObserver::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  base::UnguessableToken token = base::UnguessableToken::Create();
  chromeos::PowerManagerClient::Get()->BlockSuspend(token, client_name_);
  power_remote_observer_->OnSystemSuspend(base::BindPostTaskToCurrentDefault(
      base::BindOnce(&PowerObserver::UnblockSuspend,
                     weak_ptr_factory_.GetWeakPtr(), token)));
  block_suspend_tokens_.insert(token);
}

void SystemEventMonitorImpl::PowerObserver::SuspendDone(
    base::TimeDelta sleep_duration) {
  power_remote_observer_->OnSystemResume();
}

void SystemEventMonitorImpl::PowerObserver::UnblockSuspend(
    base::UnguessableToken token) {
  chromeos::PowerManagerClient::Get()->UnblockSuspend(token);
  block_suspend_tokens_.erase(token);
}

void SystemEventMonitorImpl::PowerObserver::OnRemoteObserverDisconnected(
    base::OnceCallback<void(PowerObserver*)> cleanup_callback) {
  std::move(cleanup_callback).Run(this);
}

}  // namespace media
