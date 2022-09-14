// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_UDP_SOCKET_GLOBAL_LIMITS_H_
#define NET_SOCKET_UDP_SOCKET_GLOBAL_LIMITS_H_

#include "net/base/net_errors.h"
#include "net/base/net_export.h"

namespace net {

// Helper class for RAII-style management of the global count of "open UDP
// sockets" [1] in the process.
//
// Keeping OwnedUDPSocketCount alive increases the global socket counter by 1.
// When it goes out of scope - or is explicitly Reset() - the reference is
// returned to the global counter.
class NET_EXPORT OwnedUDPSocketCount {
 public:
  // The default constructor builds an empty OwnedUDPSocketCount (does not own a
  // count).
  OwnedUDPSocketCount();

  // Any count held by OwnedUDPSocketCount is transferred when moving.
  OwnedUDPSocketCount(OwnedUDPSocketCount&&);
  OwnedUDPSocketCount& operator=(OwnedUDPSocketCount&&);

  // This is a move-only type.
  OwnedUDPSocketCount(const OwnedUDPSocketCount&) = delete;
  OwnedUDPSocketCount& operator=(const OwnedUDPSocketCount&) = delete;

  ~OwnedUDPSocketCount();

  // Returns false if this instance "owns" a socket count. In
  // other words, when |empty()|, destruction of |this| will
  // not change the global socket count.
  bool empty() const { return empty_; }

  // Resets |this| to an empty state (|empty()| becomes true after
  // calling this). If |this| was previously |!empty()|, the global
  // socket count will be decremented.
  void Reset();

 private:
  // Only TryAcquireGlobalUDPSocketCount() is allowed to construct a non-empty
  // OwnedUDPSocketCount.
  friend NET_EXPORT OwnedUDPSocketCount TryAcquireGlobalUDPSocketCount();
  explicit OwnedUDPSocketCount(bool empty);

  bool empty_;
};

// Attempts to increase the global "open UDP socket" [1] count.
//
// * On failure returns an OwnedUDPSocketCount that is |empty()|. This happens
//   if the global socket limit has been reached.
// * On success returns an OwnedUDPSocketCount that is |!empty()|. This
//   OwnedUDPSocketCount should be kept alive until the socket resource is
//   released.
//
// [1] For simplicity, an "open UDP socket" is defined as a net::UDPSocket that
// successfully called Open(), and has not yet called Close(). This is
// analogous to the number of open platform socket handles, and in practice
// should also be a good proxy for the number of consumed UDP ports.
[[nodiscard]] NET_EXPORT OwnedUDPSocketCount TryAcquireGlobalUDPSocketCount();

// Returns the current count of open UDP sockets (for testing only).
NET_EXPORT int GetGlobalUDPSocketCountForTesting();

}  // namespace net

#endif  // NET_SOCKET_UDP_SOCKET_GLOBAL_LIMITS_H_
