// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_ADDRESS_MISMATCH_H_
#define NET_QUIC_QUIC_ADDRESS_MISMATCH_H_

#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"

namespace net {

enum QuicAddressMismatch {
  // The addresses don't match.
  QUIC_ADDRESS_MISMATCH_BASE = 0,
  QUIC_ADDRESS_MISMATCH_V4_V4 = 0,
  QUIC_ADDRESS_MISMATCH_V6_V6 = 1,
  QUIC_ADDRESS_MISMATCH_V4_V6 = 2,
  QUIC_ADDRESS_MISMATCH_V6_V4 = 3,

  // The addresses match, but the ports don't match.
  QUIC_PORT_MISMATCH_BASE = 4,
  QUIC_PORT_MISMATCH_V4_V4 = 4,
  QUIC_PORT_MISMATCH_V6_V6 = 5,

  QUIC_ADDRESS_AND_PORT_MATCH_BASE = 6,
  QUIC_ADDRESS_AND_PORT_MATCH_V4_V4 = 6,
  QUIC_ADDRESS_AND_PORT_MATCH_V6_V6 = 7,

  QUIC_ADDRESS_MISMATCH_MAX,
};

// Returns a value of the QuicAddressMismatch enum type that indicates how
// |first_address| differs from |second_address|. Returns -1 if either address
// is empty.
NET_EXPORT_PRIVATE int GetAddressMismatch(const IPEndPoint& first_address,
                                          const IPEndPoint& second_address);

}  // namespace net

#endif  // NET_QUIC_QUIC_ADDRESS_MISMATCH_H_
