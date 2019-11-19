// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Provides a simple interface for QUIC tests to create a variety of packets.

#ifndef NET_QUIC_QUIC_TEST_PACKET_MAKER_H_
#define NET_QUIC_QUIC_TEST_PACKET_MAKER_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "net/base/request_priority.h"
#include "net/third_party/quiche/src/quic/core/http/http_encoder.h"
#include "net/third_party/quiche/src/quic/core/qpack/qpack_encoder.h"
#include "net/third_party/quiche/src/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quic/core/quic_stream_frame_data_producer.h"
#include "net/third_party/quiche/src/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_clock.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_string_piece.h"
#include "net/third_party/quiche/src/quic/test_tools/mock_random.h"
#include "net/third_party/quiche/src/spdy/core/spdy_framer.h"
#include "net/third_party/quiche/src/spdy/core/spdy_protocol.h"

namespace net {
namespace test {

class QuicTestPacketMaker {
 public:
  struct Http2StreamDependency {
    quic::QuicStreamId stream_id;
    quic::QuicStreamId parent_stream_id;
    spdy::SpdyPriority spdy_priority;
  };

  // |client_headers_include_h2_stream_dependency| affects the output of
  // the MakeRequestHeaders...() methods. If its value is true, then request
  // headers are constructed with the exclusive flag set to true and the parent
  // stream id set to the |parent_stream_id| param of MakeRequestHeaders...().
  // Otherwise, headers are constructed with the exclusive flag set to false and
  // the parent stream ID set to 0 (ignoring the |parent_stream_id| param).
  QuicTestPacketMaker(quic::ParsedQuicVersion version,
                      quic::QuicConnectionId connection_id,
                      const quic::QuicClock* clock,
                      const std::string& host,
                      quic::Perspective perspective,
                      bool client_headers_include_h2_stream_dependency);
  ~QuicTestPacketMaker();

  void set_hostname(const std::string& host);
  std::unique_ptr<quic::QuicReceivedPacket> MakeConnectivityProbingPacket(
      uint64_t num,
      bool include_version);
  std::unique_ptr<quic::QuicReceivedPacket> MakePingPacket(
      uint64_t num,
      bool include_version);
  std::unique_ptr<quic::QuicReceivedPacket> MakeDummyCHLOPacket(
      uint64_t packet_num);
  std::unique_ptr<quic::QuicReceivedPacket> MakeAckAndPingPacket(
      uint64_t num,
      bool include_version,
      uint64_t largest_received,
      uint64_t smallest_received,
      uint64_t least_unacked);

  std::unique_ptr<quic::QuicReceivedPacket> MakeStreamsBlockedPacket(
      uint64_t num,
      bool include_version,
      quic::QuicStreamCount stream_count,
      bool unidirectional);

  std::unique_ptr<quic::QuicReceivedPacket> MakeMaxStreamsPacket(
      uint64_t num,
      bool include_version,
      quic::QuicStreamCount stream_count,
      bool unidirectional);

  std::unique_ptr<quic::QuicReceivedPacket> MakeRstPacket(
      uint64_t num,
      bool include_version,
      quic::QuicStreamId stream_id,
      quic::QuicRstStreamErrorCode error_code);

  std::unique_ptr<quic::QuicReceivedPacket> MakeRstPacket(
      uint64_t num,
      bool include_version,
      quic::QuicStreamId stream_id,
      quic::QuicRstStreamErrorCode error_code,
      bool include_stop_sending_if_v99);

  std::unique_ptr<quic::QuicReceivedPacket> MakeRstAndRequestHeadersPacket(
      uint64_t num,
      bool include_version,
      quic::QuicStreamId rst_stream_id,
      quic::QuicRstStreamErrorCode rst_error_code,
      quic::QuicStreamId stream_id,
      bool fin,
      spdy::SpdyPriority priority,
      spdy::SpdyHeaderBlock headers,
      quic::QuicStreamId parent_stream_id,
      size_t* spdy_headers_frame_length);

