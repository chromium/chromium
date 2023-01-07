// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/sockaddr_storage.h"

#include <string.h>

namespace net {

SockaddrStorage::SockaddrStorage()
    : addr_len(sizeof(addr_storage)),
      addr(reinterpret_cast<struct sockaddr*>(&addr_storage)) {}

SockaddrStorage::SockaddrStorage(const SockaddrStorage& other)
    : addr_len(other.addr_len),
      addr(reinterpret_cast<struct sockaddr*>(&addr_storage)) {
  memcpy(addr, other.addr, addr_len);
}

void SockaddrStorage::operator=(const SockaddrStorage& other) {
  addr_len = other.addr_len;
  // addr is already set to &this->addr_storage by default ctor.
  memcpy(addr, other.addr, addr_len);
}

}  // namespace net
