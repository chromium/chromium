// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_HOST_RESOLVER_MDNS_LISTENER_H_
#define SERVICES_NETWORK_HOST_RESOLVER_MDNS_LISTENER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/dns_query_type.h"
#include "services/network/public/mojom/host_resolver.mojom.h"

namespace net {
class HostPortPair;
}  // namespace net

namespace network {

class HostResolverMdnsListener
    : public net::HostResolver::MdnsListener::Delegate {
 public:
  HostResolverMdnsListener(net::HostResolver* resolver,
                           const net::HostPortPair& host,
                           net::DnsQueryType query_type);
  ~HostResolverMdnsListener() override;

  int Start(mojo::PendingRemote<mojom::MdnsListenClient> response_client,
            base::OnceClosure cancellation_callback);

  // net::HostResolver::MdnsListenerDelegate implementation
  void OnAddressResult(
      net::HostResolver::MdnsListener::Delegate::UpdateType update_type,
      net::DnsQueryType query_type,
      net::IPEndPoint address) override;
  void OnTextResult(
      net::HostResolver::MdnsListener::Delegate::UpdateType update_type,
      net::DnsQueryType query_type,
      std::vector<std::string> text_records) override;
  void OnHostnameResult(
      net::HostResolver::MdnsListener::Delegate::UpdateType update_type,
      net::DnsQueryType query_type,
      net::HostPortPair host) override;
  void OnUnhandledResult(
      net::HostResolver::MdnsListener::Delegate::UpdateType update_type,
      net::DnsQueryType query_type) override;

 private:
  void OnConnectionError();

  std::unique_ptr<net::HostResolver::MdnsListener> internal_listener_;
  mojo::Remote<mojom::MdnsListenClient> response_client_;

  base::OnceClosure cancellation_callback_;

  DISALLOW_COPY_AND_ASSIGN(HostResolverMdnsListener);
};

}  // namespace network

#endif  // SERVICES_NETWORK_HOST_RESOLVER_MDNS_LISTENER_H_
