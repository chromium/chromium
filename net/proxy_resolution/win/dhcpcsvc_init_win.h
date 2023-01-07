// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_WIN_DHCPCSVC_INIT_WIN_H_
#define NET_PROXY_RESOLUTION_WIN_DHCPCSVC_INIT_WIN_H_

namespace net {

// Initialization of the Dhcpcsvc library must happen before any of its
// calls are made.  This function will make sure that the appropriate
// initialization has been done, and that uninitialization is also
// performed at static uninitialization time.
//
// Note: This initializes only for DHCP, not DHCPv6.
void EnsureDhcpcsvcInit();

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_WIN_DHCPCSVC_INIT_WIN_H_
