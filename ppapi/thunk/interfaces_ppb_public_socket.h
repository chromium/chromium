// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// no-include-guard-because-multiply-included
// NOLINT(build/header_guard)

#include "ppapi/thunk/interfaces_preamble.h"

// See interfaces_ppp_public_stable.h for documentation on these macros.
PROXIED_IFACE(PPB_TCPSOCKET_INTERFACE_1_0, PPB_TCPSocket_1_0)
PROXIED_IFACE(PPB_TCPSOCKET_INTERFACE_1_1, PPB_TCPSocket_1_1)
PROXIED_IFACE(PPB_TCPSOCKET_INTERFACE_1_2, PPB_TCPSocket_1_2)
PROXIED_IFACE(PPB_UDPSOCKET_INTERFACE_1_0, PPB_UDPSocket_1_0)
PROXIED_IFACE(PPB_UDPSOCKET_INTERFACE_1_1, PPB_UDPSocket_1_1)
PROXIED_IFACE(PPB_UDPSOCKET_INTERFACE_1_2, PPB_UDPSocket_1_2)

// These interfaces only work for whitelisted origins.
PROXIED_IFACE(PPB_TCPSERVERSOCKET_PRIVATE_INTERFACE_0_1,
              PPB_TCPServerSocket_Private_0_1)
PROXIED_IFACE(PPB_TCPSERVERSOCKET_PRIVATE_INTERFACE_0_2,
              PPB_TCPServerSocket_Private_0_2)
PROXIED_IFACE(PPB_TCPSOCKET_PRIVATE_INTERFACE_0_3, PPB_TCPSocket_Private_0_3)
PROXIED_IFACE(PPB_TCPSOCKET_PRIVATE_INTERFACE_0_4, PPB_TCPSocket_Private_0_4)
PROXIED_IFACE(PPB_TCPSOCKET_PRIVATE_INTERFACE_0_5, PPB_TCPSocket_Private_0_5)
PROXIED_IFACE(PPB_UDPSOCKET_PRIVATE_INTERFACE_0_2, PPB_UDPSocket_Private_0_2)
PROXIED_IFACE(PPB_UDPSOCKET_PRIVATE_INTERFACE_0_3, PPB_UDPSocket_Private_0_3)
PROXIED_IFACE(PPB_UDPSOCKET_PRIVATE_INTERFACE_0_4, PPB_UDPSocket_Private_0_4)

#include "ppapi/thunk/interfaces_postamble.h"
