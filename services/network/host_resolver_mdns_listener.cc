// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/host_resolver_mdns_listener.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "net/base/host_port_pair.h"

namespace network {

HostResolverMdnsListener::HostResolverMdnsListener(
    net::HostResolver* resolver,
    const net::HostPortPair& host,
    net::DnsQueryType query_type) {
  DCHECK(resolver);

  internal_listener_ = resolver->CreateMdnsListener(host, query_type);
}

HostResolverMdnsListener::~HostResolverMdnsListener() {
  internal_listener_ = nullptr;
  response_client_.reset();
}

int HostResolverMdnsListener::Start(
    mojo::PendingRemote<mojom::MdnsListenClient> response_client,
    base::OnceClosure cancellation_callback) {
  DCHECK(internal_listener_);
  DCHECK(!response_client_.is_bound());

  int rv = internal_listener_->Start(this);
  if (rv != net::OK)
    return rv;

  response_client_.Bind(std::move(response_client));
  // Unretained |this| reference is safe because connection error cannot occur
  // if |response_client_| goes out of scope.
  response_client_.set_disconnect_handler(base::BindOnce(
      &HostResolverMdnsListener::OnConnectionError, base::Unretained(this)));

  cancellation_callback_ = std::move(cancellation_callback);

  return net::OK;
}

void HostResolverMdnsListener::OnAddressResult(
    net::HostResolver::MdnsListener::Delegate::UpdateType update_type,
    net::DnsQueryType query_type,
    net::IPEndPoint address) {
  DCHECK(response_client_.is_bound());
  response_client_->OnAddressResult(update_type, query_type, address);
}

void HostResolverMdnsListener::OnTextResult(
    net::HostResolver::MdnsListener::Delegate::UpdateType update_type,
    net::DnsQueryType query_type,
    std::vector<std::string> text_records) {
  DCHECK(response_client_.is_bound());
  response_client_->OnTextResult(update_type, query_type, text_records);
}

void HostResolverMdnsListener::OnHostnameResult(
    net::HostResolver::MdnsListener::Delegate::UpdateType update_type,
    net::DnsQueryType query_type,
    net::HostPortPair host) {
  DCHECK(response_client_.is_bound());
  response_client_->OnHostnameResult(update_type, query_type, host);
}

void HostResolverMdnsListener::OnUnhandledResult(
    net::HostResolver::MdnsListener::Delegate::UpdateType update_type,
    net::DnsQueryType query_type) {
  DCHECK(response_client_.is_bound());
  response_client_->OnUnhandledResult(update_type, query_type);
}

void HostResolverMdnsListener::OnConnectionError() {
  DCHECK(cancellation_callback_);

  internal_listener_ = nullptr;

  // Invoke cancellation callback last as it may delete |this|.
  std::move(cancellation_callback_).Run();
}

}  // namespace network
