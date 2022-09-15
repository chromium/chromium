// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/transferable_socket.h"

#if BUILDFLAG(IS_WIN)
#include <winsock2.h>
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include "base/files/scoped_file.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#else
#error "unsupported platform"
#endif

#include "base/dcheck_is_on.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/process/process_handle.h"
#include "net/socket/tcp_socket.h"
#include "net/socket/udp_socket.h"

namespace network {

TransferableSocket::TransferableSocket() = default;

#if BUILDFLAG(IS_WIN)
TransferableSocket::TransferableSocket(net::SocketDescriptor socket,
                                       const base::Process& destination_process)
    : wsa_info_buffer_(sizeof(WSAPROTOCOL_INFOW), 0) {
  DCHECK(destination_process.IsValid());

  if (socket == net::kInvalidSocket) {
    // Send across an empty buffer if invalid socket.
    wsa_info_buffer_.clear();
    return;
  }

  // If the duplicate fails, the network service process has likely died
  // asynchronously, or the caller does not have the ability to duplicate a
  // socket to that process.
  if (::WSADuplicateSocketW(socket, destination_process.Pid(),
                            reinterpret_cast<LPWSAPROTOCOL_INFOW>(
                                wsa_info_buffer_.data())) != 0) {
    wsa_info_buffer_.clear();
  }
  // The source handle must always be closed otherwise it will leak. In the
  // event of a real failure (e.g. network process died), the network stack will
  // retry.
  ::closesocket(socket);
}
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
TransferableSocket::TransferableSocket(net::SocketDescriptor socket)
    : socket_(base::ScopedFD(socket)) {}
#else
#error "Unsupported Platform"
#endif  // BUILDFLAG(IS_WIN)

TransferableSocket::~TransferableSocket() = default;
TransferableSocket& TransferableSocket::operator=(TransferableSocket&& other) =
    default;
TransferableSocket::TransferableSocket(TransferableSocket&& other) = default;

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
TransferableSocket::TransferableSocket(mojo::PlatformHandle socket)
    : socket_(std::move(socket)) {}
#elif !BUILDFLAG(IS_WIN)
#error "Unsupported Platform"
#endif

net::SocketDescriptor TransferableSocket::TakeSocket() {
#if DCHECK_IS_ON()
  DCHECK(has_been_transferred_)
      << "Cannot take a socket before transferring across processes.";
#endif
#if BUILDFLAG(IS_WIN)
  LPWSAPROTOCOL_INFOW protocol_info;
  if (wsa_info_buffer_.empty())
    return net::kInvalidSocket;
  protocol_info = reinterpret_cast<LPWSAPROTOCOL_INFOW>(
      const_cast<uint8_t*>(wsa_info_buffer_.data()));
  // These fields are documented here:
  // https://docs.microsoft.com/en-us/windows/win32/api/winsock2/ns-winsock2-wsaprotocol_infoa.
  SOCKET s =
      ::WSASocketW(FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO,
                   protocol_info, 0, WSA_FLAG_OVERLAPPED);
  return s;
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  return socket_.ReleaseFD();
#else
#error "Unsupported platform"
#endif  // BUILDFLAG(IS_WIN)
}

}  // namespace network
