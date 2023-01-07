// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_TRANSFERABLE_SOCKET_H_
#define SERVICES_NETWORK_PUBLIC_CPP_TRANSFERABLE_SOCKET_H_

#include "base/component_export.h"
#include "base/dcheck_is_on.h"
#include "base/process/process.h"
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
  // Create a TransferableSocket that represents net::kInvalidSocket.
  TransferableSocket();

  // Create a TransferableSocket from `socket`. This object takes ownership of
  // the `socket` handle.
  // On Windows, a `destination_process` must be supplied which must match the
  // process that the TransferableSocket will be transmitted to using IPC.
#if BUILDFLAG(IS_WIN)
  TransferableSocket(net::SocketDescriptor socket,
                     const base::Process& destination_process);
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  explicit TransferableSocket(net::SocketDescriptor socket);
#else
#error "Unsupported Platform"
#endif

  TransferableSocket(TransferableSocket&& other);
  TransferableSocket& operator=(TransferableSocket&& other);
  TransferableSocket& operator=(const TransferableSocket&) = delete;
  TransferableSocket(const TransferableSocket& other) = delete;
  ~TransferableSocket();

  // Acquire the socket once it has been transferred across processes. This
  // releases the ownership of the socket to the caller.
  net::SocketDescriptor TakeSocket();

 private:
  friend struct mojo::StructTraits<network::mojom::TransferableSocketDataView,
                                   network::TransferableSocket>;
#if BUILDFLAG(IS_WIN)
  std::vector<uint8_t> wsa_info_buffer_;
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  explicit TransferableSocket(mojo::PlatformHandle socket);
  mojo::PlatformHandle socket_;
#else
#error "Unsupported platform"
#endif  // BUILDFLAG(IS_WIN)

#if DCHECK_IS_ON()
  bool has_been_transferred_ = false;
#endif  // DCHECK_IS_ON()
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_TRANSFERABLE_SOCKET_H_
