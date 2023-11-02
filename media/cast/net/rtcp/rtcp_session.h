// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_NET_RTCP_RTCP_SESSION_H_
#define MEDIA_CAST_NET_RTCP_RTCP_SESSION_H_

namespace media {
namespace cast {

// Base class for an RTCP session.
class RtcpSession {
 public:
  virtual ~RtcpSession() {}

  // Handle incoming RTCP packet.
  // Returns false if it is not a RTCP packet or it is not directed to
  // this session, e.g. SSRC doesn't match.
  virtual bool IncomingRtcpPacket(const uint8_t* data, size_t length) = 0;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_NET_RTCP_RTCP_SESSION_H_
