// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/sockaddr_storage.h"

#include <string.h>

#include "base/containers/span.h"

namespace net {

SockaddrStorage::SockaddrStorage() : addr_len(sizeof(addr_storage)) {}

SockaddrStorage::SockaddrStorage(const SockaddrStorage& other)
    : addr_len(other.addr_len) {
  base::byte_span_from_ref(addr_storage)
      .copy_from(base::byte_span_from_ref(other.addr_storage));
}

void SockaddrStorage::operator=(const SockaddrStorage& other) {
  addr_len = other.addr_len;
  base::byte_span_from_ref(addr_storage)
      .copy_from(base::byte_span_from_ref(other.addr_storage));
}

}  // namespace net
