// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Winsock initialization must happen before any Winsock calls are made.  The
// EnsureWinsockInit method will make sure that WSAStartup has been called.

#ifndef NET_BASE_WINSOCK_INIT_H_
#define NET_BASE_WINSOCK_INIT_H_

#include "net/base/net_export.h"

namespace net {

// Make sure that Winsock is initialized, calling WSAStartup if needed.
NET_EXPORT void EnsureWinsockInit();

}  // namespace net

#endif  // NET_BASE_WINSOCK_INIT_H_
