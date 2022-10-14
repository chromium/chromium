// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/direct_sockets/direct_udp_socket_impl.h"

#include "content/browser/direct_sockets/direct_sockets_service_impl.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace content {

DirectUDPSocketImpl::DirectUDPSocketImpl(
    network::mojom::NetworkContext* network_context,
    mojo::PendingRemote<network::mojom::UDPSocketListener> listener) {
  network_context->CreateUDPSocket(remote_.BindNewPipeAndPassReceiver(),
                                   std::move(listener));

  remote_.set_disconnect_handler(base::BindOnce(
      &DirectUDPSocketImpl::OnDisconnect, base::Unretained(this)));
}

DirectUDPSocketImpl::~DirectUDPSocketImpl() = default;

void DirectUDPSocketImpl::Connect(const net::IPEndPoint& remote_addr,
                                  network::mojom::UDPSocketOptionsPtr options,
                                  ConnectCallback callback) {
  DCHECK(remote_.is_bound());
  remote_->Connect(remote_addr, std::move(options), std::move(callback));
}

void DirectUDPSocketImpl::ReceiveMore(uint32_t num_additional_datagrams) {
  if (!remote_.is_bound())
    return;
  remote_->ReceiveMore(num_additional_datagrams);
}

void DirectUDPSocketImpl::Send(base::span<const uint8_t> data,
                               SendCallback callback) {
  if (!remote_.is_bound()) {
    std::move(callback).Run(net::ERR_FAILED);
    return;
  }
  remote_->Send(std::move(data),
                net::MutableNetworkTrafficAnnotationTag{
                    DirectSocketsServiceImpl::TrafficAnnotation()},
                std::move(callback));
}

void DirectUDPSocketImpl::Close() {
  if (!remote_.is_bound()) {
    return;
  }
  remote_->Close();
  remote_.reset();
}

void DirectUDPSocketImpl::OnDisconnect() {
  remote_.reset();
}

}  // namespace content
