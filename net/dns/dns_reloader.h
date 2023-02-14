// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_RELOADER_H_
#define NET_DNS_DNS_RELOADER_H_

#include "build/build_config.h"

namespace net {

// Call on the network thread before calling DnsReloaderMaybeReload() anywhere.
void EnsureDnsReloaderInit();

// Call on the worker thread before doing a DNS lookup to reload the
// resolver for that thread by doing res_ninit() if required.
void DnsReloaderMaybeReload();

}  // namespace net

#endif  // NET_DNS_DNS_RELOADER_H_
