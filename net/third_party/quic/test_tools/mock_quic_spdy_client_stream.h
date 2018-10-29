// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_TEST_TOOLS_MOCK_QUIC_SPDY_CLIENT_STREAM_H_
#define NET_THIRD_PARTY_QUIC_TEST_TOOLS_MOCK_QUIC_SPDY_CLIENT_STREAM_H_

#include "base/macros.h"
#include "net/third_party/quic/core/http/quic_header_list.h"
#include "net/third_party/quic/core/http/quic_spdy_client_stream.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace quic {
namespace test {

class MockQuicSpdyClientStream : public QuicSpdyClientStream {
 public:
  MockQuicSpdyClientStream(QuicStreamId id,
                           QuicSpdyClientSession* session,
                           StreamType type);
  ~MockQuicSpdyClientStream() override;

  MOCK_METHOD1(OnStreamFrame, void(const QuicStreamFrame& frame));
  MOCK_METHOD3(OnPromiseHeaderList,
               void(QuicStreamId promised_stream_id,
                    size_t frame_len,
                    const QuicHeaderList& list));
  MOCK_METHOD0(OnDataAvailable, void());
};

}  // namespace test
}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_TEST_TOOLS_MOCK_QUIC_SPDY_CLIENT_STREAM_H_
