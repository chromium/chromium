// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/winsock_init.h"

#include <winsock2.h>

#include <type_traits>

#include "base/check.h"
#include "base/no_destructor.h"

namespace {

class WinsockInitSingleton {
 public:
  WinsockInitSingleton() {
    WORD winsock_ver = MAKEWORD(2, 2);
    WSAData wsa_data;
    bool did_init = (WSAStartup(winsock_ver, &wsa_data) == 0);
    if (did_init) {
      DCHECK(wsa_data.wVersion == winsock_ver);

      // The first time WSAGetLastError is called, the delay load helper will
      // resolve the address with GetProcAddress and fixup the import.  If a
      // third party application hooks system functions without correctly
      // restoring the error code, it is possible that the error code will be
      // overwritten during delay load resolution.  The result of the first
      // call may be incorrect, so make sure the function is bound and future
      // results will be correct.
      WSAGetLastError();
    }
  }
};

}  // namespace

namespace net {

void EnsureWinsockInit() {
  // Worker pool threads that use the Windows Sockets API may still be running
  // at shutdown. Leak instance and skip cleanup.
  static_assert(std::is_trivially_destructible<WinsockInitSingleton>::value);
  static WinsockInitSingleton singleton;
}

}  // namespace net
