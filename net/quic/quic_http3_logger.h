// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_HTTP3_LOGGER_H_
#define NET_QUIC_QUIC_HTTP3_LOGGER_H_

#include <stddef.h>

#include "base/timer/timer.h"
#include "net/log/net_log_with_source.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/third_party/quiche/src/quiche/quic/core/http/quic_spdy_session.h"

namespace net {

// This class is a debug visitor of a quic::QuicSpdySession which logs events
// to |net_log| and records histograms.
class NET_EXPORT_PRIVATE QuicHttp3Logger : public quic::Http3DebugVisitor {
 public:
  explicit QuicHttp3Logger(const NetLogWithSource& net_log);

  QuicHttp3Logger(const QuicHttp3Logger&) = delete;
  QuicHttp3Logger& operator=(const QuicHttp3Logger&) = delete;

  ~QuicHttp3Logger() override;

  // Implementation of Http3DebugVisitor.
  void OnControlStreamCreated(quic::QuicStreamId stream_id) override;
  void OnQpackEncoderStreamCreated(quic::QuicStreamId stream_id) override;
  void OnQpackDecoderStreamCreated(quic::QuicStreamId stream_id) override;
  void OnPeerControlStreamCreated(quic::QuicStreamId stream_id) override;
  void OnPeerQpackEncoderStreamCreated(quic::QuicStreamId stream_id) override;
  void OnPeerQpackDecoderStreamCreated(quic::QuicStreamId stream_id) override;

  void OnSettingsFrameReceived(const quic::SettingsFrame& frame) override;
  void OnSettingsFrameResumed(const quic::SettingsFrame& frame) override;
  void OnGoAwayFrameReceived(const quic::GoAwayFrame& frame) override;
  void OnPriorityUpdateFrameReceived(
      const quic::PriorityUpdateFrame& frame) override;

  void OnDataFrameReceived(quic::QuicStreamId stream_id,
                           quic::QuicByteCount payload_length) override;
  void OnHeadersFrameReceived(
      quic::QuicStreamId stream_id,
      quic::QuicByteCount compressed_headers_length) override;
  void OnHeadersDecoded(quic::QuicStreamId stream_id,
                        quic::QuicHeaderList headers) override;

  void OnUnknownFrameReceived(quic::QuicStreamId stream_id,
                              uint64_t frame_type,
                              quic::QuicByteCount payload_length) override;

  void OnSettingsFrameSent(const quic::SettingsFrame& frame) override;
  void OnGoAwayFrameSent(quic::QuicStreamId stream_id) override;
  void OnPriorityUpdateFrameSent(
      const quic::PriorityUpdateFrame& frame) override;

  void OnDataFrameSent(quic::QuicStreamId stream_id,
                       quic::QuicByteCount payload_length) override;
  void OnHeadersFrameSent(quic::QuicStreamId stream_id,
                          const quiche::HttpHeaderBlock& header_block) override;

 private:
  NetLogWithSource net_log_;
};
}  // namespace net

#endif  // NET_QUIC_QUIC_HTTP3_LOGGER_H_
