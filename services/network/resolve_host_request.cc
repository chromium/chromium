// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/resolve_host_request.h"

#include <optional>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/types/optional_util.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/log/net_log.h"
#include "net/log/net_log_with_source.h"
#include "url/url_canon.h"

namespace network {

namespace {

// Attempts URL canonicalization, but if unable, returns `host` without change.
std::string MaybeCanonicalizeHost(std::string host) {
  url::CanonHostInfo info;
  std::string canonicalized = net::CanonicalizeHost(host, &info);
  if (info.family == url::CanonHostInfo::BROKEN) {
    return host;
  } else {
    return canonicalized;
  }
}

}  // namespace

ResolveHostRequest::ResolveHostRequest(
    net::HostResolver* resolver,
    mojom::HostResolverHostPtr host,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    const std::optional<net::HostResolver::ResolveHostParameters>&
        optional_parameters,
    net::NetLog* net_log) {
  DCHECK(resolver);
  DCHECK(net_log);

  if (host->is_host_port_pair()) {
    // net::HostResolver expects canonicalized hostnames.
    net::HostPortPair host_port_pair = host->get_host_port_pair();
    host_port_pair.set_host(MaybeCanonicalizeHost(host_port_pair.host()));
    internal_request_ = resolver->CreateRequest(
        host_port_pair, network_anonymization_key,
        net::NetLogWithSource::Make(
            net_log, net::NetLogSourceType::NETWORK_SERVICE_HOST_RESOLVER),
        optional_parameters);
  } else {
    internal_request_ = resolver->CreateRequest(
        host->get_scheme_host_port(), network_anonymization_key,
        net::NetLogWithSource::Make(
            net_log, net::NetLogSourceType::NETWORK_SERVICE_HOST_RESOLVER),
        optional_parameters);
  }
}

ResolveHostRequest::~ResolveHostRequest() {
  control_handle_receiver_.reset();

  if (response_client_.is_bound()) {
    response_client_->OnComplete(
        net::ERR_NAME_NOT_RESOLVED, net::ResolveErrorInfo(net::ERR_FAILED),
        /*resolved_addresses=*/std::nullopt,
        /*endpoint_results_with_metadata=*/std::nullopt);
    response_client_.reset();
  }
}

int ResolveHostRequest::Start(
    mojo::PendingReceiver<mojom::ResolveHostHandle> control_handle_receiver,
    mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client,
    net::CompletionOnceCallback callback) {
  DCHECK(internal_request_);
  DCHECK(!control_handle_receiver_.is_bound());
  DCHECK(!response_client_.is_bound());

  // Unretained |this| reference is safe because if |internal_request_| goes out
  // of scope, it will cancel the request and ResolveHost() will not call the
  // callback.
  int rv = internal_request_->Start(
      base::BindOnce(&ResolveHostRequest::OnComplete, base::Unretained(this)));

  mojo::Remote<mojom::ResolveHostClient> response_client(
      std::move(pending_response_client));
  if (rv != net::ERR_IO_PENDING) {
    response_client->OnComplete(rv, GetResolveErrorInfo(),
                                base::OptionalFromPtr(GetAddressResults()),
                                GetEndpointResultsWithMetadata());
    return rv;
  }

  if (control_handle_receiver)
    control_handle_receiver_.Bind(std::move(control_handle_receiver));

  response_client_ = std::move(response_client);
  // Unretained |this| reference is safe because connection error cannot occur
  // if |response_client_| goes out of scope.
  response_client_.set_disconnect_handler(base::BindOnce(
      &ResolveHostRequest::Cancel, base::Unretained(this), net::ERR_FAILED));

  callback_ = std::move(callback);

  return net::ERR_IO_PENDING;
}

void ResolveHostRequest::Cancel(int error) {
  DCHECK_NE(net::OK, error);

  if (cancelled_)
    return;

  internal_request_ = nullptr;
  cancelled_ = true;
  resolve_error_info_ = net::ResolveErrorInfo(error);
  OnComplete(error);
}

void ResolveHostRequest::OnComplete(int error) {
  DCHECK(response_client_.is_bound());
  DCHECK(callback_);

  control_handle_receiver_.reset();
  SignalNonAddressResults();
  response_client_->OnComplete(error, GetResolveErrorInfo(),
                               base::OptionalFromPtr(GetAddressResults()),
                               GetEndpointResultsWithMetadata());

  response_client_.reset();
  // Invoke completion callback last as it may delete |this|.
  std::move(callback_).Run(GetResolveErrorInfo().error);
}

net::ResolveErrorInfo ResolveHostRequest::GetResolveErrorInfo() const {
  if (cancelled_) {
    return resolve_error_info_;
  }

  DCHECK(internal_request_);
  return internal_request_->GetResolveErrorInfo();
}

const net::AddressList* ResolveHostRequest::GetAddressResults() const {
  if (cancelled_) {
    return nullptr;
  }

  DCHECK(internal_request_);
  return internal_request_->GetAddressResults();
}

std::optional<net::HostResolverEndpointResults>
ResolveHostRequest::GetEndpointResultsWithMetadata() const {
  if (cancelled_) {
    return std::nullopt;
  }

  DCHECK(internal_request_);
  const net::HostResolverEndpointResults* endpoint_results =
      internal_request_->GetEndpointResults();

  if (!endpoint_results) {
    return std::nullopt;
  }

  net::HostResolverEndpointResults endpoint_results_with_metadata;

  // The last element of endpoint_results has only resolved IP
  // addresses(non-protocol endpoints), and this information is passed as
  // another parameter at least in OnComplete method, so drop that here to avoid
  // providing redundant information.
  base::ranges::copy_if(
      *endpoint_results, std::back_inserter(endpoint_results_with_metadata),
      [](const auto& result) {
        return !result.metadata.supported_protocol_alpns.empty();
      });

  return endpoint_results_with_metadata;
}

void ResolveHostRequest::SignalNonAddressResults() {
  if (cancelled_) {
    return;
  }
  DCHECK(internal_request_);

  if (internal_request_->GetTextResults() &&
      !internal_request_->GetTextResults()->empty()) {
    response_client_->OnTextResults(*internal_request_->GetTextResults());
  }

  if (internal_request_->GetHostnameResults() &&
      !internal_request_->GetHostnameResults()->empty()) {
    response_client_->OnHostnameResults(
        *internal_request_->GetHostnameResults());
  }
}

}  // namespace network
