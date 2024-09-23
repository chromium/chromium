// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_connection_tracker.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace network {

namespace {

// Wraps a |user_callback| when GetConnectionType() is called on a different
// thread than NetworkConnectionTracker's thread.
void OnGetConnectionType(
    scoped_refptr<base::TaskRunner> task_runner,
    NetworkConnectionTracker::ConnectionTypeCallback user_callback,
    network::mojom::ConnectionType connection_type) {
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](NetworkConnectionTracker::ConnectionTypeCallback callback,
             network::mojom::ConnectionType type) {
            std::move(callback).Run(type);
          },
          std::move(user_callback), connection_type));
}

static const int32_t kConnectionTypeInvalid = -1;

}  // namespace

NetworkConnectionTracker::NetworkConnectionTracker(BindingCallback callback)
    : bind_receiver_callback_(callback),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      connection_type_(kConnectionTypeInvalid),
      network_change_observer_list_(
          new base::ObserverListThreadSafe<NetworkConnectionObserver>(
              base::ObserverListPolicy::EXISTING_ONLY)),
      leaky_network_change_observer_list_(
          new base::ObserverListThreadSafe<NetworkConnectionObserver>(
              base::ObserverListPolicy::EXISTING_ONLY)) {
  Initialize();
  DCHECK(receiver_.is_bound());
}

NetworkConnectionTracker::~NetworkConnectionTracker() {
  // Don't check that the leaky list is empty.
  network_change_observer_list_->AssertEmpty();
}

bool NetworkConnectionTracker::GetConnectionType(
    network::mojom::ConnectionType* const type,
    ConnectionTypeCallback callback) {
  // |connection_type_| is initialized when NetworkService starts up. In most
  // cases, it won't be kConnectionTypeInvalid and code will return early.
  base::subtle::Atomic32 type_value =
      base::subtle::NoBarrier_Load(&connection_type_);
  if (type_value != kConnectionTypeInvalid) {
    *type = static_cast<network::mojom::ConnectionType>(type_value);
    return true;
  }
  base::AutoLock lock(lock_);
  // Check again after getting the lock, and return early if
  // OnInitialConnectionType() is called after first NoBarrier_Load.
  type_value = base::subtle::NoBarrier_Load(&connection_type_);
  if (type_value != kConnectionTypeInvalid) {
    *type = static_cast<network::mojom::ConnectionType>(type_value);
    return true;
  }
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    connection_type_callbacks_.push_back(base::BindOnce(
        &OnGetConnectionType, base::SequencedTaskRunner::GetCurrentDefault(),
        std::move(callback)));
  } else {
    connection_type_callbacks_.push_back(std::move(callback));
  }
  return false;
}

bool NetworkConnectionTracker::IsOffline() const {
  base::subtle::Atomic32 type_value =
      base::subtle::NoBarrier_Load(&connection_type_);
  if (type_value != kConnectionTypeInvalid) {
    auto type = static_cast<network::mojom::ConnectionType>(type_value);
    return type == network::mojom::ConnectionType::CONNECTION_NONE;
  }
  return true;
}

// static
bool NetworkConnectionTracker::IsConnectionCellular(
    const network::mojom::ConnectionType type) {
  switch (type) {
    case network::mojom::ConnectionType::CONNECTION_2G:
    case network::mojom::ConnectionType::CONNECTION_3G:
    case network::mojom::ConnectionType::CONNECTION_4G:
    case network::mojom::ConnectionType::CONNECTION_5G:
      return true;

    case network::mojom::ConnectionType::CONNECTION_UNKNOWN:
    case network::mojom::ConnectionType::CONNECTION_ETHERNET:
    case network::mojom::ConnectionType::CONNECTION_WIFI:
    case network::mojom::ConnectionType::CONNECTION_NONE:
    case network::mojom::ConnectionType::CONNECTION_BLUETOOTH:
      return false;
  }

  NOTREACHED() << "Unexpected connection type " << type;
}

void NetworkConnectionTracker::AddNetworkConnectionObserver(
    NetworkConnectionObserver* observer) {
  network_change_observer_list_->AddObserver(observer);
}

void NetworkConnectionTracker::AddLeakyNetworkConnectionObserver(
    NetworkConnectionObserver* observer) {
  leaky_network_change_observer_list_->AddObserver(observer);
}

void NetworkConnectionTracker::RemoveNetworkConnectionObserver(
    NetworkConnectionObserver* observer) {
  network_change_observer_list_->RemoveObserver(observer);
}

NetworkConnectionTracker::NetworkConnectionTracker()
    : connection_type_(kConnectionTypeInvalid),
      network_change_observer_list_(
          new base::ObserverListThreadSafe<NetworkConnectionObserver>(
              base::ObserverListPolicy::EXISTING_ONLY)),
      leaky_network_change_observer_list_(
          new base::ObserverListThreadSafe<NetworkConnectionObserver>(
              base::ObserverListPolicy::EXISTING_ONLY)) {}

void NetworkConnectionTracker::OnInitialConnectionType(
    network::mojom::ConnectionType type) {
  base::AutoLock lock(lock_);
  base::subtle::NoBarrier_Store(&connection_type_,
                                static_cast<base::subtle::Atomic32>(type));
  while (!connection_type_callbacks_.empty()) {
    std::move(connection_type_callbacks_.front()).Run(type);
    connection_type_callbacks_.pop_front();
  }
}

void NetworkConnectionTracker::OnNetworkChanged(
    network::mojom::ConnectionType type) {
  base::subtle::NoBarrier_Store(&connection_type_,
                                static_cast<base::subtle::Atomic32>(type));
  network_change_observer_list_->Notify(
      FROM_HERE, &NetworkConnectionObserver::OnConnectionChanged, type);
  leaky_network_change_observer_list_->Notify(
      FROM_HERE, &NetworkConnectionObserver::OnConnectionChanged, type);
}

void NetworkConnectionTracker::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!receiver_.is_bound());

  // Get mojo::Remote<NetworkChangeManager>.
  mojo::Remote<network::mojom::NetworkChangeManager> manager_remote;
  bind_receiver_callback_.Run(manager_remote.BindNewPipeAndPassReceiver());

  // Request notification from mojo::Remote<NetworkChangeManager>.
  manager_remote->RequestNotifications(receiver_.BindNewPipeAndPassRemote());

  // base::Unretained is safe as |receiver_| is owned by |this|.
  receiver_.set_disconnect_handler(
      base::BindOnce(&NetworkConnectionTracker::HandleNetworkServicePipeBroken,
                     base::Unretained(this)));
}

void NetworkConnectionTracker::HandleNetworkServicePipeBroken() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receiver_.reset();
  // Reset |connection_type_| to invalid, so future GetConnectionType() can be
  // delayed after network service has restarted, and that there isn't an
  // incorrectly cached state.
  base::subtle::NoBarrier_Store(&connection_type_, kConnectionTypeInvalid);
  Initialize();
}

}  // namespace network
