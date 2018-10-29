// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/test_tools/mock_quic_spdy_client_stream.h"

namespace quic {
namespace test {

MockQuicSpdyClientStream::MockQuicSpdyClientStream(
    QuicStreamId id,
    QuicSpdyClientSession* session,
    StreamType type)
    : QuicSpdyClientStream(id, session, type) {}

MockQuicSpdyClientStream::~MockQuicSpdyClientStream() {}

}  // namespace test
}  // namespace quic
