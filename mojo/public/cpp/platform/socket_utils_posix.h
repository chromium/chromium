// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_PLATFORM_SOCKET_UTILS_POSIX_H_
#define MOJO_PUBLIC_CPP_PLATFORM_SOCKET_UTILS_POSIX_H_

#include <stddef.h>
#include <sys/types.h>

#include <vector>

#include "base/component_export.h"
#include "base/files/platform_file.h"
#include "base/files/scoped_file.h"

struct iovec;  // Declared in <sys/uio.h>

namespace mojo {

// There is an upper bound of number of handles on what is supported across
// various OS implementations of sendmsg(). This value was chosen because it
// should be safe across all supported platforms.
constexpr size_t kMaxSendmsgHandles = 128;

// NOTE: Functions declared here really don't belong in Mojo, but they exist to
// support code which used to rely on internal parts of the Mojo implementation
// and there wasn't a much better home for them. Consider moving them to
// src/base or something.

// Like |write()| but handles |EINTR| and never raises |SIGPIPE|.
COMPONENT_EXPORT(MOJO_CPP_PLATFORM)
ssize_t SocketWrite(base::PlatformFile socket,
                    const void* bytes,
                    size_t num_bytes);

// Like |writev()| but handles |EINTR| and never raises |SIGPIPE|.
COMPONENT_EXPORT(MOJO_CPP_PLATFORM)
ssize_t SocketWritev(base::PlatformFile socket,
                     struct iovec* iov,
                     size_t num_iov);

// Wrapper around |sendmsg()| which makes it convenient to send attached file
// descriptors. All entries in |descriptors| must be valid and |descriptors|
// must be non-empty.
//
// Returns the same value as |sendmsg()|, i.e. -1 on error and otherwise the
// number of bytes sent. Note that the number of bytes sent may be smaller
// than the total data in |iov|.
//
// Note that regardless of success or failure, descriptors in |descriptors| are
// not closed.
COMPONENT_EXPORT(MOJO_CPP_PLATFORM)
ssize_t SendmsgWithHandles(base::PlatformFile socket,
                           struct iovec* iov,
                           size_t num_iov,
                           const std::vector<base::ScopedFD>& descriptors);

// Like |recvmsg()|, but handles |EINTR|.
COMPONENT_EXPORT(MOJO_CPP_PLATFORM)
ssize_t SocketRecvmsg(base::PlatformFile socket,
                      void* buf,
                      size_t num_bytes,
                      std::vector<base::ScopedFD>* descriptors,
                      bool block = false);

// Treats |server_fd| as a socket listening for new connections. Returns |false|
// if it encounters an unrecoverable error.
//
// If a connection wasn't established but the server is still OK, this returns
// |true| and leaves |*connection_fd| unchanged.
//
// If a connection was accepted, this returns |true| and |*connection_fd| is
// updated with a file descriptor for the new connection.
//
// Iff |check_peer_user| is |true|, connecting clients running as a different
// user from the server (i.e. the calling process) will be rejected.
COMPONENT_EXPORT(MOJO_CPP_PLATFORM)
bool AcceptSocketConnection(base::PlatformFile server_fd,
                            base::ScopedFD* connection_fd,
                            bool check_peer_user = true);

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_PLATFORM_SOCKET_UTILS_POSIX_H_
