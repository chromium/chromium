// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_HTTP_QUIC_SPDY_SERVER_STREAM_BASE_H_
#define NET_THIRD_PARTY_QUIC_CORE_HTTP_QUIC_SPDY_SERVER_STREAM_BASE_H_

#include "net/third_party/quic/core/http/quic_spdy_stream.h"

namespace quic {

class QuicSpdyServerStreamBase : public QuicSpdyStream {
 public:
  QuicSpdyServerStreamBase(QuicStreamId id,
                           QuicSpdySession* session,
                           StreamType type);
  QuicSpdyServerStreamBase(const QuicSpdyServerStreamBase&) = delete;
  QuicSpdyServerStreamBase& operator=(const QuicSpdyServerStreamBase&) = delete;

  // Override the base class to send QUIC_STREAM_NO_ERROR to the peer
  // when the stream has not received all the data.
  void CloseWriteSide() override;
  void StopReading() override;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_HTTP_QUIC_SPDY_SERVER_STREAM_BASE_H_
