// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/platform/named_platform_channel.h"

#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/sockaddr_storage.h"
#include "net/base/sockaddr_util_posix.h"

namespace mojo {

namespace {

NamedPlatformChannel::ServerName GenerateRandomServerName(
    const NamedPlatformChannel::Options& options) {
  return options.socket_dir
      .AppendASCII(base::NumberToString(base::RandUint64()))
      .value();
}

// This function fills in |addr_storage| with the appropriate data for the
// socket as well as the data's length. Returns true on success, or false on
// failure (typically because |server_name| violated the naming rules). On
// Linux and Android, setting |use_abstract_namespace| to true will return a
// socket address for an abstract non-filesystem socket.
bool MakeUnixAddr(const NamedPlatformChannel::ServerName& server_name,
                  bool use_abstract_namespace,
                  net::SockaddrStorage* addr_storage) {
  DCHECK(addr_storage);
  DCHECK(!server_name.empty());

  constexpr size_t kMaxSocketNameLength = 104;

  // We reject server_name.length() == kMaxSocketNameLength to make room for the
  // NUL terminator at the end of the string. For the Linux abstract namespace,
  // the path has a leading NUL character instead (with no NUL terminator
  // required). In both cases N+1 bytes are needed to fill the server name.
  if (server_name.length() >= kMaxSocketNameLength) {
    LOG(ERROR) << "Socket name too long: " << server_name;
    return false;
  }

  return net::FillUnixAddress(server_name, use_abstract_namespace,
                              addr_storage);
}

// This function creates a unix domain socket, and set it as non-blocking.
// If successful, this returns a PlatformHandle containing the socket.
// Otherwise, this returns an invalid PlatformHandle.
PlatformHandle CreateUnixDomainSocket() {
  // Create the unix domain socket.
  PlatformHandle handle(base::ScopedFD(socket(AF_UNIX, SOCK_STREAM, 0)));
  if (!handle.is_valid()) {
    PLOG(ERROR) << "Failed to create AF_UNIX socket.";
    return PlatformHandle();
  }

  // Now set it as non-blocking.
  if (!base::SetNonBlocking(handle.GetFD().get())) {
    PLOG(ERROR) << "base::SetNonBlocking() failed " << handle.GetFD().get();
    return PlatformHandle();
  }
  return handle;
}

}  // namespace

// static
PlatformChannelServerEndpoint NamedPlatformChannel::CreateServerEndpoint(
    const Options& options,
    ServerName* server_name) {
  ServerName name = options.server_name;
  if (name.empty())
    name = GenerateRandomServerName(options);

  // Make sure the path we need exists.
  base::FilePath socket_dir = base::FilePath(name).DirName();
  if (!base::CreateDirectory(socket_dir)) {
    LOG(ERROR) << "Couldn't create directory: " << socket_dir.value();
    return PlatformChannelServerEndpoint();
  }

  // Delete any old FS instances.
  if (unlink(name.c_str()) < 0 && errno != ENOENT) {
    PLOG(ERROR) << "unlink " << name;
    return PlatformChannelServerEndpoint();
  }

  net::SockaddrStorage storage;
  if (!MakeUnixAddr(name, options.use_abstract_namespace, &storage))
    return PlatformChannelServerEndpoint();

  PlatformHandle handle = CreateUnixDomainSocket();
  if (!handle.is_valid())
    return PlatformChannelServerEndpoint();

  // Bind the socket.
  if (bind(handle.GetFD().get(), storage.addr, storage.addr_len) < 0) {
    PLOG(ERROR) << "bind " << name;
    return PlatformChannelServerEndpoint();
  }

  // Start listening on the socket.
  if (listen(handle.GetFD().get(), SOMAXCONN) < 0) {
    PLOG(ERROR) << "listen " << name;
    unlink(name.c_str());
    return PlatformChannelServerEndpoint();
  }

  *server_name = name;
  return PlatformChannelServerEndpoint(std::move(handle));
}

// static
PlatformChannelEndpoint NamedPlatformChannel::CreateClientEndpoint(
    const Options& options) {
  DCHECK(!options.server_name.empty());

  net::SockaddrStorage storage;
  if (!MakeUnixAddr(options.server_name, options.use_abstract_namespace,
                    &storage))
    return PlatformChannelEndpoint();

  PlatformHandle handle = CreateUnixDomainSocket();
  if (!handle.is_valid())
    return PlatformChannelEndpoint();

  if (HANDLE_EINTR(
          connect(handle.GetFD().get(), storage.addr, storage.addr_len)) < 0) {
    VPLOG(1) << "connect " << options.server_name;
    return PlatformChannelEndpoint();
  }
  return PlatformChannelEndpoint(std::move(handle));
}

}  // namespace mojo
