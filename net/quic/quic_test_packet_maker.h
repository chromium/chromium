// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Provides a simple interface for QUIC tests to create a variety of packets.

#ifndef NET_QUIC_QUIC_TEST_PACKET_MAKER_H_
#define NET_QUIC_QUIC_TEST_PACKET_MAKER_H_

#include <stddef.h>
#include <sys/types.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "net/base/request_priority.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/third_party/quiche/src/quiche/quic/core/http/http_encoder.h"
#include "net/third_party/quiche/src/quiche/quic/core/qpack/qpack_encoder.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_clock.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_stream_frame_data_producer.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_types.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/mock_random.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/qpack/qpack_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/simple_data_producer.h"

namespace net::test {

class QuicTestPacketBuilder;

class QuicTestPacketMaker {
 public:
  // A container for the state of a connection, tracking stream offsets,
  // frames already sent, and so on.
  struct ConnectionState {
    ConnectionState();
    ~ConnectionState();

    // Save the given stream data to last for the duration of this packet build.
    std::string_view SaveStreamData(std::string_view data);

    void Reset();

    // Current offset of each QUIC stream on this connection.
    std::map<quic::QuicStreamId, quic::QuicStreamOffset> stream_offsets;

    // Save a copy of stream frame data that QuicStreamFrame objects can refer
    // to.
    std::vector<std::unique_ptr<std::string>> saved_stream_data;

    // If `save_packet_frames_` is true, save generated packets in
    // `saved_frames_`, allowing retransmission packets to be built.
    bool save_packet_frames = false;
    std::map<quic::QuicPacketNumber, quic::QuicFrames> saved_frames;
  };

  // |client_priority_uses_incremental| affects the output of any method that
  // includes HTTP3 priority data. The protocol default is to omit the
  // incremental flag in the priority data but HTTP streams may enable it
  // if the client supports incremental streams.
  QuicTestPacketMaker(quic::ParsedQuicVersion version,
                      quic::QuicConnectionId connection_id,
                      const quic::QuicClock* clock,
                      const std::string& host,
                      quic::Perspective perspective,
                      bool client_priority_uses_incremental = false,
                      bool use_priority_header = false);
  ~QuicTestPacketMaker();

  QuicTestPacketMaker(const QuicTestPacketMaker&) = delete;
  QuicTestPacketMaker& operator=(const QuicTestPacketMaker&) = delete;

  void set_hostname(const std::string& host);

  void set_use_priority_header(const bool use_priority_header) {
    use_priority_header_ = use_priority_header;
  }

  void set_connection_id(const quic::QuicConnectionId& connection_id) {
    connection_id_ = connection_id;
  }

  // Begin building a packet. Call methods on the returned builder to
  // define the frames in the packet, and finish with its `Build` method.
  QuicTestPacketBuilder& Packet(uint64_t packet_number);

  // Clone all frames from |packet_number|.
  quic::QuicFrames CloneSavedFrames(uint64_t packet_number);

  std::unique_ptr<quic::QuicReceivedPacket> MakeDummyCHLOPacket(
      uint64_t packet_number);

  std::unique_ptr<quic::QuicReceivedPacket> MakeAckAndDatagramPacket(
      uint64_t packet_number,
      uint64_t largest_received,
      uint64_t smallest_received,
      std::string_view data);

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
      quiche::HttpHeaderBlock headers,
      size_t* spdy_headers_frame_length,
      const std::vector<std::string>& data_writes);

