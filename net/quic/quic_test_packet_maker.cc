// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_test_packet_maker.h"

#include <list>
#include <utility>

#include "net/quic/mock_crypto_client_stream.h"
#include "net/quic/quic_http_utils.h"
#include "net/third_party/quic/core/quic_framer.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/test_tools/mock_random.h"
#include "net/third_party/quic/test_tools/quic_test_utils.h"

namespace net {
namespace test {
namespace {

quic::QuicAckFrame MakeAckFrame(quic::QuicPacketNumber largest_observed) {
  quic::QuicAckFrame ack;
  ack.largest_acked = largest_observed;
  return ack;
}

}  // namespace

QuicTestPacketMaker::QuicTestPacketMaker(
    quic::QuicTransportVersion version,
    quic::QuicConnectionId connection_id,
    quic::MockClock* clock,
    const std::string& host,
    quic::Perspective perspective,
    bool client_headers_include_h2_stream_dependency)
    : version_(version),
      connection_id_(connection_id),
      clock_(clock),
      host_(host),
      spdy_request_framer_(spdy::SpdyFramer::ENABLE_COMPRESSION),
      spdy_response_framer_(spdy::SpdyFramer::ENABLE_COMPRESSION),
      perspective_(perspective),
      encryption_level_(quic::ENCRYPTION_FORWARD_SECURE),
      long_header_type_(quic::HANDSHAKE),
      client_headers_include_h2_stream_dependency_(
          client_headers_include_h2_stream_dependency &&
          version >= quic::QUIC_VERSION_43) {
  DCHECK(!(perspective_ == quic::Perspective::IS_SERVER &&
           client_headers_include_h2_stream_dependency_));
}

QuicTestPacketMaker::~QuicTestPacketMaker() {}

void QuicTestPacketMaker::set_hostname(const std::string& host) {
  host_.assign(host);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeConnectivityProbingPacket(quic::QuicPacketNumber num,
                                                   bool include_version) {
  quic::QuicPacketHeader header;
  header.destination_connection_id = connection_id_;
  header.destination_connection_id_length = GetDestinationConnectionIdLength();
  header.source_connection_id = connection_id_;
  header.source_connection_id_length = GetSourceConnectionIdLength();
  header.reset_flag = false;
  header.version_flag = ShouldIncludeVersion(include_version);
  header.long_packet_type = long_header_type_;
  header.packet_number_length = GetPacketNumberLength();
  header.packet_number = num;

  quic::QuicFramer framer(quic::test::SupportedVersions(quic::ParsedQuicVersion(
                              quic::PROTOCOL_QUIC_CRYPTO, version_)),
                          clock_->Now(), perspective_);
  size_t max_plaintext_size =
      framer.GetMaxPlaintextSize(quic::kDefaultMaxPacketSize);
  char buffer[quic::kDefaultMaxPacketSize];
  size_t length;
  if (version_ != quic::QUIC_VERSION_99) {
    length = framer.BuildConnectivityProbingPacket(header, buffer,
                                                   max_plaintext_size);
  } else if (perspective_ == quic::Perspective::IS_CLIENT) {
    quic::test::MockRandom rand(0);
    quic::QuicPathFrameBuffer payload;
    length = framer.BuildPaddedPathChallengePacket(
        header, buffer, max_plaintext_size, &payload, &rand);
  } else {
    quic::test::MockRandom rand(0);
    quic::QuicPathFrameBuffer payload;
    rand.RandBytes(payload.data(), payload.size());
    quic::QuicDeque<quic::QuicPathFrameBuffer> payloads{payload};
    length = framer.BuildPathResponsePacket(header, buffer, max_plaintext_size,
                                            payloads, true);
  }
  size_t encrypted_size = framer.EncryptInPlace(
      quic::ENCRYPTION_NONE, header.packet_number,
      GetStartOfEncryptedData(framer.transport_version(), header), length,
      quic::kDefaultMaxPacketSize, buffer);
  EXPECT_EQ(quic::kDefaultMaxPacketSize, encrypted_size);
  quic::QuicReceivedPacket encrypted(buffer, encrypted_size, clock_->Now(),
                                     false);
  return encrypted.Clone();
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::MakePingPacket(
    quic::QuicPacketNumber num,
    bool include_version) {
  quic::QuicPacketHeader header;
  header.destination_connection_id = connection_id_;
  header.destination_connection_id_length = GetDestinationConnectionIdLength();
  header.source_connection_id = connection_id_;
  header.source_connection_id_length = GetSourceConnectionIdLength();
  header.reset_flag = false;
  header.version_flag = ShouldIncludeVersion(include_version);
  header.long_packet_type = long_header_type_;
  header.packet_number_length = GetPacketNumberLength();
  header.packet_number = num;

  quic::QuicPingFrame ping;
  return MakePacket(header, quic::QuicFrame(ping));
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeDummyCHLOPacket(quic::QuicPacketNumber packet_num) {
  encryption_level_ = quic::ENCRYPTION_NONE;
  SetLongHeaderType(quic::INITIAL);
  InitializeHeader(packet_num, /*include_version=*/true);

  quic::CryptoHandshakeMessage message =
      MockCryptoClientStream::GetDummyCHLOMessage();
  const quic::QuicData& data = message.GetSerialized();

  quic::QuicFrames frames;
  quic::QuicStreamFrame frame(
      quic::QuicUtils::GetCryptoStreamId(version_), /*fin=*/false, /*offset=*/0,
      quic::QuicStringPiece(data.data(), data.length()));
  frames.push_back(quic::QuicFrame(frame));
  DVLOG(1) << "Adding frame: " << frames.back();
  quic::QuicPaddingFrame padding;
  frames.push_back(quic::QuicFrame(padding));
  DVLOG(1) << "Adding frame: " << frames.back();

  return MakeMultipleFramesPacket(header_, frames);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeAckAndPingPacket(
    quic::QuicPacketNumber num,
    bool include_version,
    quic::QuicPacketNumber largest_received,
    quic::QuicPacketNumber smallest_received,
    quic::QuicPacketNumber least_unacked) {
  quic::QuicPacketHeader header;
  header.destination_connection_id = connection_id_;
  header.destination_connection_id_length = GetDestinationConnectionIdLength();
  header.source_connection_id = connection_id_;
  header.source_connection_id_length = GetSourceConnectionIdLength();
  header.reset_flag = false;
  header.version_flag = ShouldIncludeVersion(include_version);
  header.long_packet_type = long_header_type_;
  header.packet_number_length = GetPacketNumberLength();
  header.packet_number = num;

  quic::QuicAckFrame ack(MakeAckFrame(largest_received));
  ack.ack_delay_time = quic::QuicTime::Delta::Zero();
  for (quic::QuicPacketNumber i = smallest_received; i <= largest_received;
       ++i) {
    ack.received_packet_times.push_back(std::make_pair(i, clock_->Now()));
  }
  if (largest_received > 0) {
    ack.packets.AddRange(1, largest_received + 1);
  }
  quic::QuicFrames frames;
  frames.push_back(quic::QuicFrame(&ack));
  DVLOG(1) << "Adding frame: " << frames.back();

  quic::QuicStopWaitingFrame stop_waiting;
  if (version_ == quic::QUIC_VERSION_35) {
    stop_waiting.least_unacked = least_unacked;
    frames.push_back(quic::QuicFrame(&stop_waiting));
    DVLOG(1) << "Adding frame: " << frames.back();
  }

  frames.push_back(quic::QuicFrame(quic::QuicPingFrame()));
  DVLOG(1) << "Adding frame: " << frames.back();

  return MakeMultipleFramesPacket(header, frames);
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::MakeRstPacket(
    quic::QuicPacketNumber num,
    bool include_version,
    quic::QuicStreamId stream_id,
    quic::QuicRstStreamErrorCode error_code) {
  return MakeRstPacket(num, include_version, stream_id, error_code, 0);
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::MakeRstPacket(
    quic::QuicPacketNumber num,
    bool include_version,
    quic::QuicStreamId stream_id,
    quic::QuicRstStreamErrorCode error_code,
    size_t bytes_written) {
  quic::QuicPacketHeader header;
  header.destination_connection_id = connection_id_;
  header.destination_connection_id_length = GetDestinationConnectionIdLength();
  header.source_connection_id = connection_id_;
  header.source_connection_id_length = GetSourceConnectionIdLength();
  header.reset_flag = false;
  header.version_flag = ShouldIncludeVersion(include_version);
  header.long_packet_type = long_header_type_;
  header.packet_number_length = GetPacketNumberLength();
  header.packet_number = num;

  quic::QuicRstStreamFrame rst(1, stream_id, error_code, bytes_written);
  DVLOG(1) << "Adding frame: " << quic::QuicFrame(&rst);
  return MakePacket(header, quic::QuicFrame(&rst));
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRstAndRequestHeadersPacket(
    quic::QuicPacketNumber num,
    bool include_version,
    quic::QuicStreamId rst_stream_id,
    quic::QuicRstStreamErrorCode rst_error_code,
    quic::QuicStreamId stream_id,
    bool fin,
    spdy::SpdyPriority priority,
    spdy::SpdyHeaderBlock headers,
    quic::QuicStreamId parent_stream_id,
    size_t* spdy_headers_frame_length,
    quic::QuicStreamOffset* offset) {
  quic::QuicRstStreamFrame rst_frame(1, rst_stream_id, rst_error_code, 0);

  spdy::SpdySerializedFrame spdy_frame = MakeSpdyHeadersFrame(
      stream_id, fin, priority, std::move(headers), parent_stream_id);
  if (spdy_headers_frame_length) {
    *spdy_headers_frame_length = spdy_frame.size();
  }
  quic::QuicStreamOffset header_offset = 0;
  if (offset != nullptr) {
    header_offset = *offset;
    *offset += spdy_frame.size();
  }
  quic::QuicStreamFrame headers_frame(
      quic::QuicUtils::GetHeadersStreamId(version_), false, header_offset,
      quic::QuicStringPiece(spdy_frame.data(), spdy_frame.size()));

  quic::QuicFrames frames;
  frames.push_back(quic::QuicFrame(&rst_frame));
  DVLOG(1) << "Adding frame: " << frames.back();
  frames.push_back(quic::QuicFrame(headers_frame));
  DVLOG(1) << "Adding frame: " << frames.back();

  InitializeHeader(num, include_version);
  return MakeMultipleFramesPacket(header_, frames);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeAckAndRstPacket(
    quic::QuicPacketNumber num,
    bool include_version,
    quic::QuicStreamId stream_id,
    quic::QuicRstStreamErrorCode error_code,
    quic::QuicPacketNumber largest_received,
    quic::QuicPacketNumber smallest_received,
    quic::QuicPacketNumber least_unacked,
    bool send_feedback) {
  return MakeAckAndRstPacket(num, include_version, stream_id, error_code,
                             largest_received, smallest_received, least_unacked,
                             send_feedback, 0);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeAckAndRstPacket(
    quic::QuicPacketNumber num,
    bool include_version,
    quic::QuicStreamId stream_id,
    quic::QuicRstStreamErrorCode error_code,
    quic::QuicPacketNumber largest_received,
    quic::QuicPacketNumber smallest_received,
    quic::QuicPacketNumber least_unacked,
    bool send_feedback,
    size_t bytes_written) {
  quic::QuicPacketHeader header;
  header.destination_connection_id = connection_id_;
  header.destination_connection_id_length = GetDestinationConnectionIdLength();
  header.source_connection_id = connection_id_;
  header.source_connection_id_length = GetSourceConnectionIdLength();
  header.reset_flag = false;
  header.version_flag = ShouldIncludeVersion(include_version);
  header.long_packet_type = long_header_type_;
  header.packet_number_length = GetPacketNumberLength();
  header.packet_number = num;

  quic::QuicAckFrame ack(MakeAckFrame(largest_received));
  ack.ack_delay_time = quic::QuicTime::Delta::Zero();
  for (quic::QuicPacketNumber i = smallest_received; i <= largest_received;
       ++i) {
    ack.received_packet_times.push_back(std::make_pair(i, clock_->Now()));
  }
  if (largest_received > 0) {
    ack.packets.AddRange(1, largest_received + 1);
  }
  quic::QuicFrames frames;
  frames.push_back(quic::QuicFrame(&ack));
  DVLOG(1) << "Adding frame: " << frames.back();

  quic::QuicStopWaitingFrame stop_waiting;
  if (version_ == quic::QUIC_VERSION_35) {
    stop_waiting.least_unacked = least_unacked;
    frames.push_back(quic::QuicFrame(&stop_waiting));
    DVLOG(1) << "Adding frame: " << frames.back();
  }

  quic::QuicRstStreamFrame rst(1, stream_id, error_code, bytes_written);
  frames.push_back(quic::QuicFrame(&rst));
  DVLOG(1) << "Adding frame: " << frames.back();

  return MakeMultipleFramesPacket(header, frames);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRstAckAndConnectionClosePacket(
    quic::QuicPacketNumber num,
    bool include_version,
    quic::QuicStreamId stream_id,
    quic::QuicRstStreamErrorCode error_code,
    quic::QuicTime::Delta ack_delay_time,
    quic::QuicPacketNumber largest_received,
    quic::QuicPacketNumber smallest_received,
    quic::QuicPacketNumber least_unacked,
    quic::QuicErrorCode quic_error,
    const std::string& quic_error_details) {
  quic::QuicPacketHeader header;
  header.destination_connection_id = connection_id_;
  header.destination_connection_id_length = GetDestinationConnectionIdLength();
  header.source_connection_id = connection_id_;
  header.source_connection_id_length = GetSourceConnectionIdLength();
  header.reset_flag = false;
  header.version_flag = ShouldIncludeVersion(include_version);
  header.long_packet_type = long_header_type_;
  header.packet_number_length = GetPacketNumberLength();
  header.packet_number = num;

  quic::QuicFrames frames;
  quic::QuicRstStreamFrame rst(1, stream_id, error_code, 0);
  frames.push_back(quic::QuicFrame(&rst));
  DVLOG(1) << "Adding frame: " << frames.back();

  quic::QuicAckFrame ack(MakeAckFrame(largest_received));
  ack.ack_delay_time = ack_delay_time;
  for (quic::QuicPacketNumber i = smallest_received; i <= largest_received;
       ++i) {
    ack.received_packet_times.push_back(std::make_pair(i, clock_->Now()));
  }
  if (largest_received > 0) {
    ack.packets.AddRange(1, largest_received + 1);
  }
  frames.push_back(quic::QuicFrame(&ack));
  DVLOG(1) << "Adding frame: " << frames.back();

  quic::QuicStopWaitingFrame stop_waiting;
  if (version_ == quic::QUIC_VERSION_35) {
    stop_waiting.least_unacked = least_unacked;
    frames.push_back(quic::QuicFrame(&stop_waiting));
    DVLOG(1) << "Adding frame: " << frames.back();
  }

  quic::QuicConnectionCloseFrame close;
  close.error_code = quic_error;
  close.error_details = quic_error_details;

  frames.push_back(quic::QuicFrame(&close));
  DVLOG(1) << "Adding frame: " << frames.back();

  return MakeMultipleFramesPacket(header, frames);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeAckAndConnectionClosePacket(
    quic::QuicPacketNumber num,
    bool include_version,
    quic::QuicTime::Delta ack_delay_time,
    quic::QuicPacketNumber largest_received,
    quic::QuicPacketNumber smallest_received,
    quic::QuicPacketNumber least_unacked,
    quic::QuicErrorCode quic_error,
    const std::string& quic_error_details) {
  quic::QuicPacketHeader header;
  header.destination_connection_id = connection_id_;
  header.destination_connection_id_length = GetDestinationConnectionIdLength();
  header.source_connection_id = connection_id_;
  header.source_connection_id_length = GetSourceConnectionIdLength();
  header.reset_flag = false;
  header.version_flag = ShouldIncludeVersion(include_version);
  header.long_packet_type = long_header_type_;
  header.packet_number_length = GetPacketNumberLength();
  header.packet_number = num;

  quic::QuicAckFrame ack(MakeAckFrame(largest_received));
  ack.ack_delay_time = ack_delay_time;
  for (quic::QuicPacketNumber i = smallest_received; i <= largest_received;
       ++i) {
    ack.received_packet_times.push_back(std::make_pair(i, clock_->Now()));
  }
  if (largest_received > 0) {
    ack.packets.AddRange(1, largest_received + 1);
  }
  quic::QuicFrames frames;
  frames.push_back(quic::QuicFrame(&ack));
  DVLOG(1) << "Adding frame: " << frames.back();

  quic::QuicStopWaitingFrame stop_waiting;
  if (version_ == quic::QUIC_VERSION_35) {
    stop_waiting.least_unacked = least_unacked;
    frames.push_back(quic::QuicFrame(&stop_waiting));
    DVLOG(1) << "Adding frame: " << frames.back();
  }

  quic::QuicConnectionCloseFrame close;
  close.error_code = quic_error;
  close.error_details = quic_error_details;

  frames.push_back(quic::QuicFrame(&close));
  DVLOG(1) << "Adding frame: " << frames.back();

  return MakeMultipleFramesPacket(header, frames);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeConnectionClosePacket(
    quic::QuicPacketNumber num,
    bool include_version,
    quic::QuicErrorCode quic_error,
    const std::string& quic_error_details) {
  quic::QuicPacketHeader header;
  header.destination_connection_id = connection_id_;
  header.destination_connection_id_length = GetDestinationConnectionIdLength();
  header.source_connection_id = connection_id_;
  header.source_connection_id_length = GetSourceConnectionIdLength();
  header.reset_flag = false;
  header.version_flag = ShouldIncludeVersion(include_version);
  header.long_packet_type = long_header_type_;
  header.packet_number_length = GetPacketNumberLength();
  header.packet_number = num;

  quic::QuicConnectionCloseFrame close;
  close.error_code = quic_error;
  close.error_details = quic_error_details;
  return MakePacket(header, quic::QuicFrame(&close));
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::MakeGoAwayPacket(
    quic::QuicPacketNumber num,
    quic::QuicErrorCode error_code,
    std::string reason_phrase) {
  quic::QuicPacketHeader header;
  header.destination_connection_id = connection_id_;
  header.destination_connection_id_length = GetDestinationConnectionIdLength();
  header.source_connection_id = connection_id_;
  header.source_connection_id_length = GetSourceConnectionIdLength();
  header.reset_flag = false;
  header.version_flag = ShouldIncludeVersion(false);
  header.long_packet_type = long_header_type_;
  header.packet_number_length = GetPacketNumberLength();
  header.packet_number = num;

  quic::QuicGoAwayFrame goaway;
  goaway.error_code = error_code;
  goaway.last_good_stream_id = 0;
  goaway.reason_phrase = reason_phrase;
  return MakePacket(header, quic::QuicFrame(&goaway));
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::MakeAckPacket(
    quic::QuicPacketNumber packet_number,
    quic::QuicPacketNumber largest_received,
    quic::QuicPacketNumber smallest_received,
    quic::QuicPacketNumber least_unacked,
    bool send_feedback) {
  return MakeAckPacket(packet_number, 1, largest_received, smallest_received,
                       least_unacked, send_feedback,
                       quic::QuicTime::Delta::Zero());
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::MakeAckPacket(
    quic::QuicPacketNumber packet_number,
    quic::QuicPacketNumber first_received,
    quic::QuicPacketNumber largest_received,
    quic::QuicPacketNumber smallest_received,
    quic::QuicPacketNumber least_unacked,
    bool send_feedback) {
  return MakeAckPacket(packet_number, first_received, largest_received,
                       smallest_received, least_unacked, send_feedback,
                       quic::QuicTime::Delta::Zero());
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::MakeAckPacket(
    quic::QuicPacketNumber packet_number,
    quic::QuicPacketNumber largest_received,
    quic::QuicPacketNumber smallest_received,
    quic::QuicPacketNumber least_unacked,
    bool send_feedback,
    quic::QuicTime::Delta ack_delay_time) {
  return MakeAckPacket(packet_number, 1, largest_received, smallest_received,
                       least_unacked, send_feedback, ack_delay_time);
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::MakeAckPacket(
    quic::QuicPacketNumber packet_number,
    quic::QuicPacketNumber first_received,
    quic::QuicPacketNumber largest_received,
    quic::QuicPacketNumber smallest_received,
    quic::QuicPacketNumber least_unacked,
    bool send_feedback,
    quic::QuicTime::Delta ack_delay_time) {
  quic::QuicPacketHeader header;
  header.destination_connection_id = connection_id_;
  header.destination_connection_id_length = GetDestinationConnectionIdLength();
  header.source_connection_id = connection_id_;
  header.source_connection_id_length = GetSourceConnectionIdLength();
  header.reset_flag = false;
  header.version_flag = ShouldIncludeVersion(false);
  header.long_packet_type = long_header_type_;
  header.packet_number_length = GetPacketNumberLength();
  header.packet_number = packet_number;

  quic::QuicAckFrame ack(MakeAckFrame(largest_received));
  ack.ack_delay_time = ack_delay_time;
  for (quic::QuicPacketNumber i = smallest_received; i <= largest_received;
       ++i) {
    ack.received_packet_times.push_back(std::make_pair(i, clock_->Now()));
  }
  if (largest_received > 0) {
    DCHECK_GE(largest_received, first_received);
    ack.packets.AddRange(first_received, largest_received + 1);
  }
  quic::QuicFramer framer(quic::test::SupportedVersions(quic::ParsedQuicVersion(
                              quic::PROTOCOL_QUIC_CRYPTO, version_)),
                          clock_->Now(), perspective_);
  quic::QuicFrames frames;
  quic::QuicFrame ack_frame(&ack);
  frames.push_back(ack_frame);
  DVLOG(1) << "Adding frame: " << frames.back();

  quic::QuicStopWaitingFrame stop_waiting;
  if (version_ == quic::QUIC_VERSION_35) {
    stop_waiting.least_unacked = least_unacked;
    frames.push_back(quic::QuicFrame(&stop_waiting));
    DVLOG(1) << "Adding frame: " << frames.back();
  }

  std::unique_ptr<quic::QuicPacket> packet(
      quic::test::BuildUnsizedDataPacket(&framer, header, frames));
  char buffer[quic::kMaxPacketSize];
  size_t encrypted_size =
      framer.EncryptPayload(quic::ENCRYPTION_NONE, header.packet_number,
                            *packet, buffer, quic::kMaxPacketSize);
  EXPECT_NE(0u, encrypted_size);
  quic::QuicReceivedPacket encrypted(buffer, encrypted_size, clock_->Now(),
                                     false);
  return encrypted.Clone();
}

// Returns a newly created packet to send kData on stream 1.
std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::MakeDataPacket(
    quic::QuicPacketNumber packet_number,
    quic::QuicStreamId stream_id,
    bool should_include_version,
    bool fin,
    quic::QuicStreamOffset offset,
    quic::QuicStringPiece data) {
  InitializeHeader(packet_number, should_include_version);
  quic::QuicStreamFrame frame(stream_id, fin, offset, data);
  DVLOG(1) << "Adding frame: " << frame;
  return MakePacket(header_, quic::QuicFrame(frame));
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeMultipleDataFramesPacket(
    quic::QuicPacketNumber packet_number,
    quic::QuicStreamId stream_id,
    bool should_include_version,
    bool fin,
    quic::QuicStreamOffset offset,
    const std::vector<std::string>& data_writes) {
  InitializeHeader(packet_number, should_include_version);
  quic::QuicFrames data_frames;
  for (size_t i = 0; i < data_writes.size(); ++i) {
    bool is_fin = fin && (i == data_writes.size() - 1);
    quic::QuicFrame quic_frame(quic::QuicStreamFrame(
        stream_id, is_fin, offset, quic::QuicStringPiece(data_writes[i])));
    DVLOG(1) << "Adding frame: " << quic_frame;
    data_frames.push_back(quic_frame);
    offset += data_writes[i].length();
  }
  return MakeMultipleFramesPacket(header_, data_frames);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeAckAndDataPacket(
    quic::QuicPacketNumber packet_number,
    bool include_version,
    quic::QuicStreamId stream_id,
    quic::QuicPacketNumber largest_received,
    quic::QuicPacketNumber smallest_received,
    quic::QuicPacketNumber least_unacked,
    bool fin,
    quic::QuicStreamOffset offset,
    quic::QuicStringPiece data) {
  InitializeHeader(packet_number, include_version);

  quic::QuicAckFrame ack(MakeAckFrame(largest_received));
  ack.ack_delay_time = quic::QuicTime::Delta::Zero();
  for (quic::QuicPacketNumber i = smallest_received; i <= largest_received;
       ++i) {
    ack.received_packet_times.push_back(std::make_pair(i, clock_->Now()));
  }
  if (largest_received > 0) {
    ack.packets.AddRange(1, largest_received + 1);
  }
  quic::QuicFrames frames;
  frames.push_back(quic::QuicFrame(&ack));
  DVLOG(1) << "Adding frame: " << frames.back();

  quic::QuicStopWaitingFrame stop_waiting;
  if (version_ == quic::QUIC_VERSION_35) {
    stop_waiting.least_unacked = least_unacked;
    frames.push_back(quic::QuicFrame(&stop_waiting));
    DVLOG(1) << "Adding frame: " << frames.back();
  }

  frames.push_back(
      quic::QuicFrame(quic::QuicStreamFrame(stream_id, fin, offset, data)));

  return MakeMultipleFramesPacket(header_, frames);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRequestHeadersAndMultipleDataFramesPacket(
    quic::QuicPacketNumber packet_number,
    quic::QuicStreamId stream_id,
    bool should_include_version,
    bool fin,
    spdy::SpdyPriority priority,
    spdy::SpdyHeaderBlock headers,
    quic::QuicStreamId parent_stream_id,
    quic::QuicStreamOffset* header_stream_offset,
    size_t* spdy_headers_frame_length,
    const std::vector<std::string>& data_writes) {
  InitializeHeader(packet_number, should_include_version);
  spdy::SpdySerializedFrame spdy_frame =
      MakeSpdyHeadersFrame(stream_id, fin && data_writes.empty(), priority,
                           std::move(headers), parent_stream_id);

  if (spdy_headers_frame_length) {
    *spdy_headers_frame_length = spdy_frame.size();
  }
  quic::QuicFrames frames;
  quic::QuicStreamOffset header_offset =
      header_stream_offset == nullptr ? 0 : *header_stream_offset;
  quic::QuicStreamFrame frame(
      quic::QuicUtils::GetHeadersStreamId(version_), false, header_offset,
      quic::QuicStringPiece(spdy_frame.data(), spdy_frame.size()));
  frames.push_back(quic::QuicFrame(frame));
  DVLOG(1) << "Adding frame: " << frames.back();
  if (header_stream_offset != nullptr) {
    *header_stream_offset += spdy_frame.size();
  }

  quic::QuicStreamOffset offset = 0;
  // quic::QuicFrame takes a raw pointer. Use a std::vector here so we keep
  // StreamFrames alive until MakeMultipleFramesPacket is done.
  std::vector<std::unique_ptr<quic::QuicStreamFrame>> stream_frames;
  for (size_t i = 0; i < data_writes.size(); ++i) {
    bool is_fin = fin && (i == data_writes.size() - 1);
    quic::QuicFrame quic_frame(quic::QuicStreamFrame(
        stream_id, is_fin, offset, quic::QuicStringPiece(data_writes[i])));
    DVLOG(1) << "Adding frame: " << quic_frame;
    frames.push_back(quic_frame);
    offset += data_writes[i].length();
  }
  return MakeMultipleFramesPacket(header_, frames);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRequestHeadersPacket(
    quic::QuicPacketNumber packet_number,
    quic::QuicStreamId stream_id,
    bool should_include_version,
    bool fin,
    spdy::SpdyPriority priority,
    spdy::SpdyHeaderBlock headers,
    quic::QuicStreamId parent_stream_id,
    size_t* spdy_headers_frame_length) {
  return MakeRequestHeadersPacket(
      packet_number, stream_id, should_include_version, fin, priority,
      std::move(headers), parent_stream_id, spdy_headers_frame_length, nullptr);
}

// If |offset| is provided, will use the value when creating the packet.
// Will also update the value after packet creation.
std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRequestHeadersPacket(
    quic::QuicPacketNumber packet_number,
    quic::QuicStreamId stream_id,
    bool should_include_version,
    bool fin,
    spdy::SpdyPriority priority,
    spdy::SpdyHeaderBlock headers,
    quic::QuicStreamId parent_stream_id,
    size_t* spdy_headers_frame_length,
    quic::QuicStreamOffset* offset) {
  std::string unused_stream_data;
  return MakeRequestHeadersPacketAndSaveData(
      packet_number, stream_id, should_include_version, fin, priority,
      std::move(headers), parent_stream_id, spdy_headers_frame_length, offset,
      &unused_stream_data);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRequestHeadersPacketAndSaveData(
    quic::QuicPacketNumber packet_number,
    quic::QuicStreamId stream_id,
    bool should_include_version,
    bool fin,
    spdy::SpdyPriority priority,
    spdy::SpdyHeaderBlock headers,
    quic::QuicStreamId parent_stream_id,
    size_t* spdy_headers_frame_length,
    quic::QuicStreamOffset* offset,
    std::string* stream_data) {
  InitializeHeader(packet_number, should_include_version);
  spdy::SpdySerializedFrame spdy_frame = MakeSpdyHeadersFrame(
      stream_id, fin, priority, std::move(headers), parent_stream_id);
  *stream_data = std::string(spdy_frame.data(), spdy_frame.size());

  if (spdy_headers_frame_length)
    *spdy_headers_frame_length = spdy_frame.size();

  if (offset != nullptr) {
    quic::QuicStreamFrame frame(
        quic::QuicUtils::GetHeadersStreamId(version_), false, *offset,
        quic::QuicStringPiece(spdy_frame.data(), spdy_frame.size()));
    *offset += spdy_frame.size();
    return MakePacket(header_, quic::QuicFrame(frame));
  } else {
    quic::QuicStreamFrame frame(
        quic::QuicUtils::GetHeadersStreamId(version_), false, 0,
        quic::QuicStringPiece(spdy_frame.data(), spdy_frame.size()));

    return MakePacket(header_, quic::QuicFrame(frame));
  }
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRequestHeadersAndRstPacket(
    quic::QuicPacketNumber packet_number,
    quic::QuicStreamId stream_id,
    bool should_include_version,
    bool fin,
    spdy::SpdyPriority priority,
    spdy::SpdyHeaderBlock headers,
    quic::QuicStreamId parent_stream_id,
    size_t* spdy_headers_frame_length,
    quic::QuicStreamOffset* header_stream_offset,
    quic::QuicRstStreamErrorCode error_code,
    size_t bytes_written) {
  spdy::SpdySerializedFrame spdy_frame = MakeSpdyHeadersFrame(
      stream_id, fin, priority, std::move(headers), parent_stream_id);
  if (spdy_headers_frame_length) {
    *spdy_headers_frame_length = spdy_frame.size();
  }
  quic::QuicStreamOffset header_offset = 0;
  if (header_stream_offset != nullptr) {
    header_offset = *header_stream_offset;
    *header_stream_offset += spdy_frame.size();
  }
  quic::QuicStreamFrame headers_frame(
      quic::QuicUtils::GetHeadersStreamId(version_), false, header_offset,
      quic::QuicStringPiece(spdy_frame.data(), spdy_frame.size()));

  quic::QuicRstStreamFrame rst_frame(1, stream_id, error_code, bytes_written);

  quic::QuicFrames frames;
  frames.push_back(quic::QuicFrame(headers_frame));
  DVLOG(1) << "Adding frame: " << frames.back();
  frames.push_back(quic::QuicFrame(&rst_frame));
  DVLOG(1) << "Adding frame: " << frames.back();

  InitializeHeader(packet_number, should_include_version);
  return MakeMultipleFramesPacket(header_, frames);
}

spdy::SpdySerializedFrame QuicTestPacketMaker::MakeSpdyHeadersFrame(
    quic::QuicStreamId stream_id,
    bool fin,
    spdy::SpdyPriority priority,
    spdy::SpdyHeaderBlock headers,
    quic::QuicStreamId parent_stream_id) {
  spdy::SpdyHeadersIR headers_frame(stream_id, std::move(headers));
  headers_frame.set_fin(fin);
  headers_frame.set_weight(spdy::Spdy3PriorityToHttp2Weight(priority));
  headers_frame.set_has_priority(true);

  if (client_headers_include_h2_stream_dependency_) {
    headers_frame.set_parent_stream_id(parent_stream_id);
    headers_frame.set_exclusive(true);
  } else {
    headers_frame.set_parent_stream_id(0);
    headers_frame.set_exclusive(false);
  }

  return spdy_request_framer_.SerializeFrame(headers_frame);
}

// Convenience method for calling MakeRequestHeadersPacket with nullptr for
// |spdy_headers_frame_length|.
std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRequestHeadersPacketWithOffsetTracking(
    quic::QuicPacketNumber packet_number,
    quic::QuicStreamId stream_id,
    bool should_include_version,
    bool fin,
    spdy::SpdyPriority priority,
    spdy::SpdyHeaderBlock headers,
    quic::QuicStreamId parent_stream_id,
    quic::QuicStreamOffset* offset) {
  return MakeRequestHeadersPacket(
      packet_number, stream_id, should_include_version, fin, priority,
      std::move(headers), parent_stream_id, nullptr, offset);
}

// If |offset| is provided, will use the value when creating the packet.
// Will also update the value after packet creation.
std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakePushPromisePacket(
    quic::QuicPacketNumber packet_number,
    quic::QuicStreamId stream_id,
    quic::QuicStreamId promised_stream_id,
    bool should_include_version,
    bool fin,
    spdy::SpdyHeaderBlock headers,
    size_t* spdy_headers_frame_length,
    quic::QuicStreamOffset* offset) {
  InitializeHeader(packet_number, should_include_version);
  spdy::SpdySerializedFrame spdy_frame;
  spdy::SpdyPushPromiseIR promise_frame(stream_id, promised_stream_id,
                                        std::move(headers));
  promise_frame.set_fin(fin);
  spdy_frame = spdy_request_framer_.SerializeFrame(promise_frame);
  if (spdy_headers_frame_length) {
    *spdy_headers_frame_length = spdy_frame.size();
  }
  if (offset != nullptr) {
    quic::QuicStreamFrame frame(
        quic::QuicUtils::GetHeadersStreamId(version_), false, *offset,
        quic::QuicStringPiece(spdy_frame.data(), spdy_frame.size()));
    *offset += spdy_frame.size();
    return MakePacket(header_, quic::QuicFrame(frame));
  } else {
    quic::QuicStreamFrame frame(
        quic::QuicUtils::GetHeadersStreamId(version_), false, 0,
        quic::QuicStringPiece(spdy_frame.data(), spdy_frame.size()));
    return MakePacket(header_, quic::QuicFrame(frame));
  }
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeForceHolDataPacket(
    quic::QuicPacketNumber packet_number,
    quic::QuicStreamId stream_id,
    bool should_include_version,
    bool fin,
    quic::QuicStreamOffset* offset,
    quic::QuicStringPiece data) {
  spdy::SpdyDataIR spdy_data(stream_id, data);
  spdy_data.set_fin(fin);
  spdy::SpdySerializedFrame spdy_frame(
      spdy_request_framer_.SerializeFrame(spdy_data));
  InitializeHeader(packet_number, should_include_version);
  quic::QuicStreamFrame quic_frame(
      quic::QuicUtils::GetHeadersStreamId(version_), false, *offset,
      quic::QuicStringPiece(spdy_frame.data(), spdy_frame.size()));
  *offset += spdy_frame.size();
  return MakePacket(header_, quic::QuicFrame(quic_frame));
}

// If |offset| is provided, will use the value when creating the packet.
// Will also update the value after packet creation.
std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeResponseHeadersPacket(
    quic::QuicPacketNumber packet_number,
    quic::QuicStreamId stream_id,
    bool should_include_version,
    bool fin,
    spdy::SpdyHeaderBlock headers,
    size_t* spdy_headers_frame_length,
    quic::QuicStreamOffset* offset) {
  InitializeHeader(packet_number, should_include_version);
  spdy::SpdySerializedFrame spdy_frame;
  spdy::SpdyHeadersIR headers_frame(stream_id, std::move(headers));
  headers_frame.set_fin(fin);
  spdy_frame = spdy_response_framer_.SerializeFrame(headers_frame);

  if (spdy_headers_frame_length) {
    *spdy_headers_frame_length = spdy_frame.size();
  }
  if (offset != nullptr) {
    quic::QuicStreamFrame frame(
        quic::QuicUtils::GetHeadersStreamId(version_), false, *offset,
        quic::QuicStringPiece(spdy_frame.data(), spdy_frame.size()));
    *offset += spdy_frame.size();
    return MakePacket(header_, quic::QuicFrame(frame));
  } else {
    quic::QuicStreamFrame frame(
        quic::QuicUtils::GetHeadersStreamId(version_), false, 0,
        quic::QuicStringPiece(spdy_frame.data(), spdy_frame.size()));
    return MakePacket(header_, quic::QuicFrame(frame));
  }
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeResponseHeadersPacket(
    quic::QuicPacketNumber packet_number,
    quic::QuicStreamId stream_id,
    bool should_include_version,
    bool fin,
    spdy::SpdyHeaderBlock headers,
    size_t* spdy_headers_frame_length) {
  return MakeResponseHeadersPacket(
      packet_number, stream_id, should_include_version, fin, std::move(headers),
      spdy_headers_frame_length, nullptr);
}

// Convenience method for calling MakeResponseHeadersPacket with nullptr for
// |spdy_headers_frame_length|.
std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeResponseHeadersPacketWithOffsetTracking(
    quic::QuicPacketNumber packet_number,
    quic::QuicStreamId stream_id,
    bool should_include_version,
    bool fin,
    spdy::SpdyHeaderBlock headers,
    quic::QuicStreamOffset* offset) {
  return MakeResponseHeadersPacket(packet_number, stream_id,
                                   should_include_version, fin,
                                   std::move(headers), nullptr, offset);
}

spdy::SpdyHeaderBlock QuicTestPacketMaker::GetRequestHeaders(
    const std::string& method,
    const std::string& scheme,
    const std::string& path) {
  spdy::SpdyHeaderBlock headers;
  headers[":method"] = method;
  headers[":authority"] = host_;
  headers[":scheme"] = scheme;
  headers[":path"] = path;
  return headers;
}

spdy::SpdyHeaderBlock QuicTestPacketMaker::ConnectRequestHeaders(
    const std::string& host_port) {
  spdy::SpdyHeaderBlock headers;
  headers[":method"] = "CONNECT";
  headers[":authority"] = host_port;
  return headers;
}

spdy::SpdyHeaderBlock QuicTestPacketMaker::GetResponseHeaders(
    const std::string& status) {
  spdy::SpdyHeaderBlock headers;
  headers[":status"] = status;
  headers["content-type"] = "text/plain";
  return headers;
}

spdy::SpdyHeaderBlock QuicTestPacketMaker::GetResponseHeaders(
    const std::string& status,
    const std::string& alt_svc) {
  spdy::SpdyHeaderBlock headers;
  headers[":status"] = status;
  headers["alt-svc"] = alt_svc;
  headers["content-type"] = "text/plain";
  return headers;
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::MakePacket(
    const quic::QuicPacketHeader& header,
    const quic::QuicFrame& frame) {
  quic::QuicFrames frames;
  frames.push_back(frame);
  return MakeMultipleFramesPacket(header, frames);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeMultipleFramesPacket(
    const quic::QuicPacketHeader& header,
    const quic::QuicFrames& frames) {
  quic::QuicFramer framer(quic::test::SupportedVersions(quic::ParsedQuicVersion(
                              quic::PROTOCOL_QUIC_CRYPTO, version_)),
                          clock_->Now(), perspective_);
  size_t max_plaintext_size =
      framer.GetMaxPlaintextSize(quic::kDefaultMaxPacketSize);
  std::unique_ptr<quic::QuicPacket> packet(quic::test::BuildUnsizedDataPacket(
      &framer, header, frames, max_plaintext_size));
  char buffer[quic::kMaxPacketSize];
  size_t encrypted_size =
      framer.EncryptPayload(quic::ENCRYPTION_NONE, header.packet_number,
                            *packet, buffer, quic::kMaxPacketSize);
  EXPECT_NE(0u, encrypted_size);
  quic::QuicReceivedPacket encrypted(buffer, encrypted_size, clock_->Now(),
                                     false);
  return encrypted.Clone();
}

void QuicTestPacketMaker::InitializeHeader(quic::QuicPacketNumber packet_number,
                                           bool should_include_version) {
  header_.destination_connection_id = connection_id_;
  header_.destination_connection_id_length = GetDestinationConnectionIdLength();
  header_.source_connection_id = connection_id_;
  header_.source_connection_id_length = GetSourceConnectionIdLength();
  header_.reset_flag = false;
  header_.version_flag = ShouldIncludeVersion(should_include_version);
  header_.long_packet_type = long_header_type_;
  header_.packet_number_length = GetPacketNumberLength();
  header_.packet_number = packet_number;
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeInitialSettingsPacket(
    quic::QuicPacketNumber packet_number,
    quic::QuicStreamOffset* offset) {
  std::string unused_data;
  return MakeInitialSettingsPacketAndSaveData(packet_number, offset,
                                              &unused_data);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeInitialSettingsPacketAndSaveData(
    quic::QuicPacketNumber packet_number,
    quic::QuicStreamOffset* offset,
    std::string* stream_data) {
  spdy::SpdySettingsIR settings_frame;
  settings_frame.AddSetting(spdy::SETTINGS_MAX_HEADER_LIST_SIZE,
                            quic::kDefaultMaxUncompressedHeaderSize);
  spdy::SpdySerializedFrame spdy_frame(
      spdy_request_framer_.SerializeFrame(settings_frame));
  InitializeHeader(packet_number, /*should_include_version*/ true);
  *stream_data = std::string(spdy_frame.data(), spdy_frame.size());
  if (offset != nullptr) {
    quic::QuicStreamFrame quic_frame(
        quic::QuicUtils::GetHeadersStreamId(version_), false, *offset,
        quic::QuicStringPiece(spdy_frame.data(), spdy_frame.size()));
    *offset += spdy_frame.size();
    return MakePacket(header_, quic::QuicFrame(quic_frame));
  }
  quic::QuicStreamFrame quic_frame(
      quic::QuicUtils::GetHeadersStreamId(version_), false, 0,
      quic::QuicStringPiece(spdy_frame.data(), spdy_frame.size()));
  return MakePacket(header_, quic::QuicFrame(quic_frame));
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakePriorityPacket(quic::QuicPacketNumber packet_number,
                                        bool should_include_version,
                                        quic::QuicStreamId id,
                                        quic::QuicStreamId parent_stream_id,
                                        spdy::SpdyPriority priority,
                                        quic::QuicStreamOffset* offset) {
  if (!client_headers_include_h2_stream_dependency_) {
    parent_stream_id = 0;
  }
  int weight = spdy::Spdy3PriorityToHttp2Weight(priority);
  bool exclusive = client_headers_include_h2_stream_dependency_;
  spdy::SpdyPriorityIR priority_frame(id, parent_stream_id, weight, exclusive);
  spdy::SpdySerializedFrame spdy_frame(
      spdy_request_framer_.SerializeFrame(priority_frame));

  quic::QuicStreamOffset header_offset = 0;
  if (offset != nullptr) {
    header_offset = *offset;
    *offset += spdy_frame.size();
  }
  quic::QuicStreamFrame quic_frame(
      quic::QuicUtils::GetHeadersStreamId(version_), false, header_offset,
      quic::QuicStringPiece(spdy_frame.data(), spdy_frame.size()));
  DVLOG(1) << "Adding frame: " << quic::QuicFrame(quic_frame);
  InitializeHeader(packet_number, should_include_version);
  return MakePacket(header_, quic::QuicFrame(quic_frame));
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeAckAndMultiplePriorityFramesPacket(
    quic::QuicPacketNumber packet_number,
    bool should_include_version,
    quic::QuicPacketNumber largest_received,
    quic::QuicPacketNumber smallest_received,
    quic::QuicPacketNumber least_unacked,
    const std::vector<Http2StreamDependency>& priority_frames,
    quic::QuicStreamOffset* offset) {
  quic::QuicAckFrame ack(MakeAckFrame(largest_received));
  ack.ack_delay_time = quic::QuicTime::Delta::Zero();
  for (quic::QuicPacketNumber i = smallest_received; i <= largest_received;
       ++i) {
    ack.received_packet_times.push_back(std::make_pair(i, clock_->Now()));
  }
  if (largest_received > 0) {
    ack.packets.AddRange(1, largest_received + 1);
  }
  quic::QuicFrames frames;
  frames.push_back(quic::QuicFrame(&ack));
  DVLOG(1) << "Adding frame: " << frames.back();

  quic::QuicStopWaitingFrame stop_waiting;
  if (version_ == quic::QUIC_VERSION_35) {
    stop_waiting.least_unacked = least_unacked;
    frames.push_back(quic::QuicFrame(&stop_waiting));
    DVLOG(1) << "Adding frame: " << frames.back();
  }

  const bool exclusive = client_headers_include_h2_stream_dependency_;
  quic::QuicStreamOffset header_offset = 0;
  if (offset == nullptr) {
    offset = &header_offset;
  }
  // Keep SpdySerializedFrames alive until MakeMultipleFramesPacket is done.
  std::vector<std::unique_ptr<spdy::SpdySerializedFrame>> spdy_frames;
  for (const Http2StreamDependency& info : priority_frames) {
    spdy::SpdyPriorityIR priority_frame(
        info.stream_id, info.parent_stream_id,
        spdy::Spdy3PriorityToHttp2Weight(info.spdy_priority), exclusive);

    spdy_frames.push_back(std::make_unique<spdy::SpdySerializedFrame>(
        spdy_request_framer_.SerializeFrame(priority_frame)));

    spdy::SpdySerializedFrame* spdy_frame = spdy_frames.back().get();
    quic::QuicStreamFrame stream_frame(
        quic::QuicUtils::GetHeadersStreamId(version_), false, *offset,
        quic::QuicStringPiece(spdy_frame->data(), spdy_frame->size()));
    *offset += spdy_frame->size();

    frames.push_back(quic::QuicFrame(stream_frame));
    DVLOG(1) << "Adding frame: " << frames.back();
  }

  InitializeHeader(packet_number, should_include_version);
  return MakeMultipleFramesPacket(header_, frames);
}

void QuicTestPacketMaker::SetEncryptionLevel(quic::EncryptionLevel level) {
  encryption_level_ = level;
}

void QuicTestPacketMaker::SetLongHeaderType(quic::QuicLongHeaderType type) {
  long_header_type_ = type;
}

bool QuicTestPacketMaker::ShouldIncludeVersion(bool include_version) const {
  if (version_ > quic::QUIC_VERSION_43) {
    return encryption_level_ < quic::ENCRYPTION_FORWARD_SECURE;
  }
  return include_version;
}

quic::QuicPacketNumberLength QuicTestPacketMaker::GetPacketNumberLength()
    const {
  if (version_ > quic::QUIC_VERSION_43 &&
      encryption_level_ < quic::ENCRYPTION_FORWARD_SECURE) {
    return quic::PACKET_4BYTE_PACKET_NUMBER;
  }
  return quic::PACKET_1BYTE_PACKET_NUMBER;
}

quic::QuicConnectionIdLength
QuicTestPacketMaker::GetDestinationConnectionIdLength() const {
  if (perspective_ == quic::Perspective::IS_SERVER &&
      version_ > quic::QUIC_VERSION_43) {
    return quic::PACKET_0BYTE_CONNECTION_ID;
  }
  return quic::PACKET_8BYTE_CONNECTION_ID;
}

quic::QuicConnectionIdLength QuicTestPacketMaker::GetSourceConnectionIdLength()
    const {
  if (perspective_ == quic::Perspective::IS_SERVER &&
      version_ > quic::QUIC_VERSION_43 &&
      encryption_level_ < quic::ENCRYPTION_FORWARD_SECURE) {
    return quic::PACKET_8BYTE_CONNECTION_ID;
  }
  return quic::PACKET_0BYTE_CONNECTION_ID;
}

}  // namespace test
}  // namespace net
