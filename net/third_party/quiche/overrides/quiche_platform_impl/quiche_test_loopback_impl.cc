// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quiche/overrides/quiche_platform_impl/quiche_test_loopback_impl.h"

namespace quiche {

quic::IpAddressFamily AddressFamilyUnderTestImpl() {
  return quic::IpAddressFamily::IP_V4;
}

quic::QuicIpAddress TestLoopback4Impl() {
  return quic::QuicIpAddress::Loopback4();
}

quic::QuicIpAddress TestLoopback6Impl() {
  return quic::QuicIpAddress::Loopback6();
}

quic::QuicIpAddress TestLoopbackImpl() {
  return quic::QuicIpAddress::Loopback4();
}

quic::QuicIpAddress TestLoopbackImpl(int index) {
  const char kLocalhostIPv4[] = {127, 0, 0, static_cast<char>(index)};
  quic::QuicIpAddress address;
  address.FromPackedString(kLocalhostIPv4, 4);
  return address;
}

}  // namespace quiche
