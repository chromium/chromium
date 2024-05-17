// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/nqe/network_quality_estimator_util.h"

#include <memory>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "net/base/address_list.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/host_resolver_source.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "url/scheme_host_port.h"

namespace net::nqe {

namespace {

bool IsPrivateHost(HostResolver* host_resolver,
                   url::SchemeHostPort scheme_host_port,
                   const NetworkAnonymizationKey& network_anonymization_key,
                   NetLogWithSource net_log) {
  // Try resolving |host_port_pair.host()| synchronously.
  HostResolver::ResolveHostParameters parameters;
  parameters.source = HostResolverSource::LOCAL_ONLY;
  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      host_resolver->CreateRequest(std::move(scheme_host_port),
                                   network_anonymization_key, net_log,
                                   parameters);

  int rv = request->Start(
      base::BindOnce([](int error) { NOTREACHED_IN_MIGRATION(); }));
  DCHECK_NE(rv, ERR_IO_PENDING);

  if (rv == OK && request->GetAddressResults() &&
      !request->GetAddressResults()->empty()) {
    // Checking only the first address should be sufficient.
    IPEndPoint ip_endpoint = request->GetAddressResults()->front();
    IPAddress ip_address = ip_endpoint.address();
    if (!ip_address.IsPubliclyRoutable())
      return true;
  }

  return false;
}

}  // namespace

namespace internal {

bool IsRequestForPrivateHost(const URLRequest& request,
                             NetLogWithSource net_log) {
  // Using the request's NetworkAnonymizationKey isn't necessary for privacy
  // reasons, but is needed to maximize the chances of a cache hit.
  return IsPrivateHost(
      request.context()->host_resolver(), url::SchemeHostPort(request.url()),
      request.isolation_info().network_anonymization_key(), net_log);
}

bool IsPrivateHostForTesting(
    HostResolver* host_resolver,
    url::SchemeHostPort scheme_host_port,
    const NetworkAnonymizationKey& network_anonymization_key) {
  return IsPrivateHost(host_resolver, std::move(scheme_host_port),
                       network_anonymization_key, NetLogWithSource());
}

}  // namespace internal

}  // namespace net::nqe
