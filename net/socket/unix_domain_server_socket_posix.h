// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_UNIX_DOMAIN_SERVER_SOCKET_POSIX_H_
#define NET_SOCKET_UNIX_DOMAIN_SERVER_SOCKET_POSIX_H_

#include <stdint.h>
#include <sys/types.h>

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/socket/server_socket.h"
#include "net/socket/socket_descriptor.h"

namespace net {

class SocketPosix;

// A server socket that uses unix domain socket as the transport layer.
// Supports abstract namespaces on Linux and Android.
class NET_EXPORT UnixDomainServerSocket : public ServerSocket {
 public:
  // Credentials of a peer process connected to the socket.
  struct NET_EXPORT Credentials {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_FUCHSIA)
    // Linux and Fuchsia provide more information about the connected peer
    // than Windows/OS X. It's useful for permission-based authorization on
    // Android.
    pid_t process_id;
#endif
    uid_t user_id;
    gid_t group_id;
  };

  // Callback that returns whether the already connected client, identified by
  // its credentials, is allowed to keep the connection open. Note that
  // the socket is closed immediately in case the callback returns false.
  using AuthCallback = base::RepeatingCallback<bool(const Credentials&)>;

  UnixDomainServerSocket(const AuthCallback& auth_callack,
                         bool use_abstract_namespace);

  UnixDomainServerSocket(const UnixDomainServerSocket&) = delete;
  UnixDomainServerSocket& operator=(const UnixDomainServerSocket&) = delete;

  ~UnixDomainServerSocket() override;

  // Gets credentials of peer to check permissions.
  static bool GetPeerCredentials(SocketDescriptor socket_fd,
                                 Credentials* credentials);

  // ServerSocket implementation.
  int Listen(const IPEndPoint& address,
             int backlog,
             std::optional<bool> ipv6_only) override;
  int ListenWithAddressAndPort(const std::string& address_string,
                               uint16_t port,
                               int backlog) override;
  int GetLocalAddress(IPEndPoint* address) const override;
  int Accept(std::unique_ptr<StreamSocket>* socket,
             CompletionOnceCallback callback) override;

  // Creates a server socket, binds it to the specified |socket_path| and
  // starts listening for incoming connections with the specified |backlog|.
  int BindAndListen(const std::string& socket_path, int backlog);

  // Accepts an incoming connection on |listen_socket_|, but passes back
  // a raw SocketDescriptor instead of a StreamSocket.
  int AcceptSocketDescriptor(SocketDescriptor* socket_descriptor,
                             CompletionOnceCallback callback);

 private:
  int DoAccept();
  void AcceptCompleted(int rv);
  bool AuthenticateAndGetStreamSocket();
  void SetSocketResult(std::unique_ptr<SocketPosix> accepted_socket);
  void RunCallback(int rv);
  void CancelCallback();

  std::unique_ptr<SocketPosix> listen_socket_;
  const AuthCallback auth_callback_;
  CompletionOnceCallback callback_;
  const bool use_abstract_namespace_;

  std::unique_ptr<SocketPosix> accept_socket_;

  struct SocketDestination {
    // Non-null while a call to Accept is pending.
    raw_ptr<std::unique_ptr<StreamSocket>> stream = nullptr;

    // Non-null while a call to AcceptSocketDescriptor is pending.
    raw_ptr<SocketDescriptor> descriptor = nullptr;
  };
  SocketDestination out_socket_;
};

}  // namespace net

#endif  // NET_SOCKET_UNIX_DOMAIN_SERVER_SOCKET_POSIX_H_
