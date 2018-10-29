// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/p2p/socket_dispatcher.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "content/child/child_process.h"
#include "content/child/child_thread_impl.h"
#include "content/public/common/service_names.mojom.h"
#include "content/renderer/p2p/network_list_observer.h"
#include "content/renderer/p2p/socket_client_impl.h"
#include "content/renderer/render_view_impl.h"
#include "services/network/public/cpp/p2p_param_traits.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

P2PSocketDispatcher::P2PSocketDispatcher()
    : main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      network_list_observers_(
          new base::ObserverListThreadSafe<NetworkListObserver>()),
      network_notification_client_binding_(this) {
}

P2PSocketDispatcher::~P2PSocketDispatcher() {
}

void P2PSocketDispatcher::AddNetworkListObserver(
    NetworkListObserver* network_list_observer) {
  network_list_observers_->AddObserver(network_list_observer);
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&P2PSocketDispatcher::RequestNetworkEventsIfNecessary,
                     this));
}

void P2PSocketDispatcher::RemoveNetworkListObserver(
    NetworkListObserver* network_list_observer) {
  network_list_observers_->RemoveObserver(network_list_observer);
}

scoped_refptr<network::mojom::ThreadSafeP2PSocketManagerPtr>
P2PSocketDispatcher::GetP2PSocketManager() {
  base::AutoLock lock(p2p_socket_manager_lock_);
  if (!thread_safe_p2p_socket_manager_) {
    network::mojom::P2PSocketManagerPtr p2p_socket_manager;
    p2p_socket_manager_request_ = mojo::MakeRequest(&p2p_socket_manager);
    p2p_socket_manager.set_connection_error_handler(base::BindOnce(
        &P2PSocketDispatcher::OnConnectionError, base::Unretained(this)));
    thread_safe_p2p_socket_manager_ =
        network::mojom::ThreadSafeP2PSocketManagerPtr::Create(
            std::move(p2p_socket_manager));
  }
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&P2PSocketDispatcher::RequestInterfaceIfNecessary, this));
  return thread_safe_p2p_socket_manager_;
}

void P2PSocketDispatcher::NetworkListChanged(
    const std::vector<net::NetworkInterface>& networks,
    const net::IPAddress& default_ipv4_local_address,
    const net::IPAddress& default_ipv6_local_address) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  networks_ = networks;
  default_ipv4_local_address_ = default_ipv4_local_address;
  default_ipv6_local_address_ = default_ipv6_local_address;
  network_list_observers_->Notify(
      FROM_HERE, &NetworkListObserver::OnNetworkListChanged, networks,
      default_ipv4_local_address, default_ipv6_local_address);
}

void P2PSocketDispatcher::RequestInterfaceIfNecessary() {
  if (!p2p_socket_manager_request_.is_pending())
    return;

  ChildThreadImpl::current()->GetConnector()->BindInterface(
      mojom::kBrowserServiceName, std::move(p2p_socket_manager_request_));
}

void P2PSocketDispatcher::RequestNetworkEventsIfNecessary() {
  if (network_notification_client_binding_.is_bound()) {
    network_list_observers_->Notify(
        FROM_HERE, &NetworkListObserver::OnNetworkListChanged, networks_,
        default_ipv4_local_address_, default_ipv6_local_address_);
  } else {
    network::mojom::P2PNetworkNotificationClientPtr network_notification_client;
    network_notification_client_binding_.Bind(
        mojo::MakeRequest(&network_notification_client));
    GetP2PSocketManager()->get()->StartNetworkNotifications(
        std::move(network_notification_client));
  }
}

void P2PSocketDispatcher::OnConnectionError() {
  base::AutoLock lock(p2p_socket_manager_lock_);
  thread_safe_p2p_socket_manager_.reset();
}

}  // namespace content
