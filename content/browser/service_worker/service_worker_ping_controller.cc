// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_ping_controller.h"

#include "content/browser/service_worker/service_worker_version.h"

namespace content {

namespace {
// Timeout for waiting for a response to a ping.
constexpr base::TimeDelta kPingTimeout = base::Seconds(30);
}  // namespace

ServiceWorkerPingController::ServiceWorkerPingController(
    ServiceWorkerVersion* version)
    : version_(version) {}

ServiceWorkerPingController::~ServiceWorkerPingController() = default;

void ServiceWorkerPingController::Activate() {
  ping_state_ = PingState::kPinging;
}

void ServiceWorkerPingController::Deactivate() {
  ClearLastPingTime();
  ping_state_ = PingState::kNotPinging;
}

void ServiceWorkerPingController::OnPongReceived() {
  ClearLastPingTime();
}

bool ServiceWorkerPingController::IsActivated() const {
  return ping_state_ == PingState::kPinging;
}

bool ServiceWorkerPingController::IsTimedOut() const {
  return ping_state_ == PingState::kPingTimedOut;
}

void ServiceWorkerPingController::CheckPingStatus() {
  if (version_->GetTickDuration(last_ping_time_) > kPingTimeout) {
    ping_state_ = PingState::kPingTimedOut;
    version_->OnPingTimeout();
    return;
  }

  // Check if we want to send a next ping.
  if (ping_state_ != PingState::kPinging || !last_ping_time_.is_null())
    return;

  version_->PingWorker();
  version_->RestartTick(&last_ping_time_);
}

void ServiceWorkerPingController::SimulateTimeoutForTesting() {
  version_->PingWorker();
  ping_state_ = PingState::kPingTimedOut;
  version_->OnPingTimeout();
}

void ServiceWorkerPingController::ClearLastPingTime() {
  last_ping_time_ = base::TimeTicks();
}

}  // namespace content
