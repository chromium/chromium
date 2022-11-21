// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_network_observer.h"

#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"

namespace content {

// static
bool BackgroundSyncNetworkObserver::ignore_network_changes_ = false;

// static
void BackgroundSyncNetworkObserver::SetIgnoreNetworkChangesForTests(
    bool ignore) {
  ignore_network_changes_ = ignore;
}

BackgroundSyncNetworkObserver::BackgroundSyncNetworkObserver(
    base::RepeatingClosure connection_changed_callback)
    : network_connection_tracker_(nullptr),
      connection_type_(network::mojom::ConnectionType::CONNECTION_UNKNOWN),
      connection_changed_callback_(std::move(connection_changed_callback)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(connection_changed_callback_);

  RegisterWithNetworkConnectionTracker(GetNetworkConnectionTracker());
}

BackgroundSyncNetworkObserver::~BackgroundSyncNetworkObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (network_connection_tracker_)
    network_connection_tracker_->RemoveNetworkConnectionObserver(this);
}

void BackgroundSyncNetworkObserver::RegisterWithNetworkConnectionTracker(
    network::NetworkConnectionTracker* network_connection_tracker) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(network_connection_tracker);
  network_connection_tracker_ = network_connection_tracker;
  network_connection_tracker_->AddNetworkConnectionObserver(this);

  UpdateConnectionType();
}

void BackgroundSyncNetworkObserver::UpdateConnectionType() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  network::mojom::ConnectionType connection_type;
  bool synchronous_return = network_connection_tracker_->GetConnectionType(
      &connection_type,
      base::BindOnce(&BackgroundSyncNetworkObserver::OnConnectionChanged,
                     weak_ptr_factory_.GetWeakPtr()));
  if (synchronous_return)
    OnConnectionChanged(connection_type);
}

bool BackgroundSyncNetworkObserver::NetworkSufficient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return connection_type_ != network::mojom::ConnectionType::CONNECTION_NONE;
}

void BackgroundSyncNetworkObserver::OnConnectionChanged(
    network::mojom::ConnectionType connection_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ignore_network_changes_)
    return;
  NotifyManagerIfConnectionChanged(connection_type);
}

void BackgroundSyncNetworkObserver::NotifyManagerIfConnectionChangedForTesting(
    network::mojom::ConnectionType connection_type) {
  NotifyManagerIfConnectionChanged(connection_type);
}

void BackgroundSyncNetworkObserver::NotifyManagerIfConnectionChanged(
    network::mojom::ConnectionType connection_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (connection_type == connection_type_)
    return;

  connection_type_ = connection_type;
  NotifyConnectionChanged();
}

void BackgroundSyncNetworkObserver::NotifyConnectionChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, connection_changed_callback_);
}

}  // namespace content
