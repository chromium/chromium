// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_TRANSFERABLE_SOCKET_H_
#define SERVICES_NETWORK_PUBLIC_CPP_TRANSFERABLE_SOCKET_H_

#include "base/component_export.h"
#include "base/process/process_handle.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "net/base/address_family.h"
#include "net/socket/datagram_socket.h"
#include "net/socket/socket_descriptor.h"
#include "net/socket/tcp_socket.h"
#include "net/socket/udp_socket.h"

namespace network {
class TransferableSocket;
}
namespace network::mojom {
class TransferableSocketDataView;
}

namespace mojo {
template <>
struct StructTraits<network::mojom::TransferableSocketDataView,
                    network::TransferableSocket>;
}

namespace network {

class COMPONENT_EXPORT(NETWORK_CPP_BASE) TransferableSocket {
 public:
  TransferableSocket();
  TransferableSocket(const base::ProcessHandle& destination_process,
                     const net::SocketDescriptor& socket);
  TransferableSocket(TransferableSocket&& other);
  TransferableSocket& operator=(TransferableSocket&& other);
  TransferableSocket& operator=(const TransferableSocket&) = delete;
  TransferableSocket(const TransferableSocket& other) = delete;
  ~TransferableSocket();

  net::SocketDescriptor TakeSocket();

 private:
  friend struct mojo::StructTraits<network::mojom::TransferableSocketDataView,
                                   network::TransferableSocket>;
#if BUILDFLAG(IS_WIN)
  explicit TransferableSocket(const std::vector<uint8_t>& wsa_buffer);

  base::ProcessHandle destination_process_handle_;
  std::vector<uint8_t> wsa_info_buffer_;
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  explicit TransferableSocket(mojo::PlatformHandle socket);
  mojo::PlatformHandle socket_;
#else
#error "Unsupported platform"
#endif  // BUILDFLAG(IS_WIN)
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_TRANSFERABLE_SOCKET_H_
