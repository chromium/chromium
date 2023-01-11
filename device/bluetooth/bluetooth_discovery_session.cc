// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_discovery_session.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_discovery_filter.h"

namespace device {

BluetoothDiscoverySession::BluetoothDiscoverySession(
    scoped_refptr<BluetoothAdapter> adapter,
    std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter)
    : status_(SessionStatus::PENDING_START),
      is_stop_in_progress_(false),
      adapter_(adapter),
      discovery_filter_(discovery_filter.release()) {
  DCHECK(adapter_.get());
}

BluetoothDiscoverySession::~BluetoothDiscoverySession() {
  if (IsActive())
    Stop();
}

bool BluetoothDiscoverySession::IsActive() const {
  return status_ != SessionStatus::INACTIVE;
}

void BluetoothDiscoverySession::PendingSessionsStarting() {
  if (status_ == SessionStatus::PENDING_START)
    status_ = SessionStatus::STARTING;
}

void BluetoothDiscoverySession::StartingSessionsScanning() {
  if (status_ == SessionStatus::STARTING)
    status_ = SessionStatus::SCANNING;
}

void BluetoothDiscoverySession::Stop(base::OnceClosure success_callback,
                                     ErrorCallback error_callback) {
  if (!IsActive()) {
    DVLOG(1) << "Discovery session not active. Cannot stop.";
    std::move(error_callback).Run();
    return;
  }

  if (is_stop_in_progress_) {
    LOG(WARNING) << "Discovery session Stop in progress.";
    std::move(error_callback).Run();
    return;
  }

  is_stop_in_progress_ = true;

  DVLOG(1) << "Stopping device discovery session.";
  base::OnceClosure deactive_discovery_session =
      base::BindOnce(&BluetoothDiscoverySession::DeactivateDiscoverySession,
                     weak_ptr_factory_.GetWeakPtr());

  MarkAsInactive();

  // Create a callback that runs
  // BluetoothDiscoverySession::DeactivateDiscoverySession if the session
  // still exists, but always runs success_callback.
  base::OnceClosure discovery_session_removed_callback = base::BindOnce(
      &BluetoothDiscoverySession::OnDiscoverySessionRemoved,
      weak_ptr_factory_.GetWeakPtr(), std::move(deactive_discovery_session),
      std::move(success_callback));
  adapter_->RemoveDiscoverySession(
      this, std::move(discovery_session_removed_callback),
      base::BindOnce(
          &BluetoothDiscoverySession::OnDiscoverySessionRemovalFailed,
          weak_ptr_factory_.GetWeakPtr(), std::move(error_callback)));
}

// static
void BluetoothDiscoverySession::OnDiscoverySessionRemoved(
    base::WeakPtr<BluetoothDiscoverySession> session,
    base::OnceClosure deactivate_discovery_session,
    base::OnceClosure success_callback) {
  if (session)
    session->is_stop_in_progress_ = false;
  std::move(deactivate_discovery_session).Run();
  std::move(success_callback).Run();
}

// static
void BluetoothDiscoverySession::OnDiscoverySessionRemovalFailed(
    base::WeakPtr<BluetoothDiscoverySession> session,
    base::OnceClosure error_callback,
    UMABluetoothDiscoverySessionOutcome outcome) {
  if (session)
    session->is_stop_in_progress_ = false;
  std::move(error_callback).Run();
}

void BluetoothDiscoverySession::DeactivateDiscoverySession() {
  MarkAsInactive();
  discovery_filter_.reset();
}

void BluetoothDiscoverySession::MarkAsInactive() {
  status_ = SessionStatus::INACTIVE;
}

const BluetoothDiscoveryFilter* BluetoothDiscoverySession::GetDiscoveryFilter()
    const {
  return discovery_filter_.get();
}

base::WeakPtr<BluetoothDiscoverySession>
BluetoothDiscoverySession::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace device
