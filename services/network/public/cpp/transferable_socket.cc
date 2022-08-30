// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/transferable_socket.h"

#if BUILDFLAG(IS_WIN)
#include <winsock2.h>
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include "mojo/public/cpp/platform/platform_handle.h"
#else
#error "unsupported platform"
#endif

#include "base/logging.h"
#include "base/notreached.h"
#include "base/process/process_handle.h"
#include "net/socket/tcp_socket.h"
#include "net/socket/udp_socket.h"

namespace network {

TransferableSocket::TransferableSocket() = default;

#if BUILDFLAG(IS_WIN)
TransferableSocket::TransferableSocket(
    const base::ProcessHandle& destination_process_handle,
    const net::SocketDescriptor& socket)
    : destination_process_handle_(destination_process_handle),
      wsa_info_buffer_(sizeof(WSAPROTOCOL_INFOW), 0) {
  DCHECK_NE(destination_process_handle_, base::kNullProcessHandle);

  ::WSADuplicateSocketW(
      socket, ::GetProcessId(destination_process_handle_),
      reinterpret_cast<LPWSAPROTOCOL_INFOW>(wsa_info_buffer_.data()));
}
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
TransferableSocket::TransferableSocket(
    const base::ProcessHandle& destination_process_handle,
    const net::SocketDescriptor& socket)
    : socket_(base::ScopedFD(socket)) {}
#else
#error "Unsupported Platform"
#endif  // BUILDFLAG(IS_WIN)

TransferableSocket::~TransferableSocket() = default;
TransferableSocket& TransferableSocket::operator=(TransferableSocket&& other) =
    default;
TransferableSocket::TransferableSocket(TransferableSocket&& other) = default;

#if BUILDFLAG(IS_WIN)
TransferableSocket::TransferableSocket(const std::vector<uint8_t>& wsa_buffer) {
  wsa_info_buffer_ = wsa_buffer;
}
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
TransferableSocket::TransferableSocket(mojo::PlatformHandle socket)
    : socket_(std::move(socket)) {}
#else
#error "Unsupported Platform"
#endif  // BUILDFLAG(IS_WIN)

net::SocketDescriptor TransferableSocket::TakeSocket() {
#if BUILDFLAG(IS_WIN)
  LPWSAPROTOCOL_INFOW protocol_info;
  if (wsa_info_buffer_.empty())
    return net::SocketDescriptor();
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
