// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SOCKADDR_STORAGE_H_
#define NET_BASE_SOCKADDR_STORAGE_H_

#include "base/memory/raw_ptr_exclusion.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <winsock2.h>
#include <ws2tcpip.h>
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include <sys/socket.h>
#include <sys/types.h>
#endif

#include "net/base/net_export.h"

namespace net {

// Convenience struct for when you need a |struct sockaddr|.
struct NET_EXPORT SockaddrStorage {
  SockaddrStorage();
  SockaddrStorage(const SockaddrStorage& other);
  void operator=(const SockaddrStorage& other);

  struct sockaddr_storage addr_storage;
  socklen_t addr_len;
  // This field is not a raw_ptr<> because of a rewriter issue not adding .get()
  // in reinterpret_cast.
  RAW_PTR_EXCLUSION struct sockaddr* const addr;
};

}  // namespace net

#endif  // NET_BASE_SOCKADDR_STORAGE_H_
