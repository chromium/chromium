// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/dns_config_change_manager.h"

#include <utility>

namespace network {

DnsConfigChangeManager::DnsConfigChangeManager() {
  net::NetworkChangeNotifier::AddDNSObserver(this);
}

DnsConfigChangeManager::~DnsConfigChangeManager() {
  net::NetworkChangeNotifier::RemoveDNSObserver(this);
}

void DnsConfigChangeManager::AddReceiver(
    mojo::PendingReceiver<mojom::DnsConfigChangeManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void DnsConfigChangeManager::RequestNotifications(
    mojo::PendingRemote<mojom::DnsConfigChangeManagerClient> client) {
  clients_.Add(std::move(client));
}

void DnsConfigChangeManager::OnDNSChanged() {
  for (const auto& client : clients_)
    client->OnDnsConfigChanged();
}

}  // namespace network
