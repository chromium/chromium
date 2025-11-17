// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_DELEGATION_DNS_REQUEST_H_
#define CONTENT_BROWSER_WEBID_DELEGATION_DNS_REQUEST_H_

#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/network_context_getter.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "url/origin.h"

namespace content::webid {

// Performs a DNS TXT query for a given origin.
class CONTENT_EXPORT DnsRequest : public network::mojom::ResolveHostClient {
 public:
  using DnsRequestCallback = base::OnceCallback<void(
      const std::optional<std::vector<std::string>>& text_records)>;

  explicit DnsRequest(network::NetworkContextGetter network_context_getter);
  ~DnsRequest() override;

  // Sends a DNS TXT request for the given hostname.
  virtual void SendRequest(const std::string& hostname,
                           DnsRequestCallback callback);

 private:
  // network::mojom::ResolveHostClient:
  void OnComplete(int result,
                  const net::ResolveErrorInfo& resolve_error_info,
                  const net::AddressList& resolved_addresses,
                  const std::vector<net::HostResolverEndpointResult>&
                      alternative_endpoints) override;
  void OnTextResults(const std::vector<std::string>& text_results) override;
  void OnHostnameResults(const std::vector<net::HostPortPair>& hosts) override;

  network::NetworkContextGetter network_context_getter_;
  mojo::Remote<network::mojom::HostResolver> host_resolver_;
  mojo::Receiver<network::mojom::ResolveHostClient> receiver_{this};
  DnsRequestCallback callback_;
};

}  // namespace content::webid

#endif  // CONTENT_BROWSER_WEBID_DELEGATION_DNS_REQUEST_H_
