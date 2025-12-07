// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_WINSOCK_UTIL_H_
#define NET_BASE_WINSOCK_UTIL_H_

#include <winsock2.h>

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <type_traits>

#include "base/containers/span.h"

namespace net {

// Bluetooth address size. Windows Bluetooth is supported via winsock.
static const size_t kBluetoothAddressSize = 6;

// If the (manual-reset) event object is signaled, resets it and returns true.
// Otherwise, does nothing and returns false.  Called after a Winsock function
// succeeds synchronously
//
// Our testing shows that except in rare cases (when running inside QEMU),
// the event object is already signaled at this point, so we call this method
// to avoid a context switch in common cases.  This is just a performance
// optimization.  The code still works if this function simply returns false.
bool ResetEventIfSignaled(WSAEVENT hEvent);

// OVERLAPPED structs contain unions, and thus should not be initialized by `=
// {}` lest there still be uninitialized bytes in union members larger than the
// first. And some platforms, OVERLAPPED structs contain padding, and thus do
// not meet `std::has_unique_object_representations_v`, so they need to be
// explicitly allowed for byte spanification. These helpers encapsulate both
// complexities.
template <typename T>
void FillOVERLAPPEDStruct(T& t, uint8_t value) {
  if constexpr (std::has_unique_object_representations_v<T>) {
    std::ranges::fill(base::byte_span_from_ref(t), value);
  } else {
    std::ranges::fill(base::byte_span_from_ref(base::allow_nonunique_obj, t),
                      value);
  }
}

}  // namespace net

#endif  // NET_BASE_WINSOCK_UTIL_H_
