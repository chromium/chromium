// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/p2p/socket_dispatcher.h"

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/pass_key.h"
#include "services/network/public/cpp/p2p_param_traits.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mojo/mojo_binding_context.h"
#include "third_party/blink/renderer/platform/p2p/network_list_observer.h"
#include "third_party/blink/renderer/platform/p2p/socket_client_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

using PassKey = base::PassKey<P2PSocketDispatcher>;

const char P2PSocketDispatcher::kSupplementName[] = "P2PSocketDispatcher";

// static
P2PSocketDispatcher& P2PSocketDispatcher::From(MojoBindingContext& context) {
  auto* supplement =
      Supplement<MojoBindingContext>::From<P2PSocketDispatcher>(context);
  if (!supplement) {
    supplement = MakeGarbageCollected<P2PSocketDispatcher>(context, PassKey());
    ProvideTo(context, supplement);
  }
  return *supplement;
}

P2PSocketDispatcher::P2PSocketDispatcher(MojoBindingContext& context, PassKey)
    : Supplement(context),
      main_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      network_list_observers_(
          new base::ObserverListThreadSafe<blink::NetworkListObserver>()),
      network_notification_client_receiver_(this, &context) {}

P2PSocketDispatcher::~P2PSocketDispatcher() = default;

void P2PSocketDispatcher::AddNetworkListObserver(
    blink::NetworkListObserver* network_list_observer) {
  network_list_observers_->AddObserver(network_list_observer);
  PostCrossThreadTask(
      *main_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(&P2PSocketDispatcher::RequestNetworkEventsIfNecessary,
                          WrapCrossThreadWeakPersistent(this)));
}

void P2PSocketDispatcher::RemoveNetworkListObserver(
    blink::NetworkListObserver* network_list_observer) {
  network_list_observers_->RemoveObserver(network_list_observer);
}

mojo::SharedRemote<network::mojom::blink::P2PSocketManager>
P2PSocketDispatcher::GetP2PSocketManager() {
  base::AutoLock lock(p2p_socket_manager_lock_);
  if (!p2p_socket_manager_) {
    mojo::PendingRemote<network::mojom::blink::P2PSocketManager>
        p2p_socket_manager;
    p2p_socket_manager_receiver_ =
        p2p_socket_manager.InitWithNewPipeAndPassReceiver();
    p2p_socket_manager_ =
        mojo::SharedRemote<network::mojom::blink::P2PSocketManager>(
            std::move(p2p_socket_manager));
    p2p_socket_manager_.set_disconnect_handler(
        ConvertToBaseOnceCallback(
            CrossThreadBindOnce(&P2PSocketDispatcher::OnConnectionError,
                                WrapCrossThreadWeakPersistent(this))),
        main_task_runner_);
  }

  PostCrossThreadTask(
      *main_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(&P2PSocketDispatcher::RequestInterfaceIfNecessary,
                          WrapCrossThreadWeakPersistent(this)));
  return p2p_socket_manager_;
}

void P2PSocketDispatcher::NetworkListChanged(
    const Vector<net::NetworkInterface>& networks,
    const net::IPAddress& default_ipv4_local_address,
    const net::IPAddress& default_ipv6_local_address) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  networks_ = networks;
  default_ipv4_local_address_ = default_ipv4_local_address;
  default_ipv6_local_address_ = default_ipv6_local_address;

  // TODO(crbug.com/787254): Remove this helper when network_list_observer.h
  // gets moved from blink/public to blink/renderer, and operate over
  // WTF::Vector.
  std::vector<net::NetworkInterface> copy(networks.size());
  for (wtf_size_t i = 0; i < networks.size(); i++)
    copy[i] = networks[i];

  network_list_observers_->Notify(
      FROM_HERE, &blink::NetworkListObserver::OnNetworkListChanged,
      std::move(copy), default_ipv4_local_address, default_ipv6_local_address);
}

void P2PSocketDispatcher::RequestInterfaceIfNecessary() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (!p2p_socket_manager_receiver_.is_valid())
    return;

  GetSupplementable()->GetBrowserInterfaceBroker().GetInterface(
      std::move(p2p_socket_manager_receiver_));
}

void P2PSocketDispatcher::RequestNetworkEventsIfNecessary() {
  if (network_notification_client_receiver_.is_bound()) {
    // TODO(crbug.com/787254): Remove this helper when network_list_observer.h
    // gets moved from blink/public to blink/renderer, and operate over
    // WTF::Vector.
    std::vector<net::NetworkInterface> copy(networks_.size());
    for (wtf_size_t i = 0; i < networks_.size(); i++)
      copy[i] = networks_[i];

    network_list_observers_->Notify(
        FROM_HERE, &blink::NetworkListObserver::OnNetworkListChanged,
        std::move(copy), default_ipv4_local_address_,
        default_ipv6_local_address_);
  } else {
    GetP2PSocketManager()->StartNetworkNotifications(
        network_notification_client_receiver_.BindNewPipeAndPassRemote(
            GetSupplementable()->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }
}

void P2PSocketDispatcher::OnConnectionError() {
  base::AutoLock lock(p2p_socket_manager_lock_);
  p2p_socket_manager_.reset();
  // Attempt to reconnect in case the network service crashed in his being
  // restarted.
  PostCrossThreadTask(
      *main_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(&P2PSocketDispatcher::ReconnectP2PSocketManager,
                          WrapCrossThreadWeakPersistent(this)));
}

void P2PSocketDispatcher::ReconnectP2PSocketManager() {
  network_notification_client_receiver_.reset();
  if (GetSupplementable()->IsContextDestroyed())
    return;
  GetP2PSocketManager()->StartNetworkNotifications(
      network_notification_client_receiver_.BindNewPipeAndPassRemote(
          GetSupplementable()->GetTaskRunner(TaskType::kNetworking)));
}

void P2PSocketDispatcher::Trace(Visitor* visitor) const {
  Supplement::Trace(visitor);
  NetworkListManager::Trace(visitor);
  visitor->Trace(network_notification_client_receiver_);
}

}  // namespace blink
