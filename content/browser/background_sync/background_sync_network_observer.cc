// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_network_observer.h"

#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
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
    const base::RepeatingClosure& connection_changed_callback)
    : network_connection_tracker_(nullptr),
      connection_type_(network::mojom::ConnectionType::CONNECTION_UNKNOWN),
      connection_changed_callback_(connection_changed_callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(connection_changed_callback_);

  if (ServiceWorkerContext::IsServiceWorkerOnUIEnabled()) {
    RegisterWithNetworkConnectionTracker(GetNetworkConnectionTracker());
  } else {
    base::PostTaskAndReplyWithResult(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&GetNetworkConnectionTracker),
        base::BindOnce(&BackgroundSyncNetworkObserver::
                           RegisterWithNetworkConnectionTracker,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

BackgroundSyncNetworkObserver::~BackgroundSyncNetworkObserver() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  if (network_connection_tracker_)
    network_connection_tracker_->RemoveNetworkConnectionObserver(this);
}

void BackgroundSyncNetworkObserver::RegisterWithNetworkConnectionTracker(
    network::NetworkConnectionTracker* network_connection_tracker) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(network_connection_tracker);
  network_connection_tracker_ = network_connection_tracker;
  network_connection_tracker_->AddNetworkConnectionObserver(this);

  UpdateConnectionType();
}

void BackgroundSyncNetworkObserver::UpdateConnectionType() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  network::mojom::ConnectionType connection_type;
  bool synchronous_return = network_connection_tracker_->GetConnectionType(
      &connection_type,
      base::BindOnce(&BackgroundSyncNetworkObserver::OnConnectionChanged,
                     weak_ptr_factory_.GetWeakPtr()));
  if (synchronous_return)
    OnConnectionChanged(connection_type);
}

bool BackgroundSyncNetworkObserver::NetworkSufficient() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  return connection_type_ != network::mojom::ConnectionType::CONNECTION_NONE;
}

void BackgroundSyncNetworkObserver::OnConnectionChanged(
    network::mojom::ConnectionType connection_type) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
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
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (connection_type == connection_type_)
    return;

  connection_type_ = connection_type;
  NotifyConnectionChanged();
}

void BackgroundSyncNetworkObserver::NotifyConnectionChanged() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                connection_changed_callback_);
}

}  // namespace content
