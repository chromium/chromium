// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_TEST_LOOPBACK_IMPL_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_TEST_LOOPBACK_IMPL_H_

#include "net/third_party/quiche/src/quic/platform/api/quic_ip_address.h"

namespace quic {

// Returns the address family IPv4 used to run test under.
IpAddressFamily AddressFamilyUnderTestImpl();

// Returns an IPv4 loopback address.
QuicIpAddress TestLoopback4Impl();

// Returns the only IPv6 loopback address.
QuicIpAddress TestLoopback6Impl();

// Returns an IPv4 loopback address.
QuicIpAddress TestLoopbackImpl();

// Returns an indexed IPv4 loopback address.
QuicIpAddress TestLoopbackImpl(int index);

}  // namespace quic

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_TEST_LOOPBACK_IMPL_H_
