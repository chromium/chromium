// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_DNS_CONFIG_CHANGE_MANAGER_H_
#define SERVICES_NETWORK_DNS_CONFIG_CHANGE_MANAGER_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "net/base/network_change_notifier.h"
#include "services/network/public/mojom/host_resolver.mojom.h"

namespace network {

class COMPONENT_EXPORT(NETWORK_SERVICE) DnsConfigChangeManager
    : public mojom::DnsConfigChangeManager,
      public net::NetworkChangeNotifier::DNSObserver {
 public:
  DnsConfigChangeManager();

  DnsConfigChangeManager(const DnsConfigChangeManager&) = delete;
  DnsConfigChangeManager& operator=(const DnsConfigChangeManager&) = delete;

  ~DnsConfigChangeManager() override;

  void AddReceiver(
      mojo::PendingReceiver<mojom::DnsConfigChangeManager> receiver);

  // mojom::DnsConfigChangeManager implementation:
  void RequestNotifications(
      mojo::PendingRemote<mojom::DnsConfigChangeManagerClient> client) override;

 private:
  // net::NetworkChangeNotifier::DNSObserver implementation:
  void OnDNSChanged() override;

  mojo::ReceiverSet<mojom::DnsConfigChangeManager> receivers_;
  mojo::RemoteSet<mojom::DnsConfigChangeManagerClient> clients_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_DNS_CONFIG_CHANGE_MANAGER_H_
