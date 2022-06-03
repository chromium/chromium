// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_address_mismatch.h"

#include "base/check_op.h"
#include "net/base/ip_address.h"

namespace net {

int GetAddressMismatch(const IPEndPoint& first_address,
                       const IPEndPoint& second_address) {
  if (first_address.address().empty() || second_address.address().empty()) {
    return -1;
  }
  IPAddress first_ip_address = first_address.address();
  if (first_ip_address.IsIPv4MappedIPv6()) {
    first_ip_address = ConvertIPv4MappedIPv6ToIPv4(first_ip_address);
  }
  IPAddress second_ip_address = second_address.address();
  if (second_ip_address.IsIPv4MappedIPv6()) {
    second_ip_address = ConvertIPv4MappedIPv6ToIPv4(second_ip_address);
  }

  int sample;
  if (first_ip_address != second_ip_address) {
    sample = QUIC_ADDRESS_MISMATCH_BASE;
  } else if (first_address.port() != second_address.port()) {
    sample = QUIC_PORT_MISMATCH_BASE;
  } else {
    sample = QUIC_ADDRESS_AND_PORT_MATCH_BASE;
  }

  // Add an offset to |sample|:
  //   V4_V4: add 0
  //   V6_V6: add 1
  //   V4_V6: add 2
  //   V6_V4: add 3
  bool first_ipv4 = first_ip_address.IsIPv4();
  if (first_ipv4 != second_ip_address.IsIPv4()) {
    CHECK_EQ(sample, QUIC_ADDRESS_MISMATCH_BASE);
    sample += 2;
  }
  if (!first_ipv4) {
    sample += 1;
  }
  return sample;
}

}  // namespace net
