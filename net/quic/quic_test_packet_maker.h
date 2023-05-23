// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Provides a simple interface for QUIC tests to create a variety of packets.

#ifndef NET_QUIC_QUIC_TEST_PACKET_MAKER_H_
#define NET_QUIC_QUIC_TEST_PACKET_MAKER_H_

#include <stddef.h>
#include <sys/types.h>

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "net/base/request_priority.h"
#include "net/third_party/quiche/src/quiche/quic/core/http/http_encoder.h"
#include "net/third_party/quiche/src/quiche/quic/core/qpack/qpack_encoder.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_clock.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_stream_frame_data_producer.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/mock_random.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/qpack/qpack_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/simple_data_producer.h"

namespace net::test {

class QuicTestPacketMaker {
 public:
  // |client_priority_uses_incremental| affects the output of any method that
  // includes HTTP3 priority data. The protocol default is to omit the
  // incremental flag in the priority data but HTTP streams may enable it
  // if the feature kPriorityIncremental is enabled.
  QuicTestPacketMaker(quic::ParsedQuicVersion version,
                      quic::QuicConnectionId connection_id,
                      const quic::QuicClock* clock,
                      const std::string& host,
                      quic::Perspective perspective,
                      bool client_priority_uses_incremental = false);

  QuicTestPacketMaker(const QuicTestPacketMaker&) = delete;
  QuicTestPacketMaker& operator=(const QuicTestPacketMaker&) = delete;

  ~QuicTestPacketMaker();

  void set_hostname(const std::string& host);

  void set_connection_id(const quic::QuicConnectionId& connection_id) {
    connection_id_ = connection_id;
  }

  std::unique_ptr<quic::QuicReceivedPacket> MakeConnectivityProbingPacket(
      uint64_t num);

  std::unique_ptr<quic::QuicReceivedPacket> MakePingPacket(uint64_t num);

  std::unique_ptr<quic::QuicReceivedPacket> MakeRetireConnectionIdPacket(
      uint64_t num,
      uint64_t sequence_number);

  std::unique_ptr<quic::QuicReceivedPacket> MakeNewConnectionIdPacket(
      uint64_t num,
      const quic::QuicConnectionId& cid,
      uint64_t sequence_number,
      uint64_t retire_prior_to);

  std::unique_ptr<quic::QuicReceivedPacket> MakeAckAndNewConnectionIdPacket(
      uint64_t num,
      uint64_t largest_received,
      uint64_t smallest_received,
      const quic::QuicConnectionId& cid,
      uint64_t sequence_number,
      uint64_t retire_prior_to);

  std::unique_ptr<quic::QuicReceivedPacket> MakeDummyCHLOPacket(
      uint64_t packet_num);

  std::unique_ptr<quic::QuicReceivedPacket> MakeAckAndPingPacket(
      uint64_t num,
      uint64_t largest_received,
      uint64_t smallest_received);

  std::unique_ptr<quic::QuicReceivedPacket> MakeAckAndRetireConnectionIdPacket(
      uint64_t num,
      uint64_t largest_received,
      uint64_t smallest_received,
      uint64_t sequence_number);

  std::unique_ptr<quic::QuicReceivedPacket>
  MakeRetransmissionAndRetireConnectionIdPacket(
      uint64_t num,
      const std::vector<uint64_t>& original_packet_numbers,
      uint64_t sequence_number);

  std::unique_ptr<quic::QuicReceivedPacket> MakeStreamsBlockedPacket(
      uint64_t num,
      quic::QuicStreamCount stream_count,
      bool unidirectional);

  std::unique_ptr<quic::QuicReceivedPacket> MakeMaxStreamsPacket(
      uint64_t num,
      quic::QuicStreamCount stream_count,
      bool unidirectional);

  std::unique_ptr<quic::QuicReceivedPacket> MakeRstPacket(
      uint64_t num,
      quic::QuicStreamId stream_id,
      quic::QuicRstStreamErrorCode error_code);

  std::unique_ptr<quic::QuicReceivedPacket> MakeRstPacket(
      uint64_t num,
      quic::QuicStreamId stream_id,
      quic::QuicRstStreamErrorCode error_code,
      bool include_stop_sending_if_v99);

