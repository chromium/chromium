// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/sockaddr_storage.h"

#include <string.h>

#include <type_traits>

#include "base/containers/span.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <winsock2.h>
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include <sys/socket.h>
#endif

namespace net {

namespace {

// Templated because otherwise the compiler attempts to instantiate both arms of
// the constexpr if, which causes compile failures in the non-matching arm.
template <typename T>
void copy_from(T& dest, const T& src) {
  // `sockaddr_storage` nominally may contain padding, depending on the
  // implementation. However, in actuality, it's a wrapper around some block of
  // memory that's holding a `sockaddr`, and we need to copy all the bytes to
  // make sure we copy the full stored type. So allow byte spanification here.
  if constexpr (std::has_unique_object_representations_v<T>) {
    base::byte_span_from_ref(dest).copy_from(base::byte_span_from_ref(src));
  } else {
    base::byte_span_from_ref(base::allow_nonunique_obj, dest)
        .copy_from(base::byte_span_from_ref(base::allow_nonunique_obj, src));
  }
}

}  // namespace

SockaddrStorage::SockaddrStorage() = default;

SockaddrStorage::SockaddrStorage(const SockaddrStorage& other)
    : addr_len(other.addr_len) {
  copy_from(addr_storage, other.addr_storage);
}

SockaddrStorage& SockaddrStorage::operator=(const SockaddrStorage& other) {
  addr_len = other.addr_len;
  copy_from(addr_storage, other.addr_storage);
  return *this;
}

}  // namespace net
