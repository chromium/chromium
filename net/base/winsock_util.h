// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_WINSOCK_UTIL_H_
#define NET_BASE_WINSOCK_UTIL_H_

#include <winsock2.h>

#include <stddef.h>

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

}  // namespace net

#endif  // NET_BASE_WINSOCK_UTIL_H_