  std::unique_ptr<quic::QuicReceivedPacket> MakeRstAndDataPacket(
      uint64_t num,
      quic::QuicStreamId rst_stream_id,
      quic::QuicRstStreamErrorCode rst_error_code,
      quic::QuicStreamId data_stream_id,
      absl::string_view data);

  std::unique_ptr<quic::QuicReceivedPacket> MakeRetransmissionRstAndDataPacket(
      const std::vector<uint64_t>& original_packet_numbers,
      uint64_t num,
      quic::QuicStreamId rst_stream_id,
      quic::QuicRstStreamErrorCode rst_error_code,
      quic::QuicStreamId data_stream_id,
      absl::string_view data,
      uint64_t retransmit_frame_count = 0);

  std::unique_ptr<quic::QuicReceivedPacket> MakeDataAndRstPacket(
      uint64_t num,
      quic::QuicStreamId data_stream_id,
      absl::string_view data,
      quic::QuicStreamId rst_stream_id,
      quic::QuicRstStreamErrorCode rst_error_code);

  std::unique_ptr<quic::QuicReceivedPacket> MakeDataRstAndAckPacket(
      uint64_t num,
      quic::QuicStreamId data_stream_id,
      absl::string_view data,
      quic::QuicStreamId rst_stream_id,
      quic::QuicRstStreamErrorCode rst_error_code,
      uint64_t largest_received,
      uint64_t smallest_received);

  std::unique_ptr<quic::QuicReceivedPacket> MakeAckAndRstPacket(
      uint64_t num,
      quic::QuicStreamId stream_id,
      quic::QuicRstStreamErrorCode error_code,
      uint64_t largest_received,
      uint64_t smallest_received);

  std::unique_ptr<quic::QuicReceivedPacket> MakeAckAndRstPacket(
      uint64_t num,
      quic::QuicStreamId stream_id,
      quic::QuicRstStreamErrorCode error_code,
      uint64_t largest_received,
      uint64_t smallest_received,
      bool include_stop_sending_if_v99);

  std::unique_ptr<quic::QuicReceivedPacket> MakeRstAckAndConnectionClosePacket(
      uint64_t num,
      quic::QuicStreamId stream_id,
      quic::QuicRstStreamErrorCode error_code,
      uint64_t largest_received,
      uint64_t smallest_received,
      quic::QuicErrorCode quic_error,
      const std::string& quic_error_details);

  std::unique_ptr<quic::QuicReceivedPacket> MakeRstAckAndDataPacket(
      uint64_t num,
      quic::QuicStreamId stream_id,
      quic::QuicRstStreamErrorCode error_code,
      uint64_t largest_received,
      uint64_t smallest_received,
      quic::QuicStreamId data_id,
      bool fin,
      absl::string_view data);

  std::unique_ptr<quic::QuicReceivedPacket> MakeAckDataAndRst(
      uint64_t num,
      quic::QuicStreamId stream_id,
      quic::QuicRstStreamErrorCode error_code,
      uint64_t largest_received,
      uint64_t smallest_received,
      quic::QuicStreamId data_id,
      bool fin,
      absl::string_view data);

  std::unique_ptr<quic::QuicReceivedPacket> MakeAckRstAndDataPacket(
      uint64_t num,
      quic::QuicStreamId stream_id,
      quic::QuicRstStreamErrorCode error_code,
      uint64_t largest_received,
      uint64_t smallest_received,
      quic::QuicStreamId data_id,
      bool fin,
      absl::string_view data);

  std::unique_ptr<quic::QuicReceivedPacket> MakeRstAndConnectionClosePacket(
      uint64_t num,
      quic::QuicStreamId stream_id,
      quic::QuicRstStreamErrorCode error_code,
      quic::QuicErrorCode quic_error,
      const std::string& quic_error_details);

  std::unique_ptr<quic::QuicReceivedPacket> MakeDataRstAndConnectionClosePacket(
      uint64_t num,
      quic::QuicStreamId data_stream_id,
      absl::string_view data,
      quic::QuicStreamId rst_stream_id,
      quic::QuicRstStreamErrorCode error_code,
      quic::QuicErrorCode quic_error,
      const std::string& quic_error_details);