  // If |spdy_headers_frame_length| is non-null, it will be set to the size of
  // the SPDY headers frame created for this packet.
  std::unique_ptr<quic::QuicReceivedPacket> MakeRequestHeadersPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool fin,
      spdy::SpdyPriority spdy_priority,
      quiche::HttpHeaderBlock headers,
      size_t* spdy_headers_frame_length,
      bool should_include_priority_frame = true);

  std::unique_ptr<quic::QuicReceivedPacket>
  MakeRetransmissionAndRequestHeadersPacket(
      const std::vector<uint64_t>& original_packet_numbers,
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool fin,
      spdy::SpdyPriority spdy_priority,
      quiche::HttpHeaderBlock headers,
      size_t* spdy_headers_frame_length);

  std::unique_ptr<quic::QuicReceivedPacket> MakeRequestHeadersAndRstPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool fin,
      spdy::SpdyPriority spdy_priority,
      quiche::HttpHeaderBlock headers,
      size_t* spdy_headers_frame_length,
      quic::QuicRstStreamErrorCode error_code);

  // If |spdy_headers_frame_length| is non-null, it will be set to the size of
  // the SPDY headers frame created for this packet.
  std::unique_ptr<quic::QuicReceivedPacket> MakeResponseHeadersPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool fin,
      quiche::HttpHeaderBlock headers,
      size_t* spdy_headers_frame_length);

  // Creates a packet containing the initial SETTINGS frame, and saves the
  // headers stream offset into |offset|.
  std::unique_ptr<quic::QuicReceivedPacket> MakeInitialSettingsPacket(
      uint64_t packet_number);

  std::unique_ptr<quic::QuicReceivedPacket> MakePriorityPacket(
      uint64_t packet_number,
      quic::QuicStreamId id,
      spdy::SpdyPriority spdy_priority);

  std::unique_ptr<quic::QuicReceivedPacket> MakeRetransmissionPacket(
      uint64_t original_packet_number,
      uint64_t new_packet_number);

  std::unique_ptr<quic::QuicReceivedPacket> MakeCombinedRetransmissionPacket(
      const std::vector<uint64_t>& original_packet_numbers,
      uint64_t new_packet_number);

  std::unique_ptr<quic::QuicEncryptedPacket> MakeStatelessResetPacket();

  // Removes all stream frames associated with |stream_id|.
  void RemoveSavedStreamFrames(quic::QuicStreamId stream_id);

  void SetEncryptionLevel(quic::EncryptionLevel level);

  quiche::HttpHeaderBlock GetRequestHeaders(const std::string& method,
                                            const std::string& scheme,
                                            const std::string& path) const;

  quiche::HttpHeaderBlock ConnectRequestHeaders(
      const std::string& host_port) const;

  quiche::HttpHeaderBlock GetResponseHeaders(const std::string& status) const;

  quiche::HttpHeaderBlock GetResponseHeaders(const std::string& status,
                                             const std::string& alt_svc) const;

  // Reset some of the state in the packet maker.
  // TODO(https://issues.chromium.org/u/1/issues/335279177): reset all state.
  void Reset();

  quic::ParsedQuicVersion version() { return version_; }
  quic::Perspective perspective() { return perspective_; }
  const quic::QuicClock* clock() { return clock_.get(); }
  quic::EncryptionLevel encryption_level() { return encryption_level_; }
  quic::QuicLongHeaderType long_header_type() { return long_header_type_; }
  quic::QuicConnectionId connection_id() { return connection_id_; }

  void set_save_packet_frames(bool save_packet_frames) {
    connection_state_.save_packet_frames = save_packet_frames;
  }
  void set_max_plaintext_size(size_t max_plaintext_size) {
    max_plaintext_size_ = max_plaintext_size;
  }

  std::string QpackEncodeHeaders(quic::QuicStreamId stream_id,
                                 quiche::HttpHeaderBlock headers,
                                 size_t* encoded_data_length);

  void set_ecn_codepoint(quic::QuicEcnCodepoint ecn) { ecn_codepoint_ = ecn; }

 protected:
  friend class QuicTestPacketBuilder;
  std::unique_ptr<quic::QuicReceivedPacket> FinishPacket(
      quic::QuicPacketHeader header,
      quic::QuicFrames frames,
      std::unique_ptr<quic::QuicStreamFrameDataProducer> data_producer);

 private:
  void AddPriorityHeader(spdy::SpdyPriority spdy_priority,
                         quiche::HttpHeaderBlock* headers);

  quic::QuicStreamId GetFirstBidirectionalStreamId() const;

  std::string GenerateHttp3PriorityData(spdy::SpdyPriority spdy_priority,
                                        quic::QuicStreamId stream_id);

  // Parameters used throughout the lifetime of the class.
  quic::QuicConnectionId connection_id_;
  quic::ParsedQuicVersion version_;
  raw_ptr<const quic::QuicClock> clock_;  // Not owned.
  std::string host_;
  quic::NoopDecoderStreamErrorDelegate decoder_stream_error_delegate_;
  quic::test::NoopQpackStreamSenderDelegate encoder_stream_sender_delegate_;
  quic::QpackEncoder qpack_encoder_;
  quic::test::MockRandom random_generator_;
  quic::Perspective perspective_;
  quic::EncryptionLevel encryption_level_ = quic::ENCRYPTION_FORWARD_SECURE;
  quic::QuicLongHeaderType long_header_type_ = quic::INVALID_PACKET_TYPE;
  size_t max_plaintext_size_ = quic::kDefaultMaxPacketSize;

  // The value of incremental flag in generated priority headers.
  bool client_priority_uses_incremental_;

  // Add the priority header to outbound requests
  bool use_priority_header_;

  ConnectionState connection_state_;

  // The current packet builder, if one is in progress.
  std::unique_ptr<QuicTestPacketBuilder> builder_;

  // Explicit Congestion Notification (ECN) codepoint to use when making
  // packets.
  quic::QuicEcnCodepoint ecn_codepoint_ = quic::ECN_NOT_ECT;
};

