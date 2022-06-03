// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_RELOADER_H_
#define NET_DNS_DNS_RELOADER_H_

#include "build/build_config.h"

#if defined(OS_POSIX) && !defined(OS_APPLE) && !defined(OS_OPENBSD)
namespace net {

// Call on the network thread before calling DnsReloaderMaybeReload() anywhere.
void EnsureDnsReloaderInit();

// Call on the worker thread before doing a DNS lookup to reload the
// resolver for that thread by doing res_ninit() if required.
void DnsReloaderMaybeReload();

}  // namespace net
#endif  // defined(OS_POSIX) && !defined(OS_APPLE) && !defined(OS_OPENBSD)

#endif  // NET_DNS_DNS_RELOADER_H_
