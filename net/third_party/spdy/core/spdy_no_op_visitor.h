// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// SpdyNoOpVisitor implements several of the visitor and handler interfaces
// to make it easier to write tests that need to provide instances. Other
// interfaces can be added as needed.

#ifndef NET_THIRD_PARTY_SPDY_CORE_SPDY_NO_OP_VISITOR_H_
#define NET_THIRD_PARTY_SPDY_CORE_SPDY_NO_OP_VISITOR_H_

#include <cstdint>

#include "net/third_party/spdy/core/http2_frame_decoder_adapter.h"
#include "net/third_party/spdy/core/spdy_protocol.h"
#include "net/third_party/spdy/platform/api/spdy_string_piece.h"

namespace spdy {
namespace test {

class SpdyNoOpVisitor : public SpdyFramerVisitorInterface,
                        public SpdyFramerDebugVisitorInterface,
                        public SpdyHeadersHandlerInterface {
 public:
  SpdyNoOpVisitor();
  ~SpdyNoOpVisitor() override;

  // SpdyFramerVisitorInterface methods:
  void OnError(http2::Http2DecoderAdapter::SpdyFramerError error) override {}
  SpdyHeadersHandlerInterface* OnHeaderFrameStart(
      SpdyStreamId stream_id) override;
  void OnHeaderFrameEnd(SpdyStreamId stream_id) override {}
  void OnDataFrameHeader(SpdyStreamId stream_id,
                         size_t length,
                         bool fin) override {}
  void OnStreamFrameData(SpdyStreamId stream_id,
                         const char* data,
                         size_t len) override {}
  void OnStreamEnd(SpdyStreamId stream_id) override {}
  void OnStreamPadding(SpdyStreamId stream_id, size_t len) override {}
  void OnRstStream(SpdyStreamId stream_id, SpdyErrorCode error_code) override {}
  void OnSetting(SpdySettingsId id, uint32_t value) override {}
  void OnPing(SpdyPingId unique_id, bool is_ack) override {}
  void OnSettingsEnd() override {}
  void OnSettingsAck() override {}
  void OnGoAway(SpdyStreamId last_accepted_stream_id,
                SpdyErrorCode error_code) override {}
  void OnHeaders(SpdyStreamId stream_id,
                 bool has_priority,
                 int weight,
                 SpdyStreamId parent_stream_id,
                 bool exclusive,
                 bool fin,
                 bool end) override {}
  void OnWindowUpdate(SpdyStreamId stream_id, int delta_window_size) override {}
  void OnPushPromise(SpdyStreamId stream_id,
                     SpdyStreamId promised_stream_id,
                     bool end) override {}
  void OnContinuation(SpdyStreamId stream_id, bool end) override {}
  void OnAltSvc(SpdyStreamId stream_id,
                SpdyStringPiece origin,
                const SpdyAltSvcWireFormat::AlternativeServiceVector&
                    altsvc_vector) override {}
  void OnPriority(SpdyStreamId stream_id,
                  SpdyStreamId parent_stream_id,
                  int weight,
                  bool exclusive) override {}
  bool OnUnknownFrame(SpdyStreamId stream_id, uint8_t frame_type) override;

  // SpdyFramerDebugVisitorInterface methods:
  void OnSendCompressedFrame(SpdyStreamId stream_id,
                             SpdyFrameType type,
                             size_t payload_len,
                             size_t frame_len) override {}
  void OnReceiveCompressedFrame(SpdyStreamId stream_id,
                                SpdyFrameType type,
                                size_t frame_len) override {}

  // SpdyHeadersHandlerInterface methods:
  void OnHeaderBlockStart() override {}
  void OnHeader(SpdyStringPiece key, SpdyStringPiece value) override {}
  void OnHeaderBlockEnd(size_t /* uncompressed_header_bytes */,
                        size_t /* compressed_header_bytes */) override {}
};

}  // namespace test
}  // namespace spdy

#endif  // NET_THIRD_PARTY_SPDY_CORE_SPDY_NO_OP_VISITOR_H_