  std::unique_ptr<quic::QuicReceivedPacket>
  MakeDataRstAckAndConnectionClosePacket(
      uint64_t num,
      quic::QuicStreamId data_stream_id,
      absl::string_view data,
      quic::QuicStreamId rst_stream_id,
      quic::QuicRstStreamErrorCode error_code,
      uint64_t largest_received,
      uint64_t smallest_received,
      quic::QuicErrorCode quic_error,
      const std::string& quic_error_details);

  std::unique_ptr<quic::QuicReceivedPacket>
  MakeDataRstAckAndConnectionClosePacket(
      uint64_t num,
      quic::QuicStreamId data_stream_id,
      absl::string_view data,
      quic::QuicStreamId rst_stream_id,
      quic::QuicRstStreamErrorCode error_code,
      uint64_t largest_received,
      uint64_t smallest_received,
      quic::QuicErrorCode quic_error,
      const std::string& quic_error_details,
      uint64_t frame_type);

  std::unique_ptr<quic::QuicReceivedPacket> MakeStopSendingPacket(
      uint64_t num,
      quic::QuicStreamId stream_id,
      quic::QuicRstStreamErrorCode error_code);

  std::unique_ptr<quic::QuicReceivedPacket> MakeAckAndConnectionClosePacket(
      uint64_t num,
      uint64_t largest_received,
      uint64_t smallest_received,
      quic::QuicErrorCode quic_error,
      const std::string& quic_error_details,
      uint64_t frame_type);

  std::unique_ptr<quic::QuicReceivedPacket> MakeConnectionClosePacket(
      uint64_t num,
      quic::QuicErrorCode quic_error,
      const std::string& quic_error_details);

  std::unique_ptr<quic::QuicReceivedPacket> MakeGoAwayPacket(
      uint64_t num,
      quic::QuicErrorCode error_code,
      std::string reason_phrase);

  std::unique_ptr<quic::QuicReceivedPacket> MakeAckPacket(
      uint64_t packet_number,
      uint64_t largest_received,
      uint64_t smallest_received);

  std::unique_ptr<quic::QuicReceivedPacket> MakeAckPacket(
      uint64_t packet_number,
      uint64_t first_received,
      uint64_t largest_received,
      uint64_t smallest_received);

  std::unique_ptr<quic::QuicReceivedPacket> MakeDataPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool fin,
      absl::string_view data);

