// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/platform/impl/quic_test_loopback_impl.h"

namespace quic {

IpAddressFamily AddressFamilyUnderTestImpl() {
  return IpAddressFamily::IP_V4;
}

QuicIpAddress TestLoopback4Impl() {
  return QuicIpAddress::Loopback4();
}

QuicIpAddress TestLoopback6Impl() {
  return QuicIpAddress::Loopback6();
}

QuicIpAddress TestLoopbackImpl() {
  return QuicIpAddress::Loopback4();
}

QuicIpAddress TestLoopbackImpl(int index) {
  const char kLocalhostIPv4[] = {127, 0, 0, index};
  QuicIpAddress address;
  address.FromPackedString(kLocalhostIPv4, 4);
  return address;
}

}  // namespace quic
