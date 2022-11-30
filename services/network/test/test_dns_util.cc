// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/test_dns_util.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/dns/public/host_resolver_results.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace network {

namespace {

// Utility class that waits until a DNS resolve completes, and returns the
// resulting net::Error code. Expects there to only be one result on success.
class DnsLookupClient : public network::mojom::ResolveHostClient {
 public:
  explicit DnsLookupClient(
      mojo::PendingReceiver<network::mojom::ResolveHostClient> receiver)
      : receiver_(this, std::move(receiver)) {
    receiver_.set_disconnect_handler(
        base::BindOnce(&DnsLookupClient::OnComplete, base::Unretained(this),
                       net::ERR_FAILED, net::ResolveErrorInfo(net::ERR_FAILED),
                       /*resolved_addresses=*/absl::nullopt,
                       /*endpoint_results_with_metadata=*/absl::nullopt));
  }
  ~DnsLookupClient() override = default;

  DnsLookupClient(const DnsLookupClient&) = delete;
  DnsLookupClient& operator=(const DnsLookupClient&) = delete;

  DnsLookupResult WaitForResult() {
    run_loop_.Run();
    return dns_lookup_result_;
  }

 private:
  // network::mojom::ResolveHostClient:
  void OnComplete(int32_t error,
                  const net::ResolveErrorInfo& resolve_error_info,
                  const absl::optional<net::AddressList>& resolved_addresses,
                  const absl::optional<net::HostResolverEndpointResults>&
                      endpoint_results_with_metadata) override {
    receiver_.reset();
    dns_lookup_result_.error = error;
    dns_lookup_result_.resolve_error_info = resolve_error_info;
    dns_lookup_result_.resolved_addresses = resolved_addresses;
    dns_lookup_result_.endpoint_results_with_metadata =
        endpoint_results_with_metadata;
    run_loop_.Quit();
  }
  void OnTextResults(const std::vector<std::string>& text_results) override {
    NOTREACHED();
  }
  void OnHostnameResults(const std::vector<net::HostPortPair>& hosts) override {
    NOTREACHED();
  }

 private:
  DnsLookupResult dns_lookup_result_;

  mojo::Receiver<network::mojom::ResolveHostClient> receiver_;
  base::RunLoop run_loop_;
};

}  // namespace

DnsLookupResult::DnsLookupResult() = default;
DnsLookupResult::DnsLookupResult(const DnsLookupResult& dns_lookup_result) =
    default;
DnsLookupResult::~DnsLookupResult() = default;

DnsLookupResult BlockingDnsLookup(
    mojom::NetworkContext* network_context,
    const net::HostPortPair& host_port_pair,
    network::mojom::ResolveHostParametersPtr params,
    const net::NetworkAnonymizationKey& network_anonymization_key) {
  mojo::PendingRemote<network::mojom::ResolveHostClient> client;
  DnsLookupClient dns_lookup_client(client.InitWithNewPipeAndPassReceiver());
  // TODO(crbug.com/1355169): Consider passing a SchemeHostPort to trigger HTTPS
  // DNS resource record query.
  network_context->ResolveHost(
      network::mojom::HostResolverHost::NewHostPortPair(host_port_pair),
      network_anonymization_key, std::move(params), std::move(client));
  return dns_lookup_client.WaitForResult();
}

}  // namespace network
