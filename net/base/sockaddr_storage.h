// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SOCKADDR_STORAGE_H_
#define NET_BASE_SOCKADDR_STORAGE_H_

#include "base/memory/raw_ptr_exclusion.h"
#include "build/build_config.h"
#include "net/base/net_export.h"

#if BUILDFLAG(IS_WIN)
#include <winsock2.h>
#include <ws2tcpip.h>
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include <sys/socket.h>
#include <sys/types.h>
#endif

namespace net {

// Convenience struct for when you need a |struct sockaddr|.
struct NET_EXPORT SockaddrStorage {
  SockaddrStorage();
  SockaddrStorage(const SockaddrStorage& other);
  SockaddrStorage& operator=(const SockaddrStorage& other);

  const sockaddr* addr() const {
    return reinterpret_cast<const sockaddr*>(&addr_storage);
  }
  sockaddr* addr() { return reinterpret_cast<sockaddr*>(&addr_storage); }

  sockaddr_storage addr_storage;
  socklen_t addr_len = sizeof(addr_storage);
};

}  // namespace net

#endif  // NET_BASE_SOCKADDR_STORAGE_H_
