// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_test_packet_maker.h"

#include <list>
#include <utility>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "net/quic/mock_crypto_client_stream.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/quic/quic_http_utils.h"
#include "net/third_party/quiche/src/quic/core/crypto/null_encrypter.h"
#include "net/third_party/quiche/src/quic/core/http/http_constants.h"
#include "net/third_party/quiche/src/quic/core/quic_framer.h"
#include "net/third_party/quiche/src/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quic/test_tools/mock_random.h"
#include "net/third_party/quiche/src/quic/test_tools/quic_test_utils.h"
#include "net/third_party/quiche/src/quic/test_tools/simple_data_producer.h"

namespace net {
namespace test {
namespace {

quic::QuicAckFrame MakeAckFrame(uint64_t largest_observed) {
  quic::QuicAckFrame ack;
  ack.largest_acked = quic::QuicPacketNumber(largest_observed);
  return ack;
}

quic::QuicFrames CloneFrames(const quic::QuicFrames& frames) {
  quic::QuicFrames new_frames = frames;
  for (auto& frame : new_frames) {
    switch (frame.type) {
      // Frames smaller than a pointer are inlined, so don't need to be cloned.
      case quic::PADDING_FRAME:
      case quic::MTU_DISCOVERY_FRAME:
      case quic::PING_FRAME:
      case quic::MAX_STREAMS_FRAME:
      case quic::STOP_WAITING_FRAME:
      case quic::STREAMS_BLOCKED_FRAME:
      case quic::STREAM_FRAME:
        break;
      case quic::ACK_FRAME:
        frame.ack_frame = new quic::QuicAckFrame(*frame.ack_frame);
        break;
      case quic::RST_STREAM_FRAME:
        frame.rst_stream_frame =
            new quic::QuicRstStreamFrame(*frame.rst_stream_frame);
        break;
      case quic::CONNECTION_CLOSE_FRAME:
        frame.connection_close_frame =
            new quic::QuicConnectionCloseFrame(*frame.connection_close_frame);
        break;
      case quic::GOAWAY_FRAME:
        frame.goaway_frame = new quic::QuicGoAwayFrame(*frame.goaway_frame);
        break;
      case quic::BLOCKED_FRAME:
        frame.blocked_frame = new quic::QuicBlockedFrame(*frame.blocked_frame);
        break;
      case quic::WINDOW_UPDATE_FRAME:
        frame.window_update_frame =
            new quic::QuicWindowUpdateFrame(*frame.window_update_frame);
        break;
      case quic::PATH_CHALLENGE_FRAME:
        frame.path_challenge_frame =
            new quic::QuicPathChallengeFrame(*frame.path_challenge_frame);
        break;
      case quic::STOP_SENDING_FRAME:
        frame.stop_sending_frame =
            new quic::QuicStopSendingFrame(*frame.stop_sending_frame);
        break;
      case quic::NEW_CONNECTION_ID_FRAME:
        frame.new_connection_id_frame =
            new quic::QuicNewConnectionIdFrame(*frame.new_connection_id_frame);
        break;
      case quic::RETIRE_CONNECTION_ID_FRAME:
        frame.retire_connection_id_frame =
            new quic::QuicRetireConnectionIdFrame(
                *frame.retire_connection_id_frame);
        break;
      case quic::PATH_RESPONSE_FRAME:
        frame.path_response_frame =
            new quic::QuicPathResponseFrame(*frame.path_response_frame);
        break;
      case quic::MESSAGE_FRAME:
        DCHECK(false) << "Message frame not supported";
        // frame.message_frame = new
        // quic::QuicMessageFrame(*frame.message_frame);
        break;
      case quic::CRYPTO_FRAME:
        frame.crypto_frame = new quic::QuicCryptoFrame(*frame.crypto_frame);
        break;
      case quic::NEW_TOKEN_FRAME:
        frame.new_token_frame =
            new quic::QuicNewTokenFrame(*frame.new_token_frame);
        break;

      case quic::NUM_FRAME_TYPES:
        DCHECK(false) << "Cannot clone frame type: " << frame.type;
    }
  }
  return new_frames;
}

}  // namespace

void QuicTestPacketMaker::DecoderStreamErrorDelegate::OnDecoderStreamError(
    quic::QuicStringPiece error_message) {
  LOG(FATAL) << error_message;
}

void QuicTestPacketMaker::EncoderStreamSenderDelegate::WriteStreamData(
    quic::QuicStringPiece data) {
  LOG(FATAL) << "data.length: " << data.length();
}

QuicTestPacketMaker::QuicTestPacketMaker(
    quic::ParsedQuicVersion version,
    quic::QuicConnectionId connection_id,
    const quic::QuicClock* clock,
    const std::string& host,
    quic::Perspective perspective,
    bool client_headers_include_h2_stream_dependency)
    : version_(version),
      connection_id_(connection_id),
      clock_(clock),
      host_(host),
      spdy_request_framer_(spdy::SpdyFramer::ENABLE_COMPRESSION),
      spdy_response_framer_(spdy::SpdyFramer::ENABLE_COMPRESSION),
      coalesce_http_frames_(false),
      save_packet_frames_(false),
      qpack_encoder_(&decoder_stream_error_delegate_),
      perspective_(perspective),
      encryption_level_(quic::ENCRYPTION_FORWARD_SECURE),
      long_header_type_(quic::INVALID_PACKET_TYPE),
      client_headers_include_h2_stream_dependency_(
          client_headers_include_h2_stream_dependency &&
          version.transport_version >= quic::QUIC_VERSION_43) {
  DCHECK(!(perspective_ == quic::Perspective::IS_SERVER &&
           client_headers_include_h2_stream_dependency_));

  qpack_encoder_.set_qpack_stream_sender_delegate(
      &encoder_stream_sender_delegate_);
}

QuicTestPacketMaker::~QuicTestPacketMaker() {
  for (auto& kv : saved_frames_) {
    quic::DeleteFrames(&(kv.second));
  }
}

void QuicTestPacketMaker::set_hostname(const std::string& host) {
  host_.assign(host);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeConnectivityProbingPacket(uint64_t num,
                                                   bool include_version) {
  InitializeHeader(num, include_version);

  quic::QuicFramer framer(quic::test::SupportedVersions(version_),
                          clock_->Now(), perspective_,
                          quic::kQuicDefaultConnectionIdLength);
  if (encryption_level_ == quic::ENCRYPTION_INITIAL) {
    framer.SetInitialObfuscators(perspective_ == quic::Perspective::IS_CLIENT
                                     ? header_.destination_connection_id
                                     : header_.source_connection_id);
  } else {
    framer.SetEncrypter(encryption_level_,
                        std::make_unique<quic::NullEncrypter>(perspective_));
  }

  if (version_.transport_version != quic::QUIC_VERSION_99) {
    quic::QuicFrames frames{{quic::QuicFrame{quic::QuicPingFrame{}},
                             quic::QuicFrame{quic::QuicPaddingFrame{}}}};
    return MakeMultipleFramesPacket(header_, frames, nullptr);
  } else if (perspective_ == quic::Perspective::IS_CLIENT) {
    quic::test::MockRandom rand(0);
    quic::QuicPathFrameBuffer payload;
    rand.RandBytes(payload.data(), payload.size());
    quic::QuicPathChallengeFrame path_challenge(0, payload);
    quic::QuicFrames frames{{quic::QuicFrame{&path_challenge},
                             quic::QuicFrame{quic::QuicPaddingFrame{}}}};
    return MakeMultipleFramesPacket(header_, frames, nullptr);
  } else {
    quic::test::MockRandom rand(0);
    quic::QuicPathFrameBuffer payload;
    rand.RandBytes(payload.data(), payload.size());
    quic::QuicPathResponseFrame path_response(0, payload);
    quic::QuicFrames frames{{quic::QuicFrame{&path_response},
                             quic::QuicFrame{quic::QuicPaddingFrame{}}}};
    return MakeMultipleFramesPacket(header_, frames, nullptr);
  }
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::MakePingPacket(
    uint64_t num,
    bool include_version) {
  InitializeHeader(num, include_version);

  quic::QuicPingFrame ping;
  return MakePacket(header_, quic::QuicFrame(ping));
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeDummyCHLOPacket(uint64_t packet_num) {
  SetEncryptionLevel(quic::ENCRYPTION_INITIAL);
  InitializeHeader(packet_num, /*include_version=*/true);

  quic::CryptoHandshakeMessage message =
      MockCryptoClientStream::GetDummyCHLOMessage();
  const quic::QuicData& data = message.GetSerialized();

  quic::QuicFrames frames;
  quic::QuicCryptoFrame crypto_frame;
  quic::test::SimpleDataProducer producer;
  quic::QuicStreamFrameDataProducer* producer_p = nullptr;
  if (!QuicVersionUsesCryptoFrames(version_.transport_version)) {
    quic::QuicStreamFrame frame(
        quic::QuicUtils::GetCryptoStreamId(version_.transport_version),
        /*fin=*/false, /*offset=*/0, data.AsStringPiece());
    frames.push_back(quic::QuicFrame(frame));
  } else {
    crypto_frame =
        quic::QuicCryptoFrame(quic::ENCRYPTION_INITIAL, 0, data.length());
    producer.SaveCryptoData(quic::ENCRYPTION_INITIAL, 0, data.AsStringPiece());
    frames.push_back(quic::QuicFrame(&crypto_frame));
    producer_p = &producer;
  }
  DVLOG(1) << "Adding frame: " << frames.back();
  quic::QuicPaddingFrame padding;
  frames.push_back(quic::QuicFrame(padding));
  DVLOG(1) << "Adding frame: " << frames.back();

  std::unique_ptr<quic::QuicReceivedPacket> packet =
      MakeMultipleFramesPacket(header_, frames, producer_p);
  return packet;
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeAckAndPingPacket(uint64_t num,
                                          bool include_version,
                                          uint64_t largest_received,
                                          uint64_t smallest_received,
                                          uint64_t least_unacked) {
  InitializeHeader(num, include_version);

  quic::QuicAckFrame ack(MakeAckFrame(largest_received));
  ack.ack_delay_time = quic::QuicTime::Delta::Zero();
  for (uint64_t i = smallest_received; i <= largest_received; ++i) {
    ack.received_packet_times.push_back(
        std::make_pair(quic::QuicPacketNumber(i), clock_->Now()));
  }
  if (largest_received > 0) {
    ack.packets.AddRange(quic::QuicPacketNumber(1),
                         quic::QuicPacketNumber(largest_received + 1));
  }
  quic::QuicFrames frames;
  frames.push_back(quic::QuicFrame(&ack));
  DVLOG(1) << "Adding frame: " << frames.back();

  frames.push_back(quic::QuicFrame(quic::QuicPingFrame()));
  DVLOG(1) << "Adding frame: " << frames.back();

  return MakeMultipleFramesPacket(header_, frames, nullptr);
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::MakeRstPacket(
    uint64_t num,
    bool include_version,
    quic::QuicStreamId stream_id,
    quic::QuicRstStreamErrorCode error_code) {
  return MakeRstPacket(num, include_version, stream_id, error_code,
                       /*include_stop_sending_if_v99=*/true);
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::MakeRstPacket(
    uint64_t num,
    bool include_version,
    quic::QuicStreamId stream_id,
    quic::QuicRstStreamErrorCode error_code,
    bool include_stop_sending_if_v99) {
  InitializeHeader(num, include_version);

  quic::QuicFrames frames;

  quic::QuicRstStreamFrame rst(1, stream_id, error_code,
                               stream_offsets_[stream_id]);
  if (version_.transport_version != quic::QUIC_VERSION_99 ||
      quic::QuicUtils::IsBidirectionalStreamId(stream_id)) {
    frames.push_back(quic::QuicFrame(&rst));
  }
  DVLOG(1) << "Adding frame: " << frames.back();

  // The STOP_SENDING frame must be outside of the if (version==99) so that it
  // stays in scope until the packet is built.
  quic::QuicStopSendingFrame stop(1, stream_id, error_code);
  if (include_stop_sending_if_v99 &&
      version_.transport_version == quic::QUIC_VERSION_99) {
    frames.push_back(quic::QuicFrame(&stop));
    DVLOG(1) << "Adding frame: " << frames.back();
  }
  return MakeMultipleFramesPacket(header_, frames, nullptr);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeStreamsBlockedPacket(
    uint64_t num,
    bool include_version,
    quic::QuicStreamCount stream_count,
    bool unidirectional) {
  InitializeHeader(num, include_version);

  quic::QuicStreamsBlockedFrame frame(1, stream_count, unidirectional);
  DVLOG(1) << "Adding frame: " << quic::QuicFrame(frame);
  return MakePacket(header_, quic::QuicFrame(frame));
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeMaxStreamsPacket(uint64_t num,
                                          bool include_version,
                                          quic::QuicStreamCount stream_count,
                                          bool unidirectional) {
  InitializeHeader(num, include_version);

  quic::QuicMaxStreamsFrame frame(1, stream_count, unidirectional);
  DVLOG(1) << "Adding frame: " << quic::QuicFrame(frame);
  return MakePacket(header_, quic::QuicFrame(frame));
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRstAndRequestHeadersPacket(
    uint64_t num,
    bool include_version,
    quic::QuicStreamId rst_stream_id,
    quic::QuicRstStreamErrorCode rst_error_code,
    quic::QuicStreamId stream_id,
    bool fin,
    spdy::SpdyPriority priority,
    spdy::SpdyHeaderBlock headers,
    quic::QuicStreamId parent_stream_id,
    size_t* spdy_headers_frame_length) {
  InitializeHeader(num, include_version);
  quic::QuicRstStreamFrame rst_frame(1, rst_stream_id, rst_error_code, 0);
  quic::QuicFrames frames;
  frames.push_back(quic::QuicFrame(&rst_frame));
  DVLOG(1) << "Adding frame: " << frames.back();

  // The STOP_SENDING frame must be outside of the if (version==99) so that it
  // stays in scope until the packet is built.
  quic::QuicStopSendingFrame stop(1, rst_stream_id, rst_error_code);
  if (version_.transport_version == quic::QUIC_VERSION_99) {
    frames.push_back(quic::QuicFrame(&stop));
    DVLOG(1) << "Adding frame: " << frames.back();
  }

  if (quic::VersionUsesHttp3(version_.transport_version)) {
    // Send SETTINGS frame(s) if they have not already been sent.
    MaybeAddHttp3SettingsFrames(&frames);

    if (FLAGS_quic_allow_http3_priority) {
      std::string priority_data =
          GenerateHttp3PriorityData(priority, stream_id);
      frames.push_back(GenerateNextStreamFrame(2, false, priority_data));
    }

    // STREAM frames for HEADERS.
    std::vector<std::string> data = QpackEncodeHeaders(
        stream_id, std::move(headers), spdy_headers_frame_length);
    for (const auto& frame : GenerateNextStreamFrames(stream_id, fin, data))
      frames.push_back(frame);

    InitializeHeader(num, include_version);
    return MakeMultipleFramesPacket(header_, frames, nullptr);
  }

  spdy::SpdySerializedFrame spdy_frame = MakeSpdyHeadersFrame(
      stream_id, fin, priority, std::move(headers), parent_stream_id);
  if (spdy_headers_frame_length) {
    *spdy_headers_frame_length = spdy_frame.size();
  }
  frames.push_back(GenerateNextStreamFrame(
      GetHeadersStreamId(), false,
      quic::QuicStringPiece(spdy_frame.data(), spdy_frame.size())));

  InitializeHeader(num, include_version);
  return MakeMultipleFramesPacket(header_, frames, nullptr);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeAckAndRstPacket(
    uint64_t num,
    bool include_version,
    quic::QuicStreamId stream_id,
    quic::QuicRstStreamErrorCode error_code,
    uint64_t largest_received,
    uint64_t smallest_received,
    uint64_t least_unacked,
    bool send_feedback) {
  return MakeAckAndRstPacket(num, include_version, stream_id, error_code,
                             largest_received, smallest_received, least_unacked,
                             send_feedback,
                             /*include_stop_sending_if_v99=*/true);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeAckAndRstPacket(
    uint64_t num,
    bool include_version,
    quic::QuicStreamId stream_id,
    quic::QuicRstStreamErrorCode error_code,
    uint64_t largest_received,
    uint64_t smallest_received,
    uint64_t least_unacked,
    bool send_feedback,
    bool include_stop_sending_if_v99) {
  InitializeHeader(num, include_version);

  quic::QuicAckFrame ack(MakeAckFrame(largest_received));
  ack.ack_delay_time = quic::QuicTime::Delta::Zero();
  for (uint64_t i = smallest_received; i <= largest_received; ++i) {
    ack.received_packet_times.push_back(
        std::make_pair(quic::QuicPacketNumber(i), clock_->Now()));
  }
  if (largest_received > 0) {
    ack.packets.AddRange(quic::QuicPacketNumber(1),
                         quic::QuicPacketNumber(largest_received + 1));
  }
  quic::QuicFrames frames;
  frames.push_back(quic::QuicFrame(&ack));
  DVLOG(1) << "Adding frame: " << frames.back();

  quic::QuicRstStreamFrame rst(1, stream_id, error_code,
                               stream_offsets_[stream_id]);
  if (version_.transport_version != quic::QUIC_VERSION_99 ||
      quic::QuicUtils::IsBidirectionalStreamId(stream_id)) {
    frames.push_back(quic::QuicFrame(&rst));
    DVLOG(1) << "Adding frame: " << frames.back();
  }

  // The STOP_SENDING frame must be outside of the if (version==99) so that it
  // stays in scope until the packet is built.
  quic::QuicStopSendingFrame stop(1, stream_id, error_code);
  if (version_.transport_version == quic::QUIC_VERSION_99 &&
      include_stop_sending_if_v99) {
    frames.push_back(quic::QuicFrame(&stop));
    DVLOG(1) << "Adding frame: " << frames.back();
  }

  return MakeMultipleFramesPacket(header_, frames, nullptr);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRstAckAndConnectionClosePacket(
    uint64_t num,
    bool include_version,
    quic::QuicStreamId stream_id,
    quic::QuicRstStreamErrorCode error_code,
    quic::QuicTime::Delta ack_delay_time,
    uint64_t largest_received,
    uint64_t smallest_received,
    uint64_t least_unacked,
    quic::QuicErrorCode quic_error,
    const std::string& quic_error_details) {
  InitializeHeader(num, include_version);

  quic::QuicFrames frames;
  quic::QuicRstStreamFrame rst(1, stream_id, error_code, 0);
  frames.push_back(quic::QuicFrame(&rst));
  DVLOG(1) << "Adding frame: " << frames.back();

  // The STOP_SENDING frame must be outside of the if (version==99) so that it
  // stays in scope until the packet is built.
  quic::QuicStopSendingFrame stop(
      1, stream_id, static_cast<quic::QuicApplicationErrorCode>(error_code));
  if (version_.transport_version == quic::QUIC_VERSION_99) {
    frames.push_back(quic::QuicFrame(&stop));
    DVLOG(1) << "Adding frame: " << frames.back();
  }

  quic::QuicAckFrame ack(MakeAckFrame(largest_received));
  ack.ack_delay_time = ack_delay_time;
  for (uint64_t i = smallest_received; i <= largest_received; ++i) {
    ack.received_packet_times.push_back(
        std::make_pair(quic::QuicPacketNumber(i), clock_->Now()));
  }
  if (largest_received > 0) {
    ack.packets.AddRange(quic::QuicPacketNumber(1),
                         quic::QuicPacketNumber(largest_received + 1));
  }
  frames.push_back(quic::QuicFrame(&ack));
  DVLOG(1) << "Adding frame: " << frames.back();

  quic::QuicConnectionCloseFrame close(version_.transport_version, quic_error,
                                       quic_error_details,
                                       /*transport_close_frame_type=*/0);

  frames.push_back(quic::QuicFrame(&close));
  DVLOG(1) << "Adding frame: " << frames.back();

  return MakeMultipleFramesPacket(header_, frames, nullptr);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRstAndConnectionClosePacket(
    uint64_t num,
    bool include_version,
    quic::QuicStreamId stream_id,
    quic::QuicRstStreamErrorCode error_code,
    quic::QuicErrorCode quic_error,
    const std::string& quic_error_details) {
  InitializeHeader(num, include_version);

  quic::QuicFrames frames;
  quic::QuicRstStreamFrame rst(1, stream_id, error_code, 0);
  frames.push_back(quic::QuicFrame(&rst));
  DVLOG(1) << "Adding frame: " << frames.back();

  // The STOP_SENDING frame must be outside of the if (version==99) so that it
  // stays in scope until the packet is built.
  quic::QuicStopSendingFrame stop(
      1, stream_id, static_cast<quic::QuicApplicationErrorCode>(error_code));
  if (version_.transport_version == quic::QUIC_VERSION_99) {
    frames.push_back(quic::QuicFrame(&stop));
    DVLOG(1) << "Adding frame: " << frames.back();
  }

  quic::QuicConnectionCloseFrame close(version_.transport_version, quic_error,
                                       quic_error_details,
                                       /*transport_close_frame_type=*/0);

  frames.push_back(quic::QuicFrame(&close));
  DVLOG(1) << "Adding frame: " << frames.back();

  return MakeMultipleFramesPacket(header_, frames, nullptr);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeAckAndConnectionClosePacket(
    uint64_t num,
    bool include_version,
    quic::QuicTime::Delta ack_delay_time,
    uint64_t largest_received,
    uint64_t smallest_received,
    uint64_t least_unacked,
    quic::QuicErrorCode quic_error,
    const std::string& quic_error_details,
    uint64_t frame_type) {
  InitializeHeader(num, include_version);

  quic::QuicAckFrame ack(MakeAckFrame(largest_received));
  ack.ack_delay_time = ack_delay_time;
  for (uint64_t i = smallest_received; i <= largest_received; ++i) {
    ack.received_packet_times.push_back(
        std::make_pair(quic::QuicPacketNumber(i), clock_->Now()));
  }
  if (largest_received > 0) {
    ack.packets.AddRange(quic::QuicPacketNumber(1),
                         quic::QuicPacketNumber(largest_received + 1));
  }
  quic::QuicFrames frames;
  frames.push_back(quic::QuicFrame(&ack));
  DVLOG(1) << "Adding frame: " << frames.back();

  quic::QuicConnectionCloseFrame close(version_.transport_version, quic_error,
                                       quic_error_details, frame_type);

  frames.push_back(quic::QuicFrame(&close));
  DVLOG(1) << "Adding frame: " << frames.back();

  return MakeMultipleFramesPacket(header_, frames, nullptr);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeConnectionClosePacket(
    uint64_t num,
    bool include_version,
    quic::QuicErrorCode quic_error,
    const std::string& quic_error_details) {
  InitializeHeader(num, include_version);

  quic::QuicConnectionCloseFrame close(version_.transport_version, quic_error,
                                       quic_error_details,
                                       /*transport_close_frame_type=*/0);

  return MakePacket(header_, quic::QuicFrame(&close));
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::MakeGoAwayPacket(
    uint64_t num,
    quic::QuicErrorCode error_code,
    std::string reason_phrase) {
  InitializeHeader(num, /*include_version=*/false);

  quic::QuicGoAwayFrame goaway;
  goaway.error_code = error_code;
  goaway.last_good_stream_id = 0;
  goaway.reason_phrase = reason_phrase;
  return MakePacket(header_, quic::QuicFrame(&goaway));
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::MakeAckPacket(
    uint64_t packet_number,
    uint64_t largest_received,
    uint64_t smallest_received,
    uint64_t least_unacked,
    bool send_feedback) {
  return MakeAckPacket(packet_number, 1, largest_received, smallest_received,
                       least_unacked, send_feedback,
                       quic::QuicTime::Delta::Zero());
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::MakeAckPacket(
    uint64_t packet_number,
    uint64_t first_received,
    uint64_t largest_received,
    uint64_t smallest_received,
    uint64_t least_unacked,
    bool send_feedback) {
  return MakeAckPacket(packet_number, first_received, largest_received,
                       smallest_received, least_unacked, send_feedback,
                       quic::QuicTime::Delta::Zero());
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::MakeAckPacket(
    uint64_t packet_number,
    uint64_t largest_received,
    uint64_t smallest_received,
    uint64_t least_unacked,
    bool send_feedback,
    quic::QuicTime::Delta ack_delay_time) {
  return MakeAckPacket(packet_number, 1, largest_received, smallest_received,
                       least_unacked, send_feedback, ack_delay_time);
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::MakeAckPacket(
    uint64_t packet_number,
    uint64_t first_received,
    uint64_t largest_received,
    uint64_t smallest_received,
    uint64_t least_unacked,
    bool send_feedback,
    quic::QuicTime::Delta ack_delay_time) {
  InitializeHeader(packet_number, /*include_version=*/false);

  quic::QuicAckFrame ack(MakeAckFrame(largest_received));
  ack.ack_delay_time = ack_delay_time;
  for (uint64_t i = smallest_received; i <= largest_received; ++i) {
    ack.received_packet_times.push_back(
        std::make_pair(quic::QuicPacketNumber(i), clock_->Now()));
  }
  if (largest_received > 0) {
    DCHECK_GE(largest_received, first_received);
    ack.packets.AddRange(quic::QuicPacketNumber(first_received),
                         quic::QuicPacketNumber(largest_received + 1));
  }
  quic::QuicFramer framer(quic::test::SupportedVersions(version_),
                          clock_->Now(), perspective_,
                          quic::kQuicDefaultConnectionIdLength);
  if (encryption_level_ == quic::ENCRYPTION_INITIAL) {
    framer.SetInitialObfuscators(perspective_ == quic::Perspective::IS_CLIENT
                                     ? header_.destination_connection_id
                                     : header_.source_connection_id);
  } else {
    framer.SetEncrypter(encryption_level_,
                        std::make_unique<quic::NullEncrypter>(perspective_));
  }
  quic::QuicFrames frames;
  quic::QuicFrame ack_frame(&ack);
  frames.push_back(ack_frame);
  DVLOG(1) << "Adding frame: " << frames.back();

  size_t max_plaintext_size =
      framer.GetMaxPlaintextSize(quic::kDefaultMaxPacketSize);
  size_t ack_frame_length = framer.GetSerializedFrameLength(
      ack_frame, max_plaintext_size, /*first_frame*/ true, /*last_frame*/ false,
      header_.packet_number_length);
  const size_t min_plaintext_size = 7;
  if (version_.HasHeaderProtection() && ack_frame_length < min_plaintext_size) {
    size_t padding_length = min_plaintext_size - ack_frame_length;
    frames.push_back(quic::QuicFrame(quic::QuicPaddingFrame(padding_length)));
  }

  std::unique_ptr<quic::QuicPacket> packet(
      quic::test::BuildUnsizedDataPacket(&framer, header_, frames));
  char buffer[quic::kMaxOutgoingPacketSize];
  size_t encrypted_size =
      framer.EncryptPayload(encryption_level_, header_.packet_number, *packet,
                            buffer, quic::kMaxOutgoingPacketSize);
  EXPECT_NE(0u, encrypted_size);
  quic::QuicReceivedPacket encrypted(buffer, encrypted_size, clock_->Now(),
                                     false);
  return encrypted.Clone();
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::MakeDataPacket(
    uint64_t packet_number,
    quic::QuicStreamId stream_id,
    bool should_include_version,
    bool fin,
    quic::QuicStringPiece data) {
  InitializeHeader(packet_number, should_include_version);
  return MakePacket(header_, GenerateNextStreamFrame(stream_id, fin, data));
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeMultipleDataFramesPacket(
    uint64_t packet_number,
    quic::QuicStreamId stream_id,
    bool should_include_version,
    bool fin,
    const std::vector<std::string>& data_writes) {
  InitializeHeader(packet_number, should_include_version);
  quic::QuicFrames data_frames;
  for (size_t i = 0; i < data_writes.size(); ++i) {
    bool is_fin = fin && (i == data_writes.size() - 1);
    data_frames.push_back(
        GenerateNextStreamFrame(stream_id, is_fin, data_writes[i]));
  }
  return MakeMultipleFramesPacket(header_, data_frames, nullptr);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeAckAndDataPacket(uint64_t packet_number,
                                          bool include_version,
                                          quic::QuicStreamId stream_id,
                                          uint64_t largest_received,
                                          uint64_t smallest_received,
                                          uint64_t least_unacked,
                                          bool fin,
                                          quic::QuicStringPiece data) {
  InitializeHeader(packet_number, include_version);

  quic::QuicAckFrame ack(MakeAckFrame(largest_received));
  ack.ack_delay_time = quic::QuicTime::Delta::Zero();
  for (uint64_t i = smallest_received; i <= largest_received; ++i) {
    ack.received_packet_times.push_back(
        std::make_pair(quic::QuicPacketNumber(i), clock_->Now()));
  }
  if (largest_received > 0) {
    ack.packets.AddRange(quic::QuicPacketNumber(1),
                         quic::QuicPacketNumber(largest_received + 1));
  }
  quic::QuicFrames frames;
  frames.push_back(quic::QuicFrame(&ack));
  DVLOG(1) << "Adding frame: " << frames.back();

  frames.push_back(GenerateNextStreamFrame(stream_id, fin, data));

  return MakeMultipleFramesPacket(header_, frames, nullptr);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeAckAndMultipleDataFramesPacket(
    uint64_t packet_number,
    bool include_version,
    quic::QuicStreamId stream_id,
    uint64_t largest_received,
    uint64_t smallest_received,
    uint64_t least_unacked,
    bool fin,
    const std::vector<std::string>& data_writes) {
  InitializeHeader(packet_number, include_version);

  quic::QuicAckFrame ack(MakeAckFrame(largest_received));
  ack.ack_delay_time = quic::QuicTime::Delta::Zero();
  for (uint64_t i = smallest_received; i <= largest_received; ++i) {
    ack.received_packet_times.push_back(
        std::make_pair(quic::QuicPacketNumber(i), clock_->Now()));
  }
  if (largest_received > 0) {
    ack.packets.AddRange(quic::QuicPacketNumber(1),
                         quic::QuicPacketNumber(largest_received + 1));
  }
  quic::QuicFrames frames;
  frames.push_back(quic::QuicFrame(&ack));
  DVLOG(1) << "Adding frame: " << frames.back();

  for (size_t i = 0; i < data_writes.size(); ++i) {
    bool is_fin = fin && (i == data_writes.size() - 1);
    frames.push_back(GenerateNextStreamFrame(
        stream_id, is_fin, quic::QuicStringPiece(data_writes[i])));
  }
  return MakeMultipleFramesPacket(header_, frames, nullptr);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRequestHeadersAndMultipleDataFramesPacket(
    uint64_t packet_number,
    quic::QuicStreamId stream_id,
    bool should_include_version,
    bool fin,
    spdy::SpdyPriority priority,
    spdy::SpdyHeaderBlock headers,
    quic::QuicStreamId parent_stream_id,
    size_t* spdy_headers_frame_length,
    const std::vector<std::string>& data_writes) {
  InitializeHeader(packet_number, should_include_version);
  if (quic::VersionUsesHttp3(version_.transport_version)) {
    quic::QuicFrames frames;

    // Send SETTINGS frame(s) if they have not already been sent.
    MaybeAddHttp3SettingsFrames(&frames);

    if (FLAGS_quic_allow_http3_priority) {
      std::string priority_data =
          GenerateHttp3PriorityData(priority, stream_id);
      frames.push_back(GenerateNextStreamFrame(2, false, priority_data));
    }
    // STREAM frames for HEADERS.
    std::vector<std::string> data = QpackEncodeHeaders(
        stream_id, std::move(headers), spdy_headers_frame_length);

    for (const auto& frame : GenerateNextStreamFrames(stream_id, false, data))
      frames.push_back(frame);

    // STREAM frames for DATA.
    for (size_t i = 0; i < data_writes.size(); ++i) {
      bool is_fin = fin && (i == data_writes.size() - 1);
      frames.push_back(GenerateNextStreamFrame(
          stream_id, is_fin, quic::QuicStringPiece(data_writes[i])));
    }

    return MakeMultipleFramesPacket(header_, frames, nullptr);
  }

  spdy::SpdySerializedFrame spdy_frame =
      MakeSpdyHeadersFrame(stream_id, fin && data_writes.empty(), priority,
                           std::move(headers), parent_stream_id);

  if (spdy_headers_frame_length) {
    *spdy_headers_frame_length = spdy_frame.size();
  }
  quic::QuicFrames frames;
  frames.push_back(GenerateNextStreamFrame(
      GetHeadersStreamId(), false,
      quic::QuicStringPiece(spdy_frame.data(), spdy_frame.size())));

  // quic::QuicFrame takes a raw pointer. Use a std::vector here so we keep
  // StreamFrames alive until MakeMultipleFramesPacket is done.
  std::vector<std::unique_ptr<quic::QuicStreamFrame>> stream_frames;
  for (size_t i = 0; i < data_writes.size(); ++i) {
    bool is_fin = fin && (i == data_writes.size() - 1);
    frames.push_back(GenerateNextStreamFrame(
        stream_id, is_fin, quic::QuicStringPiece(data_writes[i])));
  }
  return MakeMultipleFramesPacket(header_, frames, nullptr);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRequestHeadersPacket(
    uint64_t packet_number,
    quic::QuicStreamId stream_id,
    bool should_include_version,
    bool fin,
    spdy::SpdyPriority priority,
    spdy::SpdyHeaderBlock headers,
    quic::QuicStreamId parent_stream_id,
    size_t* spdy_headers_frame_length) {
  InitializeHeader(packet_number, should_include_version);

  if (quic::VersionUsesHttp3(version_.transport_version)) {
    quic::QuicFrames frames;

    // Send SETTINGS frame(s) if they have not already been sent.
    MaybeAddHttp3SettingsFrames(&frames);

    if (FLAGS_quic_allow_http3_priority) {
      std::string priority_data =
          GenerateHttp3PriorityData(priority, stream_id);
      frames.push_back(GenerateNextStreamFrame(2, false, priority_data));
    }
    std::vector<std::string> data = QpackEncodeHeaders(
        stream_id, std::move(headers), spdy_headers_frame_length);

    for (const auto& frame : GenerateNextStreamFrames(stream_id, fin, data))
      frames.push_back(frame);
    return MakeMultipleFramesPacket(header_, frames, nullptr);
  }

  spdy::SpdySerializedFrame spdy_frame = MakeSpdyHeadersFrame(
      stream_id, fin, priority, std::move(headers), parent_stream_id);

  if (spdy_headers_frame_length)
    *spdy_headers_frame_length = spdy_frame.size();

  return MakePacket(header_, GenerateNextStreamFrame(
                                 GetHeadersStreamId(), false,
                                 quic::QuicStringPiece(spdy_frame.data(),
                                                       spdy_frame.size())));
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRequestHeadersAndRstPacket(
    uint64_t packet_number,
    quic::QuicStreamId stream_id,
    bool should_include_version,
    bool fin,
    spdy::SpdyPriority priority,
    spdy::SpdyHeaderBlock headers,
    quic::QuicStreamId parent_stream_id,
    size_t* spdy_headers_frame_length,
    quic::QuicRstStreamErrorCode error_code) {
  if (quic::VersionUsesHttp3(version_.transport_version)) {
    quic::QuicFrames frames;

    // Send SETTINGS frame(s) if they have not already been sent.
    MaybeAddHttp3SettingsFrames(&frames);

    if (FLAGS_quic_allow_http3_priority) {
      std::string priority_data =
          GenerateHttp3PriorityData(priority, stream_id);
      frames.push_back(GenerateNextStreamFrame(2, false, priority_data));
    }
    std::vector<std::string> data = QpackEncodeHeaders(
        stream_id, std::move(headers), spdy_headers_frame_length);

    for (const auto& frame : GenerateNextStreamFrames(stream_id, fin, data))
      frames.push_back(frame);

    quic::QuicRstStreamFrame rst_frame(1, stream_id, error_code,
                                       stream_offsets_[stream_id]);
    frames.push_back(quic::QuicFrame(&rst_frame));
    DVLOG(1) << "Adding frame: " << frames.back();

    quic::QuicStopSendingFrame stop(1, stream_id, error_code);
    frames.push_back(quic::QuicFrame(&stop));
    DVLOG(1) << "Adding frame: " << frames.back();
    InitializeHeader(packet_number, should_include_version);
    return MakeMultipleFramesPacket(header_, frames, nullptr);
  }

  spdy::SpdySerializedFrame spdy_frame = MakeSpdyHeadersFrame(
      stream_id, fin, priority, std::move(headers), parent_stream_id);
  if (spdy_headers_frame_length) {
    *spdy_headers_frame_length = spdy_frame.size();
  }
  quic::QuicFrames frames;
  frames.push_back(GenerateNextStreamFrame(
      GetHeadersStreamId(), false,
      quic::QuicStringPiece(spdy_frame.data(), spdy_frame.size())));
  DVLOG(1) << "Adding frame: " << frames.back();

  quic::QuicRstStreamFrame rst_frame(1, stream_id, error_code,
                                     stream_offsets_[stream_id]);

  frames.push_back(quic::QuicFrame(&rst_frame));
  DVLOG(1) << "Adding frame: " << frames.back();

  // The STOP_SENDING frame must be outside of the if (version==99) so that it
  // stays in scope until the packet is built.
  quic::QuicStopSendingFrame stop(1, stream_id, error_code);
  if (version_.transport_version == quic::QUIC_VERSION_99) {
    frames.push_back(quic::QuicFrame(&stop));
    DVLOG(1) << "Adding frame: " << frames.back();
  }

  InitializeHeader(packet_number, should_include_version);
  return MakeMultipleFramesPacket(header_, frames, nullptr);
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

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakePushPromisePacket(
    uint64_t packet_number,
    quic::QuicStreamId stream_id,
    quic::QuicStreamId promised_stream_id,
    bool should_include_version,
    bool fin,
    spdy::SpdyHeaderBlock headers,
    size_t* spdy_headers_frame_length) {
  if (quic::VersionUsesHttp3(version_.transport_version)) {
    std::string encoded_headers =
        qpack_encoder_.EncodeHeaderList(stream_id, headers, nullptr);
    quic::QuicFrames frames;
    quic::PushPromiseFrame frame;
    frame.push_id = promised_stream_id;
    frame.headers = encoded_headers;
    std::unique_ptr<char[]> buffer;
    quic::QuicByteCount frame_length =
        quic::HttpEncoder::SerializePushPromiseFrameWithOnlyPushId(frame,
                                                                   &buffer);
    std::string push_promise_data(buffer.get(), frame_length);
    frames.push_back(
        GenerateNextStreamFrame(stream_id, false, push_promise_data));
    frames.push_back(
        GenerateNextStreamFrame(stream_id, false, encoded_headers));
    InitializeHeader(packet_number, should_include_version);
    return MakeMultipleFramesPacket(header_, frames, nullptr);
  }
  InitializeHeader(packet_number, should_include_version);
  spdy::SpdySerializedFrame spdy_frame;
  spdy::SpdyPushPromiseIR promise_frame(stream_id, promised_stream_id,
                                        std::move(headers));
  promise_frame.set_fin(fin);
  spdy_frame = spdy_request_framer_.SerializeFrame(promise_frame);
  if (spdy_headers_frame_length) {
    *spdy_headers_frame_length = spdy_frame.size();
  }
  return MakePacket(header_, GenerateNextStreamFrame(
                                 GetHeadersStreamId(), false,
                                 quic::QuicStringPiece(spdy_frame.data(),
                                                       spdy_frame.size())));
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeForceHolDataPacket(uint64_t packet_number,
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
      GetHeadersStreamId(), false, *offset,
      quic::QuicStringPiece(spdy_frame.data(), spdy_frame.size()));
  *offset += spdy_frame.size();
  return MakePacket(header_, quic::QuicFrame(quic_frame));
}

// If |offset| is provided, will use the value when creating the packet.
// Will also update the value after packet creation.
std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeResponseHeadersPacket(
    uint64_t packet_number,
    quic::QuicStreamId stream_id,
    bool should_include_version,
    bool fin,
    spdy::SpdyHeaderBlock headers,
    size_t* spdy_headers_frame_length) {
  InitializeHeader(packet_number, should_include_version);

  if (quic::VersionUsesHttp3(version_.transport_version)) {
    // STREAM frames for HEADERS.
    std::vector<std::string> data = QpackEncodeHeaders(
        stream_id, std::move(headers), spdy_headers_frame_length);

    quic::QuicFrames frames;
    for (const auto& frame : GenerateNextStreamFrames(stream_id, fin, data))
      frames.push_back(frame);
    return MakeMultipleFramesPacket(header_, frames, nullptr);
  }

  spdy::SpdySerializedFrame spdy_frame;
  spdy::SpdyHeadersIR headers_frame(stream_id, std::move(headers));
  headers_frame.set_fin(fin);
  spdy_frame = spdy_response_framer_.SerializeFrame(headers_frame);

  if (spdy_headers_frame_length) {
    *spdy_headers_frame_length = spdy_frame.size();
  }
  return MakePacket(header_, GenerateNextStreamFrame(
                                 GetHeadersStreamId(), false,
                                 quic::QuicStringPiece(spdy_frame.data(),
                                                       spdy_frame.size())));
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
  return MakeMultipleFramesPacket(header, frames, nullptr);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeMultipleFramesPacket(
    const quic::QuicPacketHeader& header,
    const quic::QuicFrames& frames,
    quic::QuicStreamFrameDataProducer* data_producer) {
  quic::QuicFramer framer(quic::test::SupportedVersions(version_),
                          clock_->Now(), perspective_,
                          quic::kQuicDefaultConnectionIdLength);
  if (encryption_level_ == quic::ENCRYPTION_INITIAL) {
    framer.SetInitialObfuscators(perspective_ == quic::Perspective::IS_CLIENT
                                     ? header_.destination_connection_id
                                     : header_.source_connection_id);
  } else {
    framer.SetEncrypter(encryption_level_,
                        std::make_unique<quic::NullEncrypter>(perspective_));
  }
  if (data_producer != nullptr) {
    framer.set_data_producer(data_producer);
  }
  quic::QuicFrames frames_copy = CloneFrames(frames);
  size_t max_plaintext_size =
      framer.GetMaxPlaintextSize(quic::kDefaultMaxPacketSize);
  if (version_.HasHeaderProtection()) {
    size_t packet_size =
        quic::GetPacketHeaderSize(version_.transport_version, header);
    size_t frames_size = 0;
    for (size_t i = 0; i < frames.size(); ++i) {
      bool first_frame = i == 0;
      bool last_frame = i == frames.size() - 1;
      const size_t frame_size = framer.GetSerializedFrameLength(
          frames[i], max_plaintext_size - packet_size, first_frame, last_frame,
          header.packet_number_length);
      packet_size += frame_size;
      frames_size += frame_size;
    }
    // This should be done by calling QuicPacketCreator::MinPlaintextPacketSize.
    const size_t min_plaintext_size = 7;
    if (frames_size < min_plaintext_size) {
      size_t padding_length = min_plaintext_size - frames_size;
      frames_copy.push_back(
          quic::QuicFrame(quic::QuicPaddingFrame(padding_length)));
    }
  }
  std::unique_ptr<quic::QuicPacket> packet(quic::test::BuildUnsizedDataPacket(
      &framer, header, frames_copy, max_plaintext_size));
  char buffer[quic::kMaxOutgoingPacketSize];
  size_t encrypted_size =
      framer.EncryptPayload(encryption_level_, header.packet_number, *packet,
                            buffer, quic::kMaxOutgoingPacketSize);
  EXPECT_NE(0u, encrypted_size);
  quic::QuicReceivedPacket encrypted(buffer, encrypted_size, clock_->Now(),
                                     false);
  if (save_packet_frames_) {
    saved_frames_[header.packet_number] = frames_copy;
  } else {
    saved_stream_data_.clear();
    DeleteFrames(&frames_copy);
  }
  return encrypted.Clone();
}

void QuicTestPacketMaker::InitializeHeader(uint64_t packet_number,
                                           bool should_include_version) {
  header_.destination_connection_id = DestinationConnectionId();
  header_.destination_connection_id_included = HasDestinationConnectionId();
  header_.source_connection_id = SourceConnectionId();
  header_.source_connection_id_included = HasSourceConnectionId();
  header_.reset_flag = false;
  header_.version_flag = ShouldIncludeVersion(should_include_version);
  if (quic::VersionHasIetfInvariantHeader(version_.transport_version)) {
    header_.form = header_.version_flag ? quic::IETF_QUIC_LONG_HEADER_PACKET
                                        : quic::IETF_QUIC_SHORT_HEADER_PACKET;
  }
  header_.long_packet_type = long_header_type_;
  header_.packet_number_length = GetPacketNumberLength();
  header_.packet_number = quic::QuicPacketNumber(packet_number);
  if (quic::QuicVersionHasLongHeaderLengths(version_.transport_version) &&
      header_.version_flag) {
    if (long_header_type_ == quic::INITIAL) {
      header_.retry_token_length_length =
          quic::VARIABLE_LENGTH_INTEGER_LENGTH_1;
    }
    header_.length_length = quic::VARIABLE_LENGTH_INTEGER_LENGTH_2;
  }
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeInitialSettingsPacket(uint64_t packet_number) {
  if (!quic::VersionUsesHttp3(version_.transport_version)) {
    spdy::SpdySettingsIR settings_frame;
    settings_frame.AddSetting(spdy::SETTINGS_MAX_HEADER_LIST_SIZE,
                              kQuicMaxHeaderListSize);
    settings_frame.AddSetting(quic::SETTINGS_QPACK_BLOCKED_STREAMS,
                              quic::kDefaultMaximumBlockedStreams);
    spdy::SpdySerializedFrame spdy_frame(
        spdy_request_framer_.SerializeFrame(settings_frame));
    InitializeHeader(packet_number, /*should_include_version*/ true);
    return MakePacket(header_, GenerateNextStreamFrame(
                                   GetHeadersStreamId(), false,
                                   quic::QuicStringPiece(spdy_frame.data(),
                                                         spdy_frame.size())));
  }
  quic::QuicFrames frames;

  MaybeAddHttp3SettingsFrames(&frames);

  InitializeHeader(packet_number, /*should_include_version*/ true);
  return MakeMultipleFramesPacket(header_, frames, nullptr);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakePriorityPacket(uint64_t packet_number,
                                        bool should_include_version,
                                        quic::QuicStreamId id,
                                        quic::QuicStreamId parent_stream_id,
                                        spdy::SpdyPriority priority) {
  if (!client_headers_include_h2_stream_dependency_ ||
      quic::VersionUsesHttp3(version_.transport_version)) {
    // TODO(rch): both stream_dependencies and priority frames need to be
    // supported in IETF QUIC.
    parent_stream_id = 0;
  }
  int weight = spdy::Spdy3PriorityToHttp2Weight(priority);
  bool exclusive = client_headers_include_h2_stream_dependency_;
  if (!VersionUsesHttp3(version_.transport_version)) {
    spdy::SpdyPriorityIR priority_frame(id, parent_stream_id, weight,
                                        exclusive);
    spdy::SpdySerializedFrame spdy_frame(
        spdy_request_framer_.SerializeFrame(priority_frame));

    InitializeHeader(packet_number, should_include_version);
    return MakePacket(header_, GenerateNextStreamFrame(
                                   GetHeadersStreamId(), false,
                                   quic::QuicStringPiece(spdy_frame.data(),
                                                         spdy_frame.size())));
  }
  quic::PriorityFrame frame;
  frame.weight = weight;
  frame.exclusive = true;
  frame.prioritized_element_id = id;
  frame.element_dependency_id = parent_stream_id;
  frame.dependency_type = quic::REQUEST_STREAM;
  frame.prioritized_type =
      quic::QuicUtils::IsServerInitiatedStreamId(version_.transport_version, id)
          ? quic::PUSH_STREAM
          : quic::REQUEST_STREAM;
  std::unique_ptr<char[]> buffer;
  quic::QuicByteCount frame_length =
      quic::HttpEncoder::SerializePriorityFrame(frame, &buffer);
  std::string priority_data = std::string(buffer.get(), frame_length);

  InitializeHeader(packet_number, should_include_version);
  return MakePacket(header_, GenerateNextStreamFrame(2, false, priority_data));
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeAckAndMultiplePriorityFramesPacket(
    uint64_t packet_number,
    bool should_include_version,
    uint64_t largest_received,
    uint64_t smallest_received,
    uint64_t least_unacked,
    const std::vector<Http2StreamDependency>& priority_frames) {
  quic::QuicAckFrame ack(MakeAckFrame(largest_received));
  ack.ack_delay_time = quic::QuicTime::Delta::Zero();
  for (uint64_t i = smallest_received; i <= largest_received; ++i) {
    ack.received_packet_times.push_back(
        std::make_pair(quic::QuicPacketNumber(i), clock_->Now()));
  }
  if (largest_received > 0) {
    ack.packets.AddRange(quic::QuicPacketNumber(1),
                         quic::QuicPacketNumber(largest_received + 1));
  }
  quic::QuicFrames frames;
  frames.push_back(quic::QuicFrame(&ack));
  DVLOG(1) << "Adding frame: " << frames.back();

  const bool exclusive = client_headers_include_h2_stream_dependency_;
  // Keep SpdySerializedFrames alive until MakeMultipleFramesPacket is done.
  std::vector<std::unique_ptr<spdy::SpdySerializedFrame>> spdy_frames;
  for (const Http2StreamDependency& info : priority_frames) {
    spdy::SpdyPriorityIR priority_frame(
        info.stream_id, info.parent_stream_id,
        spdy::Spdy3PriorityToHttp2Weight(info.spdy_priority), exclusive);

    spdy_frames.push_back(std::make_unique<spdy::SpdySerializedFrame>(
        spdy_request_framer_.SerializeFrame(priority_frame)));

    spdy::SpdySerializedFrame* spdy_frame = spdy_frames.back().get();
    frames.push_back(GenerateNextStreamFrame(
        quic::VersionUsesHttp3(version_.transport_version)
            ? GetFirstBidirectionalStreamId()
            : GetHeadersStreamId(),
        false, quic::QuicStringPiece(spdy_frame->data(), spdy_frame->size())));
  }

  InitializeHeader(packet_number, should_include_version);
  return MakeMultipleFramesPacket(header_, frames, nullptr);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRetransmissionPacket(uint64_t original_packet_number,
                                              uint64_t new_packet_number,
                                              bool should_include_version) {
  DCHECK(save_packet_frames_);
  InitializeHeader(new_packet_number, should_include_version);
  return MakeMultipleFramesPacket(
      header_, saved_frames_[quic::QuicPacketNumber(original_packet_number)],
      nullptr);
}

void QuicTestPacketMaker::RemoveSavedStreamFrames(
    quic::QuicStreamId stream_id) {
  for (auto& kv : saved_frames_) {
    auto it = kv.second.begin();
    while (it != kv.second.end()) {
      if (it->type == quic::STREAM_FRAME &&
          it->stream_frame.stream_id == stream_id) {
        it = kv.second.erase(it);
      } else {
        ++it;
      }
    }
  }
}

void QuicTestPacketMaker::SetEncryptionLevel(quic::EncryptionLevel level) {
  encryption_level_ = level;
    switch (level) {
      case quic::ENCRYPTION_INITIAL:
        long_header_type_ = quic::INITIAL;
        break;
      case quic::ENCRYPTION_ZERO_RTT:
        long_header_type_ = quic::ZERO_RTT_PROTECTED;
        break;
      case quic::ENCRYPTION_FORWARD_SECURE:
        long_header_type_ = quic::INVALID_PACKET_TYPE;
        break;
      default:
        QUIC_BUG << quic::EncryptionLevelToString(level);
        long_header_type_ = quic::INVALID_PACKET_TYPE;
    }
}

bool QuicTestPacketMaker::ShouldIncludeVersion(bool include_version) const {
  if (version_.transport_version > quic::QUIC_VERSION_43) {
    return encryption_level_ < quic::ENCRYPTION_FORWARD_SECURE;
  }
  return include_version;
}

std::string QuicTestPacketMaker::MaybePrependErrorCode(
    const std::string& quic_error_details,
    quic::QuicErrorCode quic_error_code) const {
  if (!quic::VersionHasIetfQuicFrames(version_.transport_version) ||
      quic_error_code == quic::QUIC_IETF_GQUIC_ERROR_MISSING) {
    // QUIC_IETF_GQUIC_ERROR_MISSING means not to encode the error value.
    return quic_error_details;
  }
  return base::StrCat(
      {base::NumberToString(quic_error_code), ":", quic_error_details});
}

quic::QuicFrame QuicTestPacketMaker::GenerateNextStreamFrame(
    quic::QuicStreamId stream_id,
    bool fin,
    quic::QuicStringPiece data) {
  // Save the stream data so that callers can use temporary objects for data.
  saved_stream_data_.push_back(std::make_unique<std::string>(data));
  data = *saved_stream_data_.back();

  quic::QuicStreamFrame frame(stream_id, fin, stream_offsets_[stream_id], data);
  stream_offsets_[stream_id] += data.length();
  DVLOG(1) << "Adding frame: " << frame;
  return quic::QuicFrame(frame);
}

std::vector<std::string> QuicTestPacketMaker::QpackEncodeHeaders(
    quic::QuicStreamId stream_id,
    spdy::SpdyHeaderBlock headers,
    size_t* encoded_data_length) {
  DCHECK(quic::VersionUsesHttp3(version_.transport_version));
  std::vector<std::string> data;

  std::string encoded_headers =
      qpack_encoder_.EncodeHeaderList(stream_id, headers, nullptr);

  // Generate HEADERS frame header.
  std::unique_ptr<char[]> headers_frame_header;
  const size_t headers_frame_header_length =
      quic::HttpEncoder::SerializeHeadersFrameHeader(encoded_headers.size(),
                                                     &headers_frame_header);

  // Possible add a PUSH stream type.
  if (!quic::QuicUtils::IsBidirectionalStreamId(stream_id) &&
      stream_offsets_[stream_id] == 0) {
    // Push stream type header
    data.push_back("\x01");
  }

  // Add the HEADERS frame header.
  data.push_back(
      std::string(headers_frame_header.get(), headers_frame_header_length));
  // Add the HEADERS frame payload.
  data.push_back(encoded_headers);

  if (coalesce_http_frames_) {
    std::string coalesced;
    for (const auto& d : data) {
      coalesced += d;
    }
    data = {coalesced};
  }

  // Compute the total data length.
  if (encoded_data_length) {
    *encoded_data_length = 0;
    for (const auto& d : data)
      *encoded_data_length += d.length();
  }
  return data;
}

std::vector<quic::QuicFrame> QuicTestPacketMaker::GenerateNextStreamFrames(
    quic::QuicStreamId stream_id,
    bool fin,
    const std::vector<std::string>& data) {
  std::vector<quic::QuicFrame> frames;
  for (size_t i = 0; i < data.size(); ++i) {
    const bool frame_fin = i == data.size() - 1 && fin;
    frames.push_back(GenerateNextStreamFrame(stream_id, frame_fin, data[i]));
  }
  return frames;
}

quic::QuicPacketNumberLength QuicTestPacketMaker::GetPacketNumberLength()
    const {
  if (version_.transport_version > quic::QUIC_VERSION_43 &&
      encryption_level_ < quic::ENCRYPTION_FORWARD_SECURE &&
      !version_.SendsVariableLengthPacketNumberInLongHeader()) {
    return quic::PACKET_4BYTE_PACKET_NUMBER;
  }
  return quic::PACKET_1BYTE_PACKET_NUMBER;
}

quic::QuicConnectionId QuicTestPacketMaker::DestinationConnectionId() const {
  if (perspective_ == quic::Perspective::IS_SERVER) {
    return quic::EmptyQuicConnectionId();
  }
  return connection_id_;
}

quic::QuicConnectionId QuicTestPacketMaker::SourceConnectionId() const {
  if (perspective_ == quic::Perspective::IS_CLIENT) {
    return quic::EmptyQuicConnectionId();
  }
  return connection_id_;
}

quic::QuicConnectionIdIncluded QuicTestPacketMaker::HasDestinationConnectionId()
    const {
  if (!version_.SupportsClientConnectionIds() &&
      perspective_ == quic::Perspective::IS_SERVER) {
    return quic::CONNECTION_ID_ABSENT;
  }
  return quic::CONNECTION_ID_PRESENT;
}

quic::QuicConnectionIdIncluded QuicTestPacketMaker::HasSourceConnectionId()
    const {
  if (version_.SupportsClientConnectionIds() ||
      (perspective_ == quic::Perspective::IS_SERVER &&
       encryption_level_ < quic::ENCRYPTION_FORWARD_SECURE)) {
    return quic::CONNECTION_ID_PRESENT;
  }
  return quic::CONNECTION_ID_ABSENT;
}

void QuicTestPacketMaker::Reset() {
  for (const auto& kv : stream_offsets_)
    stream_offsets_[kv.first] = 0;
}

quic::QuicStreamId QuicTestPacketMaker::GetFirstBidirectionalStreamId() const {
  return quic::QuicUtils::GetFirstBidirectionalStreamId(
      version_.transport_version, perspective_);
}

quic::QuicStreamId QuicTestPacketMaker::GetHeadersStreamId() const {
  return quic::QuicUtils::GetHeadersStreamId(version_.transport_version);
}

std::string QuicTestPacketMaker::GenerateHttp3SettingsData() {
  quic::SettingsFrame settings;
  settings.values[quic::SETTINGS_MAX_HEADER_LIST_SIZE] = kQuicMaxHeaderListSize;
  settings.values[quic::SETTINGS_QPACK_MAX_TABLE_CAPACITY] =
      quic::kDefaultQpackMaxDynamicTableCapacity;
  settings.values[quic::SETTINGS_QPACK_BLOCKED_STREAMS] =
      quic::kDefaultMaximumBlockedStreams;
  std::unique_ptr<char[]> buffer;
  quic::QuicByteCount frame_length =
      quic::HttpEncoder::SerializeSettingsFrame(settings, &buffer);
  return std::string(buffer.get(), frame_length);
}

std::string QuicTestPacketMaker::GenerateHttp3PriorityData(
    spdy::SpdyPriority priority,
    quic::QuicStreamId stream_id) {
  quic::PriorityFrame frame;
  frame.weight = priority;
  frame.dependency_type = quic::ROOT_OF_TREE;
  frame.prioritized_type = quic::REQUEST_STREAM;
  frame.prioritized_element_id = stream_id;

  std::unique_ptr<char[]> buffer;
  quic::QuicByteCount frame_length =
      quic::HttpEncoder::SerializePriorityFrame(frame, &buffer);
  return std::string(buffer.get(), frame_length);
}

void QuicTestPacketMaker::MaybeAddHttp3SettingsFrames(
    quic::QuicFrames* frames) {
  DCHECK(quic::VersionUsesHttp3(version_.transport_version));

  quic::QuicStreamId stream_id =
      quic::QuicUtils::GetFirstUnidirectionalStreamId(
          version_.transport_version, perspective_);

  if (stream_offsets_[stream_id] != 0)
    return;

  // A stream frame containing stream type will be written on the control
  // stream first.
  std::string type(1, 0x00);
  std::string settings_data = GenerateHttp3SettingsData();

  // The type and the SETTINGS frame may be sent in multiple QUIC STREAM
  // frames.
  std::vector<std::string> data;
  if (coalesce_http_frames_) {
    data = {type + settings_data};
  } else {
    data = {type, settings_data};
  }

  for (const auto& frame : GenerateNextStreamFrames(stream_id, false, data))
    frames->push_back(frame);

  if (coalesce_http_frames_) {
    frames->push_back(GenerateNextStreamFrame(stream_id + 4, false, "\x03"));
    frames->push_back(GenerateNextStreamFrame(stream_id + 8, false, "\x02"));
  } else {
    frames->push_back(GenerateNextStreamFrame(stream_id + 8, false, "\x02"));
    frames->push_back(GenerateNextStreamFrame(stream_id + 4, false, "\x03"));
  }
}

}  // namespace test
}  // namespace net
