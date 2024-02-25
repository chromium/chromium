// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_DIFF_SERV_CODE_POINT_H_
#define NET_SOCKET_DIFF_SERV_CODE_POINT_H_

namespace net {

// Differentiated Services Code Point.
// See http://tools.ietf.org/html/rfc2474 for details.
enum DiffServCodePoint {
  DSCP_NO_CHANGE = -1,
  DSCP_FIRST = DSCP_NO_CHANGE,
  DSCP_DEFAULT = 0,  // Same as DSCP_CS0
  DSCP_CS0  = 0,   // The default
  DSCP_CS1  = 8,   // Bulk/background traffic
  DSCP_AF11 = 10,
  DSCP_AF12 = 12,
  DSCP_AF13 = 14,
  DSCP_CS2  = 16,
  DSCP_AF21 = 18,
  DSCP_AF22 = 20,
  DSCP_AF23 = 22,
  DSCP_CS3  = 24,
  DSCP_AF31 = 26,
  DSCP_AF32 = 28,
  DSCP_AF33 = 30,
  DSCP_CS4  = 32,
  DSCP_AF41 = 34,  // Video
  DSCP_AF42 = 36,  // Video
  DSCP_AF43 = 38,  // Video
  DSCP_CS5  = 40,  // Video
  DSCP_EF   = 46,  // Voice
  DSCP_CS6  = 48,  // Voice
  DSCP_CS7  = 56,  // Control messages
  DSCP_LAST = DSCP_CS7
};

// Explicit Congestion Notification
// See RFC3168 and RFC9330 for details.
enum EcnCodePoint {
  ECN_NO_CHANGE = -1,
  ECN_FIRST = ECN_NO_CHANGE,
  ECN_DEFAULT = 0,
  ECN_NOT_ECT = 0,
  ECN_ECT1 = 1,
  ECN_ECT0 = 2,
  ECN_CE = 3,
  ECN_LAST = ECN_CE,
};

struct DscpAndEcn {
  DiffServCodePoint dscp;
  EcnCodePoint ecn;
};

// Converts an 8-bit IP TOS field to its DSCP and ECN parts.
static inline DscpAndEcn TosToDscpAndEcn(uint8_t tos) {
  // Bitmasks to find the DSCP and ECN pieces of the TOS byte.
  constexpr uint8_t kEcnMask = 0b11;
  constexpr uint8_t kDscpMask = ~kEcnMask;
  return DscpAndEcn{static_cast<DiffServCodePoint>((tos & kDscpMask) >> 2),
                    static_cast<EcnCodePoint>(tos & kEcnMask)};
}

}  // namespace net

#endif  // NET_SOCKET_DIFF_SERV_CODE_POINT_H_
