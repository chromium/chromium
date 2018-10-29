// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/host_resolver.h"

#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/optional.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_source.h"
#include "net/log/net_log.h"
#include "services/network/resolve_host_request.h"

namespace network {
namespace {
static base::LazyInstance<HostResolver::ResolveHostCallback>::Leaky
    resolve_host_callback;
}

namespace {
base::Optional<net::HostResolver::ResolveHostParameters>
ConvertOptionalParameters(
    const mojom::ResolveHostParametersPtr& mojo_parameters) {
  if (!mojo_parameters)
    return base::nullopt;

  net::HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = mojo_parameters->dns_query_type;
  parameters.initial_priority = mojo_parameters->initial_priority;
  parameters.source = mojo_parameters->source;
  parameters.allow_cached_response = mojo_parameters->allow_cached_response;
  parameters.include_canonical_name = mojo_parameters->include_canonical_name;
  parameters.loopback_only = mojo_parameters->loopback_only;
  parameters.is_speculative = mojo_parameters->is_speculative;
  return parameters;
}
}  // namespace

HostResolver::HostResolver(
    mojom::HostResolverRequest resolver_request,
    ConnectionShutdownCallback connection_shutdown_callback,
    net::HostResolver* internal_resolver,
    net::NetLog* net_log)
    : binding_(this, std::move(resolver_request)),
      connection_shutdown_callback_(std::move(connection_shutdown_callback)),
      internal_resolver_(internal_resolver),
      net_log_(net_log) {
  binding_.set_connection_error_handler(
      base::BindOnce(&HostResolver::OnConnectionError, base::Unretained(this)));
}

HostResolver::HostResolver(net::HostResolver* internal_resolver,
                           net::NetLog* net_log)
    : binding_(this),
      internal_resolver_(internal_resolver),
      net_log_(net_log) {}

HostResolver::~HostResolver() {
  if (binding_)
    binding_.Close();
}

void HostResolver::ResolveHost(
    const net::HostPortPair& host,
    mojom::ResolveHostParametersPtr optional_parameters,
    mojom::ResolveHostClientPtr response_client) {
#if !BUILDFLAG(ENABLE_MDNS)
  // TODO(crbug.com/821021): Handle without crashing if we create restricted
  // HostResolvers for passing to untrusted processes.
  DCHECK(!optional_parameters ||
         optional_parameters->source != net::HostResolverSource::MULTICAST_DNS);
#endif  // !BUILDFLAG(ENABLE_MDNS)

  if (resolve_host_callback.Get())
    resolve_host_callback.Get().Run(host.host());

  auto request = std::make_unique<ResolveHostRequest>(
      internal_resolver_, host, ConvertOptionalParameters(optional_parameters),
      net_log_);

  mojom::ResolveHostHandleRequest control_handle_request;
  if (optional_parameters)
    control_handle_request = std::move(optional_parameters->control_handle);

  int rv = request->Start(
      std::move(control_handle_request), std::move(response_client),
      base::BindOnce(&HostResolver::OnResolveHostComplete,
                     base::Unretained(this), request.get()));
  if (rv != net::ERR_IO_PENDING)
    return;

  // Store the request with the resolver so it can be cancelled on resolver
  // shutdown.
  bool insertion_result = requests_.emplace(std::move(request)).second;
  DCHECK(insertion_result);
}

size_t HostResolver::GetNumOutstandingRequestsForTesting() const {
  return requests_.size();
}

void HostResolver::SetResolveHostCallbackForTesting(
    ResolveHostCallback callback) {
  resolve_host_callback.Get() = std::move(callback);
}

void HostResolver::OnResolveHostComplete(ResolveHostRequest* request,
                                         int error) {
  DCHECK_NE(net::ERR_IO_PENDING, error);

  auto found_request = requests_.find(request);
  DCHECK(found_request != requests_.end());
  requests_.erase(found_request);
}

void HostResolver::OnConnectionError() {
  DCHECK(connection_shutdown_callback_);

  requests_.clear();

  // Invoke last as callback may delete |this|.
  std::move(connection_shutdown_callback_).Run(this);
}

}  // namespace network