  std::unique_ptr<quic::QuicReceivedPacket> MakeAckAndRstPacket(
      uint64_t num,
      bool include_version,
      quic::QuicStreamId stream_id,
      quic::QuicRstStreamErrorCode error_code,
      uint64_t largest_received,
      uint64_t smallest_received,
      uint64_t least_unacked,
      bool send_feedback);
  std::unique_ptr<quic::QuicReceivedPacket> MakeAckAndRstPacket(
      uint64_t num,
      bool include_version,
      quic::QuicStreamId stream_id,
      quic::QuicRstStreamErrorCode error_code,
      uint64_t largest_received,
      uint64_t smallest_received,
      uint64_t least_unacked,
      bool send_feedback,
      bool include_stop_sending_if_v99);
  std::unique_ptr<quic::QuicReceivedPacket> MakeRstAckAndConnectionClosePacket(
      uint64_t num,
      bool include_version,
      quic::QuicStreamId stream_id,
      quic::QuicRstStreamErrorCode error_code,
      quic::QuicTime::Delta delta_time_largest_observed,
      uint64_t largest_received,
      uint64_t smallest_received,
      uint64_t least_unacked,
      quic::QuicErrorCode quic_error,
      const std::string& quic_error_details);
  std::unique_ptr<quic::QuicReceivedPacket> MakeRstAndConnectionClosePacket(
      uint64_t num,
      bool include_version,
      quic::QuicStreamId stream_id,
      quic::QuicRstStreamErrorCode error_code,
      quic::QuicErrorCode quic_error,
      const std::string& quic_error_details);
  std::unique_ptr<quic::QuicReceivedPacket> MakeAckAndConnectionClosePacket(
      uint64_t num,
      bool include_version,
      quic::QuicTime::Delta delta_time_largest_observed,
      uint64_t largest_received,
      uint64_t smallest_received,
      uint64_t least_unacked,
      quic::QuicErrorCode quic_error,
      const std::string& quic_error_details,
      uint64_t frame_type);
  std::unique_ptr<quic::QuicReceivedPacket> MakeConnectionClosePacket(
      uint64_t num,
      bool include_version,
      quic::QuicErrorCode quic_error,
      const std::string& quic_error_details);
  std::unique_ptr<quic::QuicReceivedPacket> MakeGoAwayPacket(
      uint64_t num,
      quic::QuicErrorCode error_code,
      std::string reason_phrase);
  std::unique_ptr<quic::QuicReceivedPacket> MakeAckPacket(
      uint64_t packet_number,
      uint64_t largest_received,
      uint64_t smallest_received,
      uint64_t least_unacked,
      bool send_feedback);
  std::unique_ptr<quic::QuicReceivedPacket> MakeAckPacket(
      uint64_t packet_number,
      uint64_t first_received,
      uint64_t largest_received,
      uint64_t smallest_received,
      uint64_t least_unacked,
      bool send_feedback);
  std::unique_ptr<quic::QuicReceivedPacket> MakeAckPacket(
      uint64_t packet_number,
      uint64_t largest_received,
      uint64_t smallest_received,
      uint64_t least_unacked,
      bool send_feedback,
      quic::QuicTime::Delta ack_delay_time);
  std::unique_ptr<quic::QuicReceivedPacket> MakeAckPacket(
      uint64_t packet_number,
      uint64_t first_received,
      uint64_t largest_received,
      uint64_t smallest_received,
      uint64_t least_unacked,
      bool send_feedback,
      quic::QuicTime::Delta ack_delay_time);
  std::unique_ptr<quic::QuicReceivedPacket> MakeDataPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      quic::QuicStringPiece data);
  std::unique_ptr<quic::QuicReceivedPacket> MakeForceHolDataPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      quic::QuicStreamOffset* offset,
      quic::QuicStringPiece data);
  std::unique_ptr<quic::QuicReceivedPacket> MakeMultipleDataFramesPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      const std::vector<std::string>& data_writes);
  std::unique_ptr<quic::QuicReceivedPacket> MakeAckAndDataPacket(
      uint64_t packet_number,
      bool include_version,
      quic::QuicStreamId stream_id,
      uint64_t largest_received,
      uint64_t smallest_received,
      uint64_t least_unacked,
      bool fin,
      quic::QuicStringPiece data);
  std::unique_ptr<quic::QuicReceivedPacket> MakeAckAndMultipleDataFramesPacket(
      uint64_t packet_number,
      bool include_version,
      quic::QuicStreamId stream_id,
      uint64_t largest_received,
      uint64_t smallest_received,
      uint64_t least_unacked,
      bool fin,
      const std::vector<std::string>& data);

  std::unique_ptr<quic::QuicReceivedPacket>
  MakeRequestHeadersAndMultipleDataFramesPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      spdy::SpdyPriority priority,
      spdy::SpdyHeaderBlock headers,
      quic::QuicStreamId parent_stream_id,
      size_t* spdy_headers_frame_length,
      const std::vector<std::string>& data_writes);

  // If |spdy_headers_frame_length| is non-null, it will be set to the size of
  // the SPDY headers frame created for this packet.
  std::unique_ptr<quic::QuicReceivedPacket> MakeRequestHeadersPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      spdy::SpdyPriority priority,
      spdy::SpdyHeaderBlock headers,
      quic::QuicStreamId parent_stream_id,
      size_t* spdy_headers_frame_length);

  std::unique_ptr<quic::QuicReceivedPacket> MakeRequestHeadersAndRstPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      spdy::SpdyPriority priority,
      spdy::SpdyHeaderBlock headers,
      quic::QuicStreamId parent_stream_id,
      size_t* spdy_headers_frame_length,
      quic::QuicRstStreamErrorCode error_code);

  // If |spdy_headers_frame_length| is non-null, it will be set to the size of
  // the SPDY headers frame created for this packet.
  std::unique_ptr<quic::QuicReceivedPacket> MakePushPromisePacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      quic::QuicStreamId promised_stream_id,
      bool should_include_version,
      bool fin,
      spdy::SpdyHeaderBlock headers,
      size_t* spdy_headers_frame_length);

  // If |spdy_headers_frame_length| is non-null, it will be set to the size of
  // the SPDY headers frame created for this packet.
  std::unique_ptr<quic::QuicReceivedPacket> MakeResponseHeadersPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      spdy::SpdyHeaderBlock headers,
      size_t* spdy_headers_frame_length);

  // Creates a packet containing the initial SETTINGS frame, and saves the
  // headers stream offset into |offset|.
  std::unique_ptr<quic::QuicReceivedPacket> MakeInitialSettingsPacket(
      uint64_t packet_number);

  std::unique_ptr<quic::QuicReceivedPacket> MakePriorityPacket(
      uint64_t packet_number,
      bool should_include_version,
      quic::QuicStreamId id,
      quic::QuicStreamId parent_stream_id,
      spdy::SpdyPriority priority);

  std::unique_ptr<quic::QuicReceivedPacket>
  MakeAckAndMultiplePriorityFramesPacket(
      uint64_t packet_number,
      bool should_include_version,
      uint64_t largest_received,
      uint64_t smallest_received,
      uint64_t least_unacked,
      const std::vector<Http2StreamDependency>& priority_frames);

  std::unique_ptr<quic::QuicReceivedPacket> MakeRetransmissionPacket(
      uint64_t original_packet_number,
      uint64_t new_packet_number,
      bool should_include_version);

  // Removes all stream frames associated with |stream_id|.
  void RemoveSavedStreamFrames(quic::QuicStreamId stream_id);

  void SetEncryptionLevel(quic::EncryptionLevel level);

  spdy::SpdyHeaderBlock GetRequestHeaders(const std::string& method,
                                          const std::string& scheme,
                                          const std::string& path);

  spdy::SpdyHeaderBlock ConnectRequestHeaders(const std::string& host_port);

  spdy::SpdyHeaderBlock GetResponseHeaders(const std::string& status);

  spdy::SpdyHeaderBlock GetResponseHeaders(const std::string& status,
                                           const std::string& alt_svc);

  spdy::SpdyFramer* spdy_request_framer() { return &spdy_request_framer_; }
  spdy::SpdyFramer* spdy_response_framer() { return &spdy_response_framer_; }

  void Reset();

  quic::QuicStreamOffset stream_offset(quic::QuicStreamId stream_id) {
    return stream_offsets_[stream_id];
  }

  void set_coalesce_http_frames(bool coalesce_http_frames) {
    coalesce_http_frames_ = coalesce_http_frames;
  }

  void set_save_packet_frames(bool save_packet_frames) {
    save_packet_frames_ = save_packet_frames;
  }

  std::vector<std::string> QpackEncodeHeaders(quic::QuicStreamId stream_id,
                                              spdy::SpdyHeaderBlock headers,
                                              size_t* encoded_data_length);

 private:
  // QpackEncoder::DecoderStreamErrorDelegate implementation that does nothing
  class DecoderStreamErrorDelegate
      : public quic::QpackEncoder::DecoderStreamErrorDelegate {
   public:
    ~DecoderStreamErrorDelegate() override = default;

    void OnDecoderStreamError(quic::QuicStringPiece error_message) override;
  };

  // QpackEncoderStreamSender::Delegate implementation that does nothing.
  class EncoderStreamSenderDelegate : public quic::QpackStreamSenderDelegate {
   public:
    ~EncoderStreamSenderDelegate() override = default;

    void WriteStreamData(quic::QuicStringPiece data) override;
  };

  std::unique_ptr<quic::QuicReceivedPacket> MakePacket(
      const quic::QuicPacketHeader& header,
      const quic::QuicFrame& frame);
  std::unique_ptr<quic::QuicReceivedPacket> MakeMultipleFramesPacket(
      const quic::QuicPacketHeader& header,
      const quic::QuicFrames& frames,
      quic::QuicStreamFrameDataProducer* data_producer);

  void InitializeHeader(uint64_t packet_number, bool should_include_version);

  spdy::SpdySerializedFrame MakeSpdyHeadersFrame(
      quic::QuicStreamId stream_id,
      bool fin,
      spdy::SpdyPriority priority,
      spdy::SpdyHeaderBlock headers,
      quic::QuicStreamId parent_stream_id);

  bool ShouldIncludeVersion(bool include_version) const;

  // This mirrors quic_framer.cc::{anonymous namespace}::GenerateErrorString()
  // behavior.
  std::string MaybePrependErrorCode(const std::string& quic_error_details,
                                    quic::QuicErrorCode quic_error_code) const;

  quic::QuicFrame GenerateNextStreamFrame(quic::QuicStreamId stream_id,
                                          bool fin,
                                          quic::QuicStringPiece data);

  std::vector<quic::QuicFrame> GenerateNextStreamFrames(
      quic::QuicStreamId stream_id,
      bool fin,
      const std::vector<std::string>& data);

  quic::QuicPacketNumberLength GetPacketNumberLength() const;

  quic::QuicConnectionId DestinationConnectionId() const;
  quic::QuicConnectionId SourceConnectionId() const;

  quic::QuicConnectionIdIncluded HasDestinationConnectionId() const;
  quic::QuicConnectionIdIncluded HasSourceConnectionId() const;

  quic::QuicStreamId GetFirstBidirectionalStreamId() const;
  quic::QuicStreamId GetHeadersStreamId() const;

  std::string GenerateHttp3SettingsData();
  std::string GenerateHttp3PriorityData(spdy::SpdyPriority priority,
                                        quic::QuicStreamId stream_id);

  void MaybeAddHttp3SettingsFrames(quic::QuicFrames* frames);

  quic::ParsedQuicVersion version_;
  quic::QuicConnectionId connection_id_;
  const quic::QuicClock* clock_;  // Not owned.
  std::string host_;
  spdy::SpdyFramer spdy_request_framer_;
  spdy::SpdyFramer spdy_response_framer_;
  bool coalesce_http_frames_;
  bool save_packet_frames_;
  DecoderStreamErrorDelegate decoder_stream_error_delegate_;
  EncoderStreamSenderDelegate encoder_stream_sender_delegate_;
  quic::QpackEncoder qpack_encoder_;
  quic::test::MockRandom random_generator_;
  std::map<quic::QuicStreamId, quic::QuicStreamOffset> stream_offsets_;
  quic::QuicPacketHeader header_;
  quic::Perspective perspective_;
  quic::EncryptionLevel encryption_level_;
  quic::QuicLongHeaderType long_header_type_;
  std::vector<std::unique_ptr<std::string>> saved_stream_data_;
  std::map<quic::QuicPacketNumber, quic::QuicFrames> saved_frames_;

  // If true, generated request headers will include non-default HTTP2 stream
  // dependency info.
  bool client_headers_include_h2_stream_dependency_;

  DISALLOW_COPY_AND_ASSIGN(QuicTestPacketMaker);
};

}  // namespace test
}  // namespace net

#endif  // NET_QUIC_QUIC_TEST_PACKET_MAKER_H_
