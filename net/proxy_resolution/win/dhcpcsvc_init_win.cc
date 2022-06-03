// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/win/dhcpcsvc_init_win.h"

#include "base/check_op.h"
#include "base/lazy_instance.h"

#include <windows.h>  // Must be in front of other Windows header files.

#include <dhcpcsdk.h>
#include <dhcpv6csdk.h>

namespace {

class DhcpcsvcInitSingleton {
 public:
  DhcpcsvcInitSingleton() {
    DWORD version = 0;
    DWORD err = DhcpCApiInitialize(&version);
    DCHECK(err == ERROR_SUCCESS);  // DCHECK_EQ complains of unsigned mismatch.
  }
};

// Worker pool threads that use the DHCP API may still be running at shutdown.
// Leak instance and skip cleanup.
static base::LazyInstance<DhcpcsvcInitSingleton>::Leaky
    g_dhcpcsvc_init_singleton = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace net {

void EnsureDhcpcsvcInit() {
  g_dhcpcsvc_init_singleton.Get();
}

}  // namespace net
