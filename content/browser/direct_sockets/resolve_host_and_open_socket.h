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
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/blink/public/mojom/direct_sockets/direct_sockets.mojom.h"

namespace content {

class DirectSocketsServiceImpl;

// Resolves the host/port pair provided in options and opens the requested
// socket on success. For calls to https port (443) an additional cors check is
// performed to ensure compliance. This class is self-owned: inheritors must
// call "delete this" at some point in OpenSocket(...).
class CONTENT_EXPORT ResolveHostAndOpenSocket
    : public network::ResolveHostClientBase {
 public:
  ResolveHostAndOpenSocket(base::WeakPtr<DirectSocketsServiceImpl> service,
                           blink::mojom::DirectSocketOptionsPtr options);

  ~ResolveHostAndOpenSocket() override;

  void Start();

  static int GetHttpsPort();
  static void SetHttpsPortForTesting(absl::optional<int> port);

 protected:
  void OnComplete(int result,
                  const net::ResolveErrorInfo& resolve_error_info,
                  const absl::optional<net::AddressList>& resolved_addresses,
                  const absl::optional<net::HostResolverEndpointResults>&
                      endpoint_results_with_metadata) override;

  void PerformCORSCheck(const std::string& address,
                        net::AddressList resolved_addresses);

  void OnCORSCheckComplete(std::unique_ptr<network::SimpleURLLoader> loader,
                           net::AddressList resolved_addresses,
                           scoped_refptr<net::HttpResponseHeaders>);

  virtual void OpenSocket(
      int result,
      const absl::optional<net::AddressList>& resolved_addresses) = 0;

  base::WeakPtr<DirectSocketsServiceImpl> service_;
  blink::mojom::DirectSocketOptionsPtr options_;

  bool is_mdns_name_ = false;
  bool is_raw_address_ = false;

  mojo::Receiver<network::mojom::ResolveHostClient> receiver_{this};
  mojo::Remote<network::mojom::HostResolver> resolver_;
};

using OpenTcpSocketCallback =
    blink::mojom::DirectSocketsService::OpenTcpSocketCallback;

// Overrides OpenSocket(...) to open a TCP connection.
class ResolveHostAndOpenTCPSocket final : public ResolveHostAndOpenSocket {
 public:
  ResolveHostAndOpenTCPSocket(
      base::WeakPtr<DirectSocketsServiceImpl> service,
      blink::mojom::DirectSocketOptionsPtr options,
      mojo::PendingReceiver<network::mojom::TCPConnectedSocket> receiver,
      mojo::PendingRemote<network::mojom::SocketObserver> observer,
      OpenTcpSocketCallback callback);

  ~ResolveHostAndOpenTCPSocket() override;

 private:
  void OpenSocket(
      int result,
      const absl::optional<net::AddressList>& resolved_addresses) override;

  mojo::PendingReceiver<network::mojom::TCPConnectedSocket> receiver_;
  mojo::PendingRemote<network::mojom::SocketObserver> observer_;
  OpenTcpSocketCallback callback_;
};

using OpenUdpSocketCallback =
    blink::mojom::DirectSocketsService::OpenUdpSocketCallback;

// Overrides OpenSocket(...) to open a UDP connection.
class ResolveHostAndOpenUDPSocket final : public ResolveHostAndOpenSocket {
 public:
  ResolveHostAndOpenUDPSocket(
      base::WeakPtr<DirectSocketsServiceImpl> service,
      blink::mojom::DirectSocketOptionsPtr options,
      mojo::PendingReceiver<blink::mojom::DirectUDPSocket> receiver,
      mojo::PendingRemote<network::mojom::UDPSocketListener> listener,
      OpenUdpSocketCallback callback);

  ~ResolveHostAndOpenUDPSocket() override;

 private:
  void OpenSocket(
      int result,
      const absl::optional<net::AddressList>& resolved_addresses) override;

  void OnUdpConnectCompleted(net::IPEndPoint peer_addr,
                             int result,
                             const absl::optional<net::IPEndPoint>& local_addr);

  mojo::PendingReceiver<blink::mojom::DirectUDPSocket> receiver_;
  mojo::PendingRemote<network::mojom::UDPSocketListener> listener_;
  OpenUdpSocketCallback callback_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DIRECT_SOCKETS_RESOLVE_HOST_AND_OPEN_SOCKET_H_