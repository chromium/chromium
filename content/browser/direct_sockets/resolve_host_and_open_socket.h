// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DIRECT_SOCKETS_RESOLVE_HOST_AND_OPEN_SOCKET_H_
#define CONTENT_BROWSER_DIRECT_SOCKETS_RESOLVE_HOST_AND_OPEN_SOCKET_H_

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/address_list.h"
#include "net/dns/public/host_resolver_results.h"
#include "services/network/public/cpp/resolve_host_client_base.h"

namespace content {

class DirectSocketsServiceImpl;

// Resolves the host/port pair provided in options. Upon completion invokes the
// supplied callback and deletes |this|.
class CONTENT_EXPORT ResolveHostAndOpenSocket
    : public network::ResolveHostClientBase {
 public:
  using OpenSocketCallback =
      base::OnceCallback<void(int result,
                              const absl::optional<net::AddressList>&)>;

  ~ResolveHostAndOpenSocket() override;

  static ResolveHostAndOpenSocket* Create(
      base::WeakPtr<DirectSocketsServiceImpl>,
      const std::string& host,
      uint16_t port,
      OpenSocketCallback);

  void Start();

 private:
  ResolveHostAndOpenSocket(base::WeakPtr<DirectSocketsServiceImpl>,
                           const std::string& host,
                           uint16_t port,
                           OpenSocketCallback);

  void OnComplete(int result,
                  const net::ResolveErrorInfo& resolve_error_info,
                  const absl::optional<net::AddressList>& resolved_addresses,
                  const absl::optional<net::HostResolverEndpointResults>&
                      endpoint_results_with_metadata) override;

  void OpenSocket(int result,
                  const absl::optional<net::AddressList>& resolved_addresses);

  base::WeakPtr<DirectSocketsServiceImpl> service_;

  const std::string host_;
  uint16_t port_;

  OpenSocketCallback callback_;

  bool is_mdns_name_ = false;

  mojo::Receiver<network::mojom::ResolveHostClient> receiver_{this};
  mojo::Remote<network::mojom::HostResolver> resolver_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DIRECT_SOCKETS_RESOLVE_HOST_AND_OPEN_SOCKET_H_