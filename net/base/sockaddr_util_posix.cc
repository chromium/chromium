// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/base/sockaddr_util_posix.h"

#include <stddef.h>
#include <string.h>
#include <stddef.h>

#include <sys/socket.h>
#include <sys/un.h>

#include "build/build_config.h"
#include "net/base/sockaddr_storage.h"

namespace net {

bool FillUnixAddress(const std::string& socket_path,
                     bool use_abstract_namespace,
                     SockaddrStorage* address) {
  // Caller should provide a non-empty path for the socket address.
  if (socket_path.empty())
    return false;

  size_t path_max = address->addr_len - offsetof(struct sockaddr_un, sun_path);
  // Non abstract namespace pathname should be null-terminated. Abstract
  // namespace pathname must start with '\0'. So, the size is always greater
  // than socket_path size by 1.
  size_t path_size = socket_path.size() + 1;
  if (path_size > path_max)
    return false;

  struct sockaddr_un* socket_addr =
      reinterpret_cast<struct sockaddr_un*>(address->addr);
  memset(socket_addr, 0, address->addr_len);
  socket_addr->sun_family = AF_UNIX;
  address->addr_len = path_size + offsetof(struct sockaddr_un, sun_path);
  if (!use_abstract_namespace) {
    memcpy(socket_addr->sun_path, socket_path.c_str(), socket_path.size());
    return true;
  }

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Convert the path given into abstract socket name. It must start with
  // the '\0' character, so we are adding it. |addr_len| must specify the
  // length of the structure exactly, as potentially the socket name may
  // have '\0' characters embedded (although we don't support this).
  // Note that addr.sun_path is already zero initialized.
  memcpy(socket_addr->sun_path + 1, socket_path.c_str(), socket_path.size());
  return true;
#else
  return false;
#endif
}

}  // namespace net