// A packet builder provides methods for building a new packet, usable via
// `QuicTestPacketMaker::Packet`. Finish building the packet with the `Build`
// method.
//
// This implements a "builder" pattern, and method calls can be chained:
//
//   auto packet = maker.Packet(num)
//     .AddFooFrame(..)
//     .AddBarFrame(..)
//     .Build();
//
//  Methods may also be called individually:
//
//  auto& builder = maker.Packet(num);
//  builder.AddFooFrame(..);
//  builder.AddBarFrame(..);
//  auto packet = builder.Build();
class QuicTestPacketBuilder {
 public:
  QuicTestPacketBuilder(uint64_t packet_number,
                        QuicTestPacketMaker* maker,
                        QuicTestPacketMaker::ConnectionState* connection_state);
  ~QuicTestPacketBuilder();

  QuicTestPacketBuilder& AddPaddingFrame(size_t padding_size = 0);
  QuicTestPacketBuilder& AddPingFrame();
  QuicTestPacketBuilder& AddRetireConnectionIdFrame(uint64_t sequence_number);
  QuicTestPacketBuilder& AddNewConnectionIdFrame(
      const quic::QuicConnectionId& cid,
      uint64_t sequence_number,
      uint64_t retire_prior_to);
  QuicTestPacketBuilder& AddMaxStreamsFrame(
      quic::QuicControlFrameId control_frame_id,
      quic::QuicStreamCount stream_count,
      bool unidirectional);
  QuicTestPacketBuilder& AddStreamsBlockedFrame(
      quic::QuicControlFrameId control_frame_id,
      quic::QuicStreamCount stream_count,
      bool unidirectional);
  // Add a stream frame, using an offset calculated from any previous stream
  // frames with this stream_id. The given data is copied and may be deallocated
  // after this call.
  QuicTestPacketBuilder& AddStreamFrame(quic::QuicStreamId stream_id,
                                        bool fin,
                                        std::string_view data);
  QuicTestPacketBuilder& AddAckFrame(
      uint64_t first_received,
      uint64_t largest_received,
      uint64_t smallest_received,
      std::optional<quic::QuicEcnCounts> ecn = std::nullopt);
  // Add a DATAGRAM frame. The given data is copied and may be deallocated after
  // this call.
  QuicTestPacketBuilder& AddMessageFrame(std::string_view data);
  QuicTestPacketBuilder& AddRstStreamFrame(
      quic::QuicStreamId stream_id,
      quic::QuicRstStreamErrorCode error_code);
  QuicTestPacketBuilder& AddConnectionCloseFrame(
      quic::QuicErrorCode quic_error,
      const std::string& quic_error_details,
      uint64_t frame_type = 0);
  QuicTestPacketBuilder& AddGoAwayFrame(quic::QuicErrorCode error_code,
                                        std::string reason_phrase);
  QuicTestPacketBuilder& AddPathResponseFrame();
  QuicTestPacketBuilder& AddPathChallengeFrame();
  QuicTestPacketBuilder& AddStopSendingFrame(
      quic::QuicStreamId stream_id,
      quic::QuicRstStreamErrorCode error_code);
  QuicTestPacketBuilder& AddCryptoFrame(quic::EncryptionLevel level,
                                        quic::QuicStreamOffset offset,
                                        const quic::QuicData& data);

  // Add a frame to the packet.
  QuicTestPacketBuilder& AddFrame(quic::QuicFrame frame);

  // Add a frame to the packet, coalescing adjacent stream frames for the same
  // stream ID into a single frame.
  QuicTestPacketBuilder& AddFrameWithCoalescing(const quic::QuicFrame& frame);

  // Retransmit the frames in a previously-transmitted packet, optionally
  // filtering frames with the given callback.
  QuicTestPacketBuilder& AddPacketRetransmission(
      uint64_t packet_number,
      base::RepeatingCallback<bool(const quic::QuicFrame&)> filter =
          base::RepeatingCallback<bool(const quic::QuicFrame&)>());

  // Complete the building process and return the resulting packet.
  std::unique_ptr<quic::QuicReceivedPacket> Build();

  // Add an H/3 SETTINGS frame on the control stream if one has not already been
  // sent.
  QuicTestPacketBuilder& MaybeAddHttp3SettingsFrames();

 private:
  std::string GenerateHttp3SettingsData() const;
  std::string GenerateHttp3GreaseData() const;

  void InitializeHeader(uint64_t packet_number);

  quic::QuicConnectionId DestinationConnectionId() const;
  quic::QuicConnectionId SourceConnectionId() const;

  bool ShouldIncludeVersion() const;

  // State necessary for building the current packet.
  quic::QuicPacketHeader header_;
  quic::QuicFrames frames_;
  std::unique_ptr<quic::test::SimpleDataProducer> data_producer_;

  // The `QuicTestPacketMaker` for which we are building this packet.Reset some
  // of the state in the packet maker.
  // TODO(https://issues.chromium.org/u/1/issues/335279177): reset all state.
  raw_ptr<QuicTestPacketMaker> maker_;

  // The connection state. This is owned by `maker_` but borrowed for
  // the lifetime of this builder.
  raw_ptr<QuicTestPacketMaker::ConnectionState> connection_state_;
};

}  // namespace net::test

#endif  // NET_QUIC_QUIC_TEST_PACKET_MAKER_H_
