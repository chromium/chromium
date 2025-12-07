// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/sockaddr_util_posix.h"

#include <stddef.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <string_view>

#include "base/containers/span.h"
#include "build/build_config.h"
#include "net/base/sockaddr_storage.h"

namespace net {

bool FillUnixAddress(std::string_view socket_path,
                     bool use_abstract_namespace,
                     SockaddrStorage* address) {
  // Caller should provide a non-empty path for the socket address.
  if (socket_path.empty()) {
    return false;
  }

  struct sockaddr_un* socket_addr =
      reinterpret_cast<struct sockaddr_un*>(&address->addr_storage);
  // Location to write the path.
  base::span<char> path_dest = base::span(socket_addr->sun_path);

  // The length of the path, including the nul.
  const size_t path_size = socket_path.size() + 1;

  // Non abstract namespace pathname should be null-terminated. Abstract
  // namespace pathname must start with '\0'. So, the size is always greater
  // than socket_path size by 1.
  if (path_size > path_dest.size()) {
    return false;
  }

  // Zero out the entire address struct.
  address->addr_storage = {};

  socket_addr->sun_family = AF_UNIX;
  address->addr_len = path_size + offsetof(struct sockaddr_un, sun_path);
  if (!use_abstract_namespace) {
    // Copy the path, except the terminating terminating '\0'. `path_dest` was
    // already filled with zeroes.
    path_dest.copy_prefix_from(socket_path);
    return true;
  }

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Convert the path given into abstract socket name. It must start with
  // the '\0' character, skip over it, as it should already be zero. `addr_len`
  // must specify the length of the structure exactly, as potentially the socket
  // name may have '\0' characters embedded (although we don't support this).
  path_dest.subspan(1u).copy_prefix_from(socket_path);
  return true;
#else
  return false;
#endif
}

}  // namespace net
