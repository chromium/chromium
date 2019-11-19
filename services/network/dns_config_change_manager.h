// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_DNS_CONFIG_CHANGE_MANAGER_H_
#define SERVICES_NETWORK_DNS_CONFIG_CHANGE_MANAGER_H_

#include <memory>
#include <set>

#include "base/component_export.h"
#include "base/macros.h"
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

  DISALLOW_COPY_AND_ASSIGN(DnsConfigChangeManager);
};

}  // namespace network

#endif  // SERVICES_NETWORK_DNS_CONFIG_CHANGE_MANAGER_H_
