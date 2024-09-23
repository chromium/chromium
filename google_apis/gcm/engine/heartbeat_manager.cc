// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/heartbeat_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/power_monitor/power_monitor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "net/base/network_change_notifier.h"

namespace gcm {

namespace {
// The default heartbeat when on a mobile or unknown network .
const int kCellHeartbeatDefaultMs = 1000 * 60 * 28;  // 28 minutes.
// The default heartbeat when on WiFi (also used for ethernet).
const int kWifiHeartbeatDefaultMs = 1000 * 60 * 15;  // 15 minutes.
// The default heartbeat ack interval.
const int kHeartbeatAckDefaultMs = 1000 * 60 * 1;  // 1 minute.
// Minimum allowed client default heartbeat interval.
const int kMinClientHeartbeatIntervalMs = 1000 * 30;  // 30 seconds.
// Minimum time spent sleeping before we force a new heartbeat.
const int kMinSuspendTimeMs = 1000 * 10; // 10 seconds.

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
// The period at which to check if the heartbeat time has passed. Used to
// protect against platforms where the timer is delayed by the system being
// suspended.  Only needed on linux because the other OSes provide a standard
// way to be notified of system suspend and resume events.
const int kHeartbeatMissedCheckMs = 1000 * 60 * 5;  // 5 minutes.
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace

HeartbeatManager::HeartbeatManager(
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    scoped_refptr<base::SequencedTaskRunner> maybe_power_wrapped_io_task_runner)
    : waiting_for_ack_(false),
      heartbeat_interval_ms_(0),
      server_interval_ms_(0),
      client_interval_ms_(0),
      io_task_runner_(std::move(io_task_runner)),
      heartbeat_timer_(new base::RetainingOneShotTimer()) {
  DCHECK(io_task_runner_);
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  // Set the heartbeat timer task runner to |maybe_power_wrapped_io_task_runner|
  // so that a delayed task posted to it can wake the system up from sleep to
  // perform the task.
  heartbeat_timer_->SetTaskRunner(
      std::move(maybe_power_wrapped_io_task_runner));
}

HeartbeatManager::~HeartbeatManager() {
  // Stop listening for system suspend and resume events.
  base::PowerMonitor::GetInstance()->RemovePowerSuspendObserver(this);
}

void HeartbeatManager::Start(
    const base::RepeatingClosure& send_heartbeat_callback,
    const ReconnectCallback& trigger_reconnect_callback) {
  DCHECK(!send_heartbeat_callback.is_null());
  DCHECK(!trigger_reconnect_callback.is_null());
  send_heartbeat_callback_ = send_heartbeat_callback;
  trigger_reconnect_callback_ = trigger_reconnect_callback;

  // Listen for system suspend and resume events.
  base::PowerMonitor::GetInstance()->AddPowerSuspendObserver(this);

  // Calculated the heartbeat interval just before we start the timer.
  UpdateHeartbeatInterval();

  // Kicks off the timer.
  waiting_for_ack_ = false;
  RestartTimer();
}

void HeartbeatManager::Stop() {
  heartbeat_expected_time_ = base::Time();
  heartbeat_interval_ms_ = 0;
  heartbeat_timer_->Stop();
  waiting_for_ack_ = false;

  base::PowerMonitor::GetInstance()->RemovePowerSuspendObserver(this);
}

void HeartbeatManager::OnHeartbeatAcked() {
  if (!heartbeat_timer_->IsRunning())
    return;

  DCHECK(!send_heartbeat_callback_.is_null());
  DCHECK(!trigger_reconnect_callback_.is_null());
  waiting_for_ack_ = false;
  RestartTimer();
}

void HeartbeatManager::UpdateHeartbeatConfig(
    const mcs_proto::HeartbeatConfig& config) {
  if (!config.IsInitialized() ||
      !config.has_interval_ms() ||
      config.interval_ms() <= 0) {
    return;
  }
  DVLOG(1) << "Updating server heartbeat interval to " << config.interval_ms();
  server_interval_ms_ = config.interval_ms();

  // Make sure heartbeat interval is recalculated when new server interval is
  // available.
  UpdateHeartbeatInterval();
}

base::TimeTicks HeartbeatManager::GetNextHeartbeatTime() const {
  if (heartbeat_timer_->IsRunning())
    return heartbeat_timer_->desired_run_time();
  else
    return base::TimeTicks();
}

void HeartbeatManager::UpdateHeartbeatTimer(
    std::unique_ptr<base::RetainingOneShotTimer> timer) {
  bool was_running = heartbeat_timer_->IsRunning();
  base::TimeDelta remaining_delay =
      heartbeat_timer_->desired_run_time() - base::TimeTicks::Now();
  base::RepeatingClosure timer_task = heartbeat_timer_->user_task();

  heartbeat_timer_->Stop();
  heartbeat_timer_ = std::move(timer);

  if (was_running)
    heartbeat_timer_->Start(FROM_HERE, remaining_delay, timer_task);
}

void HeartbeatManager::OnSuspend() {
  // The system is going to sleep. Record the time, so on resume we know how
  // much time the machine was suspended.
  suspend_time_ = base::Time::Now();
}

void HeartbeatManager::OnResume() {
  // The system just resumed from sleep. It's likely that the connection to
  // MCS was silently lost during that time, even if a heartbeat is not yet
  // due. Force a heartbeat to detect if the connection is still good.
  base::TimeDelta elapsed = base::Time::Now() - suspend_time_;

  // Make sure a minimum amount of time has passed before forcing a heartbeat to
  // avoid any tight loop scenarios.
  // If the |send_heartbeat_callback_| is null, it means the heartbeat manager
  // hasn't been started, so do nothing.
  if (elapsed > base::Milliseconds(kMinSuspendTimeMs) &&
      !send_heartbeat_callback_.is_null())
    OnHeartbeatTriggered();
}

void HeartbeatManager::OnHeartbeatTriggered() {
  // Reset the weak pointers used for heartbeat checks.
  weak_ptr_factory_.InvalidateWeakPtrs();

  if (waiting_for_ack_) {
    LOG(WARNING) << "Lost connection to MCS, reconnecting.";
    ResetConnection(ConnectionFactory::HEARTBEAT_FAILURE);
    return;
  }

  waiting_for_ack_ = true;
  RestartTimer();
  send_heartbeat_callback_.Run();
}

void HeartbeatManager::RestartTimer() {
  int interval_ms = heartbeat_interval_ms_;
  if (waiting_for_ack_) {
    interval_ms = kHeartbeatAckDefaultMs;
    DVLOG(1) << "Resetting timer for ack within " << interval_ms << " ms.";
  } else {
    DVLOG(1) << "Sending next heartbeat in " << interval_ms << " ms.";
  }

  heartbeat_expected_time_ =
      base::Time::Now() + base::Milliseconds(interval_ms);
  heartbeat_timer_->Start(
      FROM_HERE, base::Milliseconds(interval_ms),
      base::BindRepeating(&HeartbeatManager::OnHeartbeatTriggered,
                          weak_ptr_factory_.GetWeakPtr()));

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // Windows, Mac, Android, iOS, and Chrome OS all provide a way to be notified
  // when the system is suspending or resuming.  The only one that does not is
  // Linux so we need to poll to check for missed heartbeats.
  io_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&HeartbeatManager::CheckForMissedHeartbeat,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Milliseconds(kHeartbeatMissedCheckMs));
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
}

void HeartbeatManager::CheckForMissedHeartbeat() {
  // If there's no heartbeat pending, return without doing anything.
  if (heartbeat_expected_time_.is_null())
    return;

  // If the heartbeat has been missed, manually trigger it.
  if (base::Time::Now() > heartbeat_expected_time_) {
    OnHeartbeatTriggered();
    return;
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // Otherwise check again later.
  io_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&HeartbeatManager::CheckForMissedHeartbeat,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Milliseconds(kHeartbeatMissedCheckMs));
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
}

void HeartbeatManager::UpdateHeartbeatInterval() {
  // Server interval takes precedence over client interval, even if the latter
  // is less.
  if (server_interval_ms_ != 0) {
    // If a server interval is set, it overrides any local one.
    heartbeat_interval_ms_ = server_interval_ms_;
  } else if (HasClientHeartbeatInterval() &&
             (client_interval_ms_ < heartbeat_interval_ms_ ||
              heartbeat_interval_ms_ == 0)) {
    // Client interval might have been adjusted up, which should only take
    // effect during a reconnection.
    heartbeat_interval_ms_ = client_interval_ms_;
  } else if (heartbeat_interval_ms_ == 0) {
    // If interval is still 0, recalculate it based on network type.
    heartbeat_interval_ms_ = GetDefaultHeartbeatInterval();
  }
  DCHECK_GT(heartbeat_interval_ms_, 0);
}

int HeartbeatManager::GetDefaultHeartbeatInterval() {
  // For unknown connections, use the longer cellular heartbeat interval.
  int heartbeat_interval_ms = kCellHeartbeatDefaultMs;
  if (net::NetworkChangeNotifier::GetConnectionType() ==
          net::NetworkChangeNotifier::CONNECTION_WIFI ||
      net::NetworkChangeNotifier::GetConnectionType() ==
          net::NetworkChangeNotifier::CONNECTION_ETHERNET) {
    heartbeat_interval_ms = kWifiHeartbeatDefaultMs;
  }
  return heartbeat_interval_ms;
}

int HeartbeatManager::GetMaxClientHeartbeatIntervalMs() {
  return GetDefaultHeartbeatInterval();
}

int HeartbeatManager::GetMinClientHeartbeatIntervalMs() {
  // Returning a constant. This should be adjusted for connection type, like the
  // default/max interval.
  return kMinClientHeartbeatIntervalMs;
}

void HeartbeatManager::SetClientHeartbeatIntervalMs(int interval_ms) {
  if ((interval_ms != 0 && !IsValidClientHeartbeatInterval(interval_ms)) ||
      interval_ms == client_interval_ms_) {
    return;
  }

  client_interval_ms_ = interval_ms;
  // Only reset connection if the new heartbeat interval is shorter. If it is
  // longer, the connection will reset itself at some point and interval will be
  // fixed.
  if (client_interval_ms_ > 0 && client_interval_ms_ < heartbeat_interval_ms_) {
    ResetConnection(ConnectionFactory::NEW_HEARTBEAT_INTERVAL);
  }
}

int HeartbeatManager::GetClientHeartbeatIntervalMs() {
  return client_interval_ms_;
}

bool HeartbeatManager::HasClientHeartbeatInterval() {
  return client_interval_ms_ != 0;
}

bool HeartbeatManager::IsValidClientHeartbeatInterval(int interval) {
  int max_heartbeat_interval = GetDefaultHeartbeatInterval();
  return kMinClientHeartbeatIntervalMs <= interval &&
      interval <= max_heartbeat_interval;
}

void HeartbeatManager::ResetConnection(
    ConnectionFactory::ConnectionResetReason reason) {
  Stop();
  trigger_reconnect_callback_.Run(reason);
}

}  // namespace gcm