  std::unique_ptr<quic::QuicReceivedPacket> MakeAckAndDataPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      uint64_t largest_received,
      uint64_t smallest_received,
      bool fin,
      absl::string_view data);

  std::unique_ptr<quic::QuicReceivedPacket> MakeAckRetransmissionAndDataPacket(
      uint64_t packet_number,
      const std::vector<uint64_t>& original_packet_numbers,
      quic::QuicStreamId stream_id,
      uint64_t largest_received,
      uint64_t smallest_received,
      bool fin,
      absl::string_view data);

  std::unique_ptr<quic::QuicReceivedPacket> MakeAckAndRetransmissionPacket(
      uint64_t packet_number,
      uint64_t first_received,
      uint64_t largest_received,
      uint64_t smallest_received,
      const std::vector<uint64_t>& original_packet_numbers);

  std::unique_ptr<quic::QuicReceivedPacket>
  MakeRequestHeadersAndMultipleDataFramesPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool fin,
      spdy::SpdyPriority spdy_priority,
      spdy::Http2HeaderBlock headers,
      size_t* spdy_headers_frame_length,
      const std::vector<std::string>& data_writes);

  // If |spdy_headers_frame_length| is non-null, it will be set to the size of
  // the SPDY headers frame created for this packet.
  std::unique_ptr<quic::QuicReceivedPacket> MakeRequestHeadersPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool fin,
      spdy::SpdyPriority spdy_priority,
      spdy::Http2HeaderBlock headers,
      size_t* spdy_headers_frame_length,
      bool should_include_priority_frame = true);

  std::unique_ptr<quic::QuicReceivedPacket>
  MakeRetransmissionAndRequestHeadersPacket(
      const std::vector<uint64_t>& original_packet_numbers,
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool fin,
      spdy::SpdyPriority spdy_priority,
      spdy::Http2HeaderBlock headers,
      size_t* spdy_headers_frame_length);

  std::unique_ptr<quic::QuicReceivedPacket> MakeRequestHeadersAndRstPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool fin,
      spdy::SpdyPriority spdy_priority,
      spdy::Http2HeaderBlock headers,
      size_t* spdy_headers_frame_length,
      quic::QuicRstStreamErrorCode error_code);

  // If |spdy_headers_frame_length| is non-null, it will be set to the size of
  // the SPDY headers frame created for this packet.
  std::unique_ptr<quic::QuicReceivedPacket> MakeResponseHeadersPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool fin,
      spdy::Http2HeaderBlock headers,
      size_t* spdy_headers_frame_length);

  // Creates a packet containing the initial SETTINGS frame, and saves the
  // headers stream offset into |offset|.
  std::unique_ptr<quic::QuicReceivedPacket> MakeInitialSettingsPacket(
      uint64_t packet_number);

  std::unique_ptr<quic::QuicReceivedPacket> MakePriorityPacket(
      uint64_t packet_number,
      quic::QuicStreamId id,
      spdy::SpdyPriority spdy_priority);

  std::unique_ptr<quic::QuicReceivedPacket> MakeAckAndPriorityPacket(
      uint64_t packet_number,
      uint64_t largest_received,
      uint64_t smallest_received,
      quic::QuicStreamId id,
      spdy::SpdyPriority spdy_priority);

  std::unique_ptr<quic::QuicReceivedPacket> MakeRetransmissionPacket(
      uint64_t original_packet_number,
      uint64_t new_packet_number);

  std::unique_ptr<quic::QuicReceivedPacket> MakeCombinedRetransmissionPacket(
      const std::vector<uint64_t>& original_packet_numbers,
      uint64_t new_packet_number);

  std::unique_ptr<quic::QuicReceivedPacket> MakeAckAndPriorityUpdatePacket(
      uint64_t packet_number,
      uint64_t largest_received,
      uint64_t smallest_received,
      quic::QuicStreamId id,
      spdy::SpdyPriority spdy_priority);

  std::unique_ptr<quic::QuicEncryptedPacket> MakeStatelessResetPacket();

  // Removes all stream frames associated with |stream_id|.
  void RemoveSavedStreamFrames(quic::QuicStreamId stream_id);

  void SetEncryptionLevel(quic::EncryptionLevel level);

  spdy::Http2HeaderBlock GetRequestHeaders(const std::string& method,
                                           const std::string& scheme,
                                           const std::string& path) const;

  spdy::Http2HeaderBlock ConnectRequestHeaders(
      const std::string& host_port) const;

  spdy::Http2HeaderBlock GetResponseHeaders(const std::string& status) const;

  spdy::Http2HeaderBlock GetResponseHeaders(const std::string& status,
                                            const std::string& alt_svc) const;

  void Reset();

  quic::QuicStreamOffset stream_offset(quic::QuicStreamId stream_id) {
    return stream_offsets_[stream_id];
  }

  void set_save_packet_frames(bool save_packet_frames) {
    save_packet_frames_ = save_packet_frames;
  }

  std::string QpackEncodeHeaders(quic::QuicStreamId stream_id,
                                 spdy::Http2HeaderBlock headers,
                                 size_t* encoded_data_length);

 private:
  // Initialize header of next packet to build.
  void InitializeHeader(uint64_t packet_number);

  // Add frames to current packet.
  void AddQuicPaddingFrame();
  void AddQuicPingFrame();
  void AddQuicRetireConnectionIdFrame(uint64_t sequence_number);
  void AddQuicNewConnectionIdFrame(const quic::QuicConnectionId& cid,
                                   uint64_t sequence_number,
                                   uint64_t retire_prior_to,
                                   quic::StatelessResetToken reset_token);
  void AddQuicMaxStreamsFrame(quic::QuicControlFrameId control_frame_id,
                              quic::QuicStreamCount stream_count,
                              bool unidirectional);
  void AddQuicStreamsBlockedFrame(quic::QuicControlFrameId control_frame_id,
                                  quic::QuicStreamCount stream_count,
                                  bool unidirectional);
  // Use and increase stream's current offset.
  void AddQuicStreamFrame(quic::QuicStreamId stream_id,
                          bool fin,
                          absl::string_view data);
  // Use |offset| and do not change stream's current offset.
  void AddQuicStreamFrameWithOffset(quic::QuicStreamId stream_id,
                                    bool fin,
                                    quic::QuicStreamOffset offset,
                                    absl::string_view data);
  void AddQuicAckFrame(uint64_t largest_received, uint64_t smallest_received);
  void AddQuicAckFrame(uint64_t first_received,
                       uint64_t largest_received,
                       uint64_t smallest_received);
  void AddQuicRstStreamFrame(quic::QuicStreamId stream_id,
                             quic::QuicRstStreamErrorCode error_code);
  void AddQuicConnectionCloseFrame(quic::QuicErrorCode quic_error,
                                   const std::string& quic_error_details);
  void AddQuicConnectionCloseFrame(quic::QuicErrorCode quic_error,
                                   const std::string& quic_error_details,
                                   uint64_t frame_type);
  void AddQuicGoAwayFrame(quic::QuicErrorCode error_code,
                          std::string reason_phrase);
  void AddQuicPathResponseFrame();
  void AddQuicPathChallengeFrame();
  void AddQuicStopSendingFrame(quic::QuicStreamId stream_id,
                               quic::QuicRstStreamErrorCode error_code);
  void AddQuicCryptoFrame(quic::EncryptionLevel level,
                          quic::QuicStreamOffset offset,
                          quic::QuicPacketLength data_length);

  // Build packet using |header_|, |frames_|, and |data_producer_|,
  // and clear |frames_| and |data_producer_| afterwards.
  std::unique_ptr<quic::QuicReceivedPacket> BuildPacket();

  // Build packet using |header_|, |frames|, and |data_producer|.
  std::unique_ptr<quic::QuicReceivedPacket> BuildPacketImpl(
      const quic::QuicFrames& frames,
      quic::QuicStreamFrameDataProducer* data_producer);

  bool ShouldIncludeVersion() const;

  quic::QuicPacketNumberLength GetPacketNumberLength() const;

  quic::QuicConnectionId DestinationConnectionId() const;
  quic::QuicConnectionId SourceConnectionId() const;

  quic::QuicConnectionIdIncluded HasDestinationConnectionId() const;
  quic::QuicConnectionIdIncluded HasSourceConnectionId() const;

  quic::QuicStreamId GetFirstBidirectionalStreamId() const;
  quic::QuicStreamId GetHeadersStreamId() const;

  std::string GenerateHttp3SettingsData();
  std::string GenerateHttp3PriorityData(spdy::SpdyPriority spdy_priority,
                                        quic::QuicStreamId stream_id);
  std::string GenerateHttp3GreaseData();

  void MaybeAddHttp3SettingsFrames();
  bool MaybeCoalesceStreamFrame(const quic::QuicFrame& frame);

  // Parameters used throughout the lifetime of the class.
  quic::ParsedQuicVersion version_;
  quic::QuicConnectionId connection_id_;
  raw_ptr<const quic::QuicClock> clock_;  // Not owned.
  std::string host_;
  quic::NoopDecoderStreamErrorDelegate decoder_stream_error_delegate_;
  quic::test::NoopQpackStreamSenderDelegate encoder_stream_sender_delegate_;
  quic::QpackEncoder qpack_encoder_;
  quic::test::MockRandom random_generator_;
  std::map<quic::QuicStreamId, quic::QuicStreamOffset> stream_offsets_;
  quic::Perspective perspective_;
  quic::EncryptionLevel encryption_level_ = quic::ENCRYPTION_FORWARD_SECURE;
  quic::QuicLongHeaderType long_header_type_ = quic::INVALID_PACKET_TYPE;

  // The value of incremental flag in generated priority headers.
  bool client_priority_uses_incremental_;

  // Save a copy of stream frame data that QuicStreamFrame objects can refer to.
  std::vector<std::unique_ptr<std::string>> saved_stream_data_;
  // If |save_packet_frames_| is true, save generated packets in
  // |saved_frames_|, allowing retransmission packets to be built.
  bool save_packet_frames_ = false;
  std::map<quic::QuicPacketNumber, quic::QuicFrames> saved_frames_;

  // State necessary for building the current packet.
  quic::QuicPacketHeader header_;
  quic::QuicFrames frames_;
  std::unique_ptr<quic::test::SimpleDataProducer> data_producer_;
};

}  // namespace net::test

#endif  // NET_QUIC_QUIC_TEST_PACKET_MAKER_H_
