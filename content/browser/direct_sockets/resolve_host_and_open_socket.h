// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DIRECT_SOCKETS_RESOLVE_HOST_AND_OPEN_SOCKET_H_
#define CONTENT_BROWSER_DIRECT_SOCKETS_RESOLVE_HOST_AND_OPEN_SOCKET_H_

#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/address_list.h"
#include "net/dns/public/host_resolver_results.h"
#include "services/network/public/cpp/resolve_host_client_base.h"

namespace network::mojom {
class NetworkContext;
}  // namespace network::mojom

namespace content {

// Resolves the host/port pair provided on creation. After resolver signals
// completion via OnComplete(), fires the supplied callback and deletes itself.
class CONTENT_EXPORT ResolveHostAndOpenSocket
    : public network::ResolveHostClientBase {
 public:
  using OpenSocketCallback =
      base::OnceCallback<void(int result,
                              const absl::optional<net::AddressList>&)>;

  ~ResolveHostAndOpenSocket() override;

  static ResolveHostAndOpenSocket* Create(const std::string& host,
                                          uint16_t port,
                                          OpenSocketCallback);

  void Start(network::mojom::NetworkContext*);

 private:
  ResolveHostAndOpenSocket(const std::string& host,
                           uint16_t port,
                           OpenSocketCallback);

  void OnComplete(int result,
                  const net::ResolveErrorInfo& resolve_error_info,
                  const absl::optional<net::AddressList>& resolved_addresses,
                  const absl::optional<net::HostResolverEndpointResults>&
                      endpoint_results_with_metadata) override;

  const std::string host_;
  uint16_t port_;

  OpenSocketCallback callback_;

  mojo::Receiver<network::mojom::ResolveHostClient> receiver_{this};
  mojo::Remote<network::mojom::HostResolver> resolver_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DIRECT_SOCKETS_RESOLVE_HOST_AND_OPEN_SOCKET_H_