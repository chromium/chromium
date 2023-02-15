// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_test_packet_maker.h"

#include <list>
#include <utility>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/features.h"
#include "net/quic/mock_crypto_client_stream.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/quic/quic_http_utils.h"
#include "net/third_party/quiche/src/quiche/quic/core/http/http_constants.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_framer.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_stream.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/mock_random.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_test_utils.h"

namespace net::test {
namespace {

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
      case quic::HANDSHAKE_DONE_FRAME:
      case quic::BLOCKED_FRAME:
      case quic::WINDOW_UPDATE_FRAME:
      case quic::STOP_SENDING_FRAME:
      case quic::PATH_CHALLENGE_FRAME:
      case quic::PATH_RESPONSE_FRAME:
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
      case quic::NEW_CONNECTION_ID_FRAME:
        frame.new_connection_id_frame =
            new quic::QuicNewConnectionIdFrame(*frame.new_connection_id_frame);
        break;
      case quic::RETIRE_CONNECTION_ID_FRAME:
        frame.retire_connection_id_frame =
            new quic::QuicRetireConnectionIdFrame(
                *frame.retire_connection_id_frame);
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
      case quic::ACK_FREQUENCY_FRAME:
        frame.ack_frequency_frame =
            new quic::QuicAckFrequencyFrame(*frame.ack_frequency_frame);
        break;

      case quic::NUM_FRAME_TYPES:
        DCHECK(false) << "Cannot clone frame type: " << frame.type;
    }
  }
  return new_frames;
}


}  // namespace

QuicTestPacketMaker::QuicTestPacketMaker(quic::ParsedQuicVersion version,
                                         quic::QuicConnectionId connection_id,
                                         const quic::QuicClock* clock,
                                         const std::string& host,
                                         quic::Perspective perspective,
                                         bool client_priority_uses_incremental)
    : version_(version),
      connection_id_(connection_id),
      clock_(clock),
      host_(host),
      qpack_encoder_(&decoder_stream_error_delegate_),
      perspective_(perspective),
      client_priority_uses_incremental_(client_priority_uses_incremental) {
  DCHECK(!(perspective_ == quic::Perspective::IS_SERVER &&
           client_priority_uses_incremental_));

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

  if (!version_.HasIetfQuicFrames()) {
    AddQuicPingFrame();
  } else if (perspective_ == quic::Perspective::IS_CLIENT) {
    AddQuicPathChallengeFrame();
  } else {
    AddQuicPathResponseFrame();
  }

  AddQuicPaddingFrame();

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::MakePingPacket(
    uint64_t num,
    bool include_version) {
  InitializeHeader(num, include_version);
  AddQuicPingFrame();
  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRetireConnectionIdPacket(uint64_t num,
                                                  bool include_version,
                                                  uint64_t sequence_number) {
  InitializeHeader(num, include_version);
  AddQuicRetireConnectionIdFrame(sequence_number);
  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeNewConnectionIdPacket(
    uint64_t num,
    bool include_version,
    const quic::QuicConnectionId& cid,
    uint64_t sequence_number,
    uint64_t retire_prior_to) {
  InitializeHeader(num, include_version);
  AddQuicNewConnectionIdFrame(
      cid, sequence_number, retire_prior_to,
      quic::QuicUtils::GenerateStatelessResetToken(cid));
  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeAckAndNewConnectionIdPacket(
    uint64_t num,
    bool include_version,
    uint64_t largest_received,
    uint64_t smallest_received,
    const quic::QuicConnectionId& cid,
    uint64_t sequence_number,
    uint64_t retire_prior_to) {
  InitializeHeader(num, include_version);
  AddQuicAckFrame(largest_received, smallest_received);
  AddQuicNewConnectionIdFrame(
      cid, sequence_number, retire_prior_to,
      quic::QuicUtils::GenerateStatelessResetToken(cid));
  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeDummyCHLOPacket(uint64_t packet_num) {
  SetEncryptionLevel(quic::ENCRYPTION_INITIAL);
  InitializeHeader(packet_num, /*include_version=*/true);

  quic::CryptoHandshakeMessage message =
      MockCryptoClientStream::GetDummyCHLOMessage();
  const quic::QuicData& data = message.GetSerialized();

  if (!QuicVersionUsesCryptoFrames(version_.transport_version)) {
    AddQuicStreamFrameWithOffset(
        quic::QuicUtils::GetCryptoStreamId(version_.transport_version),
        /*fin=*/false, /*offset=*/0, data.AsStringPiece());
  } else {
    AddQuicCryptoFrame(quic::ENCRYPTION_INITIAL, 0, data.length());

    data_producer_ = std::make_unique<quic::test::SimpleDataProducer>();
    data_producer_->SaveCryptoData(quic::ENCRYPTION_INITIAL, 0,
                                   data.AsStringPiece());
  }
  AddQuicPaddingFrame();

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeAckAndPingPacket(uint64_t num,
                                          bool include_version,
                                          uint64_t largest_received,
                                          uint64_t smallest_received) {
  InitializeHeader(num, include_version);
  AddQuicAckFrame(largest_received, smallest_received);
  AddQuicPingFrame();
  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeAckAndRetireConnectionIdPacket(
    uint64_t num,
    bool include_version,
    uint64_t largest_received,
    uint64_t smallest_received,
    uint64_t sequence_number) {
  InitializeHeader(num, include_version);
  AddQuicAckFrame(largest_received, smallest_received);
  AddQuicRetireConnectionIdFrame(sequence_number);
  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRetransmissionAndRetireConnectionIdPacket(
    uint64_t num,
    bool include_version,
    const std::vector<uint64_t>& original_packet_numbers,
    uint64_t sequence_number) {
  InitializeHeader(num, include_version);
  for (auto it : original_packet_numbers) {
    for (auto frame : saved_frames_[quic::QuicPacketNumber(it)]) {
      if (!MaybeCoalesceStreamFrame(frame)) {
        frames_.push_back(frame);
      }
    }
  }
  AddQuicRetireConnectionIdFrame(sequence_number);
  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeStreamsBlockedPacket(
    uint64_t num,
    bool include_version,
    quic::QuicStreamCount stream_count,
    bool unidirectional) {
  InitializeHeader(num, include_version);
  AddQuicStreamsBlockedFrame(1, stream_count, unidirectional);
  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeMaxStreamsPacket(uint64_t num,
                                          bool include_version,
                                          quic::QuicStreamCount stream_count,
                                          bool unidirectional) {
  InitializeHeader(num, include_version);
  AddQuicMaxStreamsFrame(1, stream_count, unidirectional);
  return BuildPacket();
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

  if (include_stop_sending_if_v99 && version_.HasIetfQuicFrames()) {
    AddQuicStopSendingFrame(stream_id, error_code);
  }
  if (!version_.HasIetfQuicFrames() ||
      quic::QuicUtils::IsBidirectionalStreamId(stream_id, version_)) {
    AddQuicRstStreamFrame(stream_id, error_code);
  }

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRstAndDataPacket(
    uint64_t num,
    bool include_version,
    quic::QuicStreamId rst_stream_id,
    quic::QuicRstStreamErrorCode rst_error_code,
    quic::QuicStreamId data_stream_id,
    absl::string_view data) {
  InitializeHeader(num, include_version);

  if (version_.HasIetfQuicFrames()) {
    AddQuicStopSendingFrame(rst_stream_id, rst_error_code);
  }
  AddQuicRstStreamFrame(rst_stream_id, rst_error_code);

  AddQuicStreamFrame(data_stream_id, /* fin = */ false, data);

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRetransmissionRstAndDataPacket(
    const std::vector<uint64_t>& original_packet_numbers,
    uint64_t num,
    bool include_version,
    quic::QuicStreamId rst_stream_id,
    quic::QuicRstStreamErrorCode rst_error_code,
    quic::QuicStreamId data_stream_id,
    absl::string_view data,
    uint64_t retransmit_frame_count) {
  DCHECK(save_packet_frames_);
  InitializeHeader(num, include_version);

  uint64_t frame_count = 0;
  for (auto it : original_packet_numbers) {
    for (auto frame : saved_frames_[quic::QuicPacketNumber(it)]) {
      frame_count++;
      if (retransmit_frame_count == 0 ||
          frame_count <= retransmit_frame_count) {
        if (!MaybeCoalesceStreamFrame(frame)) {
          frames_.push_back(frame);
        }
      }
    }
  }

  if (version_.HasIetfQuicFrames()) {
    AddQuicStopSendingFrame(rst_stream_id, rst_error_code);
  }
  AddQuicRstStreamFrame(rst_stream_id, rst_error_code);

  AddQuicStreamFrame(data_stream_id, /* fin = */ false, data);

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeDataAndRstPacket(
    uint64_t num,
    bool include_version,
    quic::QuicStreamId data_stream_id,
    absl::string_view data,
    quic::QuicStreamId rst_stream_id,
    quic::QuicRstStreamErrorCode rst_error_code) {
  InitializeHeader(num, include_version);

  AddQuicStreamFrame(data_stream_id, /* fin = */ false, data);
  if (version_.HasIetfQuicFrames()) {
    AddQuicStopSendingFrame(rst_stream_id, rst_error_code);
  }
  AddQuicRstStreamFrame(rst_stream_id, rst_error_code);

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeDataRstAndAckPacket(
    uint64_t num,
    bool include_version,
    quic::QuicStreamId data_stream_id,
    absl::string_view data,
    quic::QuicStreamId rst_stream_id,
    quic::QuicRstStreamErrorCode rst_error_code,
    uint64_t largest_received,
    uint64_t smallest_received) {
  InitializeHeader(num, include_version);

  AddQuicAckFrame(largest_received, smallest_received);

  AddQuicStreamFrame(data_stream_id, /* fin = */ false, data);
  if (version_.HasIetfQuicFrames()) {
    AddQuicStopSendingFrame(rst_stream_id, rst_error_code);
  }
  AddQuicRstStreamFrame(rst_stream_id, rst_error_code);

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeAckAndRstPacket(
    uint64_t num,
    bool include_version,
    quic::QuicStreamId stream_id,
    quic::QuicRstStreamErrorCode error_code,
    uint64_t largest_received,
    uint64_t smallest_received) {
  return MakeAckAndRstPacket(num, include_version, stream_id, error_code,
                             largest_received, smallest_received,
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
    bool include_stop_sending_if_v99) {
  InitializeHeader(num, include_version);

  AddQuicAckFrame(largest_received, smallest_received);

  if (version_.HasIetfQuicFrames() && include_stop_sending_if_v99) {
    AddQuicStopSendingFrame(stream_id, error_code);
  }
  if (!version_.HasIetfQuicFrames() ||
      quic::QuicUtils::IsBidirectionalStreamId(stream_id, version_)) {
    AddQuicRstStreamFrame(stream_id, error_code);
  }

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRstAckAndConnectionClosePacket(
    uint64_t num,
    bool include_version,
    quic::QuicStreamId stream_id,
    quic::QuicRstStreamErrorCode error_code,
    uint64_t largest_received,
    uint64_t smallest_received,
    quic::QuicErrorCode quic_error,
    const std::string& quic_error_details) {
  InitializeHeader(num, include_version);

    AddQuicAckFrame(largest_received, smallest_received);

  if (version_.HasIetfQuicFrames()) {
    AddQuicStopSendingFrame(stream_id, error_code);
  }
  AddQuicRstStreamFrame(stream_id, error_code);
  AddQuicConnectionCloseFrame(quic_error, quic_error_details);

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRstAckAndDataPacket(
    uint64_t num,
    bool include_version,
    quic::QuicStreamId stream_id,
    quic::QuicRstStreamErrorCode error_code,
    uint64_t largest_received,
    uint64_t smallest_received,
    quic::QuicStreamId data_id,
    bool fin,
    absl::string_view data) {
  InitializeHeader(num, include_version);

  AddQuicRstStreamFrame(stream_id, error_code);

  AddQuicAckFrame(largest_received, smallest_received);
  AddQuicStreamFrame(data_id, fin, data);
  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeAckDataAndRst(uint64_t num,
                                       bool include_version,
                                       quic::QuicStreamId stream_id,
                                       quic::QuicRstStreamErrorCode error_code,
                                       uint64_t largest_received,
                                       uint64_t smallest_received,
                                       quic::QuicStreamId data_id,
                                       bool fin,
                                       absl::string_view data) {
  InitializeHeader(num, include_version);

  AddQuicAckFrame(largest_received, smallest_received);
  AddQuicStreamFrame(data_id, fin, data);

  AddQuicStopSendingFrame(stream_id, error_code);
  AddQuicRstStreamFrame(stream_id, error_code);

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeAckRstAndDataPacket(
    uint64_t num,
    bool include_version,
    quic::QuicStreamId stream_id,
    quic::QuicRstStreamErrorCode error_code,
    uint64_t largest_received,
    uint64_t smallest_received,
    quic::QuicStreamId data_id,
    bool fin,
    absl::string_view data) {
  InitializeHeader(num, include_version);

  AddQuicAckFrame(largest_received, smallest_received);
  AddQuicRstStreamFrame(stream_id, error_code);
  AddQuicStreamFrame(data_id, fin, data);
  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeAckAndRetransmissionPacket(
    uint64_t packet_number,
    uint64_t first_received,
    uint64_t largest_received,
    uint64_t smallest_received,
    const std::vector<uint64_t>& original_packet_numbers) {
  DCHECK(save_packet_frames_);
  InitializeHeader(packet_number, /*include_version=*/false);
  AddQuicAckFrame(first_received, largest_received, smallest_received);
  for (auto it : original_packet_numbers) {
    for (auto frame : saved_frames_[quic::QuicPacketNumber(it)]) {
      if (!MaybeCoalesceStreamFrame(frame)) {
        frames_.push_back(frame);
      }
    }
  }

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeCombinedRetransmissionPacket(
    const std::vector<uint64_t>& original_packet_numbers,
    uint64_t new_packet_number,
    bool should_include_version) {
  DCHECK(save_packet_frames_);
  InitializeHeader(new_packet_number, should_include_version);
  for (auto it : original_packet_numbers) {
    for (auto& frame : CloneFrames(saved_frames_[quic::QuicPacketNumber(it)])) {
      if (frame.type != quic::PADDING_FRAME) {
        if (!MaybeCoalesceStreamFrame(frame)) {
          frames_.push_back(frame);
        }
      }
    }
  }

  return BuildPacket();
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

  if (version_.HasIetfQuicFrames()) {
    AddQuicStopSendingFrame(stream_id, error_code);
  }
  AddQuicRstStreamFrame(stream_id, error_code);

  AddQuicConnectionCloseFrame(quic_error, quic_error_details);

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeDataRstAndConnectionClosePacket(
    uint64_t num,
    bool include_version,
    quic::QuicStreamId data_stream_id,
    absl::string_view data,
    quic::QuicStreamId rst_stream_id,
    quic::QuicRstStreamErrorCode error_code,
    quic::QuicErrorCode quic_error,
    const std::string& quic_error_details) {
  InitializeHeader(num, include_version);

  AddQuicStreamFrame(data_stream_id, /* fin = */ false, data);
  if (version_.HasIetfQuicFrames()) {
    AddQuicStopSendingFrame(rst_stream_id, error_code);
  }
  AddQuicRstStreamFrame(rst_stream_id, error_code);

  AddQuicConnectionCloseFrame(quic_error, quic_error_details);

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeDataRstAckAndConnectionClosePacket(
    uint64_t num,
    bool include_version,
    quic::QuicStreamId data_stream_id,
    absl::string_view data,
    quic::QuicStreamId rst_stream_id,
    quic::QuicRstStreamErrorCode error_code,
    uint64_t largest_received,
    uint64_t smallest_received,
    quic::QuicErrorCode quic_error,
    const std::string& quic_error_details) {
  InitializeHeader(num, include_version);

  AddQuicAckFrame(largest_received, smallest_received);

  AddQuicStreamFrame(data_stream_id, /* fin = */ false, data);
  if (version_.HasIetfQuicFrames()) {
    AddQuicStopSendingFrame(rst_stream_id, error_code);
  }
  AddQuicRstStreamFrame(rst_stream_id, error_code);

  AddQuicConnectionCloseFrame(quic_error, quic_error_details);

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeDataRstAckAndConnectionClosePacket(
    uint64_t num,
    bool include_version,
    quic::QuicStreamId data_stream_id,
    absl::string_view data,
    quic::QuicStreamId rst_stream_id,
    quic::QuicRstStreamErrorCode error_code,
    uint64_t largest_received,
    uint64_t smallest_received,
    quic::QuicErrorCode quic_error,
    const std::string& quic_error_details,
    uint64_t frame_type) {
  InitializeHeader(num, include_version);

  AddQuicStreamFrame(data_stream_id, /* fin = */ false, data);
  if (version_.HasIetfQuicFrames()) {
    AddQuicStopSendingFrame(rst_stream_id, error_code);
  }
  AddQuicRstStreamFrame(rst_stream_id, error_code);

  AddQuicAckFrame(largest_received, smallest_received);
  AddQuicConnectionCloseFrame(quic_error, quic_error_details, frame_type);

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeStopSendingPacket(
    uint64_t num,
    bool include_version,
    quic::QuicStreamId stream_id,
    quic::QuicRstStreamErrorCode error_code) {
  DCHECK(version_.HasIetfQuicFrames());

  InitializeHeader(num, include_version);
  AddQuicStopSendingFrame(stream_id, error_code);

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeAckAndConnectionClosePacket(
    uint64_t num,
    bool include_version,
    uint64_t largest_received,
    uint64_t smallest_received,
    quic::QuicErrorCode quic_error,
    const std::string& quic_error_details,
    uint64_t frame_type) {
  InitializeHeader(num, include_version);
  AddQuicAckFrame(largest_received, smallest_received);
  AddQuicConnectionCloseFrame(quic_error, quic_error_details, frame_type);
  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeConnectionClosePacket(
    uint64_t num,
    bool include_version,
    quic::QuicErrorCode quic_error,
    const std::string& quic_error_details) {
  InitializeHeader(num, include_version);
  AddQuicConnectionCloseFrame(quic_error, quic_error_details);
  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::MakeGoAwayPacket(
    uint64_t num,
    quic::QuicErrorCode error_code,
    std::string reason_phrase) {
  InitializeHeader(num, /*include_version=*/false);
  AddQuicGoAwayFrame(error_code, reason_phrase);
  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::MakeAckPacket(
    uint64_t packet_number,
    uint64_t largest_received,
    uint64_t smallest_received) {
  return MakeAckPacket(packet_number, 1, largest_received, smallest_received);
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::MakeAckPacket(
    uint64_t packet_number,
    uint64_t first_received,
    uint64_t largest_received,
    uint64_t smallest_received) {
  InitializeHeader(packet_number, /*include_version=*/false);
  AddQuicAckFrame(first_received, largest_received, smallest_received);
  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::MakeDataPacket(
    uint64_t packet_number,
    quic::QuicStreamId stream_id,
    bool should_include_version,
    bool fin,
    absl::string_view data) {
  InitializeHeader(packet_number, should_include_version);
  AddQuicStreamFrame(stream_id, fin, data);
  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeAckAndDataPacket(uint64_t packet_number,
                                          bool include_version,
                                          quic::QuicStreamId stream_id,
                                          uint64_t largest_received,
                                          uint64_t smallest_received,
                                          bool fin,
                                          absl::string_view data) {
  InitializeHeader(packet_number, include_version);

  AddQuicAckFrame(largest_received, smallest_received);
  AddQuicStreamFrame(stream_id, fin, data);

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeAckRetransmissionAndDataPacket(
    uint64_t packet_number,
    bool include_version,
    const std::vector<uint64_t>& original_packet_numbers,
    quic::QuicStreamId stream_id,
    uint64_t largest_received,
    uint64_t smallest_received,
    bool fin,
    absl::string_view data) {
  InitializeHeader(packet_number, include_version);

  AddQuicAckFrame(largest_received, smallest_received);
  for (auto it : original_packet_numbers) {
    for (auto frame : saved_frames_[quic::QuicPacketNumber(it)]) {
      if (!MaybeCoalesceStreamFrame(frame)) {
        frames_.push_back(frame);
      }
    }
  }
  AddQuicStreamFrame(stream_id, fin, data);

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRequestHeadersAndMultipleDataFramesPacket(
    uint64_t packet_number,
    quic::QuicStreamId stream_id,
    bool should_include_version,
    bool fin,
    spdy::SpdyPriority spdy_priority,
    spdy::Http2HeaderBlock headers,
    size_t* spdy_headers_frame_length,
    const std::vector<std::string>& data_writes) {
  InitializeHeader(packet_number, should_include_version);

  MaybeAddHttp3SettingsFrames();

  std::string priority_data =
      GenerateHttp3PriorityData(spdy_priority, stream_id);
  if (!priority_data.empty()) {
    AddQuicStreamFrame(2, false, priority_data);
  }

  std::string data = QpackEncodeHeaders(stream_id, std::move(headers),
                                        spdy_headers_frame_length);
  for (const auto& data_write : data_writes) {
    data += data_write;
  }
  AddQuicStreamFrame(stream_id, fin, data);

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRequestHeadersPacket(
    uint64_t packet_number,
    quic::QuicStreamId stream_id,
    bool should_include_version,
    bool fin,
    spdy::SpdyPriority spdy_priority,
    spdy::Http2HeaderBlock headers,
    size_t* spdy_headers_frame_length,
    bool should_include_priority_frame) {
  InitializeHeader(packet_number, should_include_version);

  MaybeAddHttp3SettingsFrames();

  if (should_include_priority_frame) {
    std::string priority_data =
        GenerateHttp3PriorityData(spdy_priority, stream_id);
    if (!priority_data.empty()) {
      AddQuicStreamFrame(2, false, priority_data);
    }
  }

  std::string data = QpackEncodeHeaders(stream_id, std::move(headers),
                                        spdy_headers_frame_length);
  AddQuicStreamFrame(stream_id, fin, data);

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRetransmissionAndRequestHeadersPacket(
    const std::vector<uint64_t>& original_packet_numbers,
    uint64_t packet_number,
    quic::QuicStreamId stream_id,
    bool should_include_version,
    bool fin,
    spdy::SpdyPriority spdy_priority,
    spdy::Http2HeaderBlock headers,
    size_t* spdy_headers_frame_length) {
  DCHECK(save_packet_frames_);
  InitializeHeader(packet_number, should_include_version);

  for (auto it : original_packet_numbers) {
    for (auto frame : saved_frames_[quic::QuicPacketNumber(it)]) {
      if (!MaybeCoalesceStreamFrame(frame)) {
        frames_.push_back(frame);
      }
    }
  }

  MaybeAddHttp3SettingsFrames();

  std::string priority_data =
      GenerateHttp3PriorityData(spdy_priority, stream_id);
  if (!priority_data.empty()) {
    AddQuicStreamFrame(2, false, priority_data);
  }

  std::string data = QpackEncodeHeaders(stream_id, std::move(headers),
                                        spdy_headers_frame_length);
  AddQuicStreamFrame(stream_id, fin, data);

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRequestHeadersAndRstPacket(
    uint64_t packet_number,
    quic::QuicStreamId stream_id,
    bool should_include_version,
    bool fin,
    spdy::SpdyPriority spdy_priority,
    spdy::Http2HeaderBlock headers,
    size_t* spdy_headers_frame_length,
    quic::QuicRstStreamErrorCode error_code) {
  InitializeHeader(packet_number, should_include_version);

  MaybeAddHttp3SettingsFrames();

  std::string priority_data =
      GenerateHttp3PriorityData(spdy_priority, stream_id);
  if (!priority_data.empty()) {
    AddQuicStreamFrame(2, false, priority_data);
  }

  std::string data = QpackEncodeHeaders(stream_id, std::move(headers),
                                        spdy_headers_frame_length);
  AddQuicStreamFrame(stream_id, fin, data);
  AddQuicStopSendingFrame(stream_id, error_code);
  AddQuicRstStreamFrame(stream_id, error_code);

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeResponseHeadersPacket(
    uint64_t packet_number,
    quic::QuicStreamId stream_id,
    bool should_include_version,
    bool fin,
    spdy::Http2HeaderBlock headers,
    size_t* spdy_headers_frame_length) {
  InitializeHeader(packet_number, should_include_version);

  std::string data = QpackEncodeHeaders(stream_id, std::move(headers),
                                        spdy_headers_frame_length);

  AddQuicStreamFrame(stream_id, fin, data);

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeInitialSettingsPacket(uint64_t packet_number) {
  InitializeHeader(packet_number, /*should_include_version*/ true);
  MaybeAddHttp3SettingsFrames();
  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakePriorityPacket(uint64_t packet_number,
                                        bool should_include_version,
                                        quic::QuicStreamId id,
                                        spdy::SpdyPriority spdy_priority) {
  InitializeHeader(packet_number, should_include_version);

  std::string priority_data = GenerateHttp3PriorityData(spdy_priority, id);
  if (!priority_data.empty()) {
    AddQuicStreamFrame(2, false, priority_data);
  }

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeAckAndPriorityPacket(
    uint64_t packet_number,
    bool should_include_version,
    uint64_t largest_received,
    uint64_t smallest_received,
    quic::QuicStreamId id,
    spdy::SpdyPriority spdy_priority) {
  InitializeHeader(packet_number, should_include_version);

  AddQuicAckFrame(largest_received, smallest_received);

  std::string priority_data = GenerateHttp3PriorityData(spdy_priority, id);
  if (!priority_data.empty()) {
    AddQuicStreamFrame(2, false, priority_data);
  }

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeAckAndPriorityUpdatePacket(
    uint64_t packet_number,
    bool should_include_version,
    uint64_t largest_received,
    uint64_t smallest_received,
    quic::QuicStreamId id,
    spdy::SpdyPriority spdy_priority) {
  InitializeHeader(packet_number, should_include_version);

  AddQuicAckFrame(largest_received, smallest_received);

  std::string priority_data = GenerateHttp3PriorityData(spdy_priority, id);
  if (!priority_data.empty()) {
    AddQuicStreamFrame(2, false, priority_data);
  }

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRetransmissionPacket(uint64_t original_packet_number,
                                              uint64_t new_packet_number,
                                              bool should_include_version) {
  DCHECK(save_packet_frames_);
  InitializeHeader(new_packet_number, should_include_version);
  return BuildPacketImpl(
      saved_frames_[quic::QuicPacketNumber(original_packet_number)], nullptr);
}

std::unique_ptr<quic::QuicEncryptedPacket>
QuicTestPacketMaker::MakeStatelessResetPacket() {
  auto connection_id = quic::test::TestConnectionId();
  return quic::QuicFramer::BuildIetfStatelessResetPacket(
      connection_id, quic::QuicFramer::GetMinStatelessResetPacketLength() + 1,
      quic::QuicUtils::GenerateStatelessResetToken(connection_id));
}

void QuicTestPacketMaker::RemoveSavedStreamFrames(
    quic::QuicStreamId stream_id) {
  for (auto& kv : saved_frames_) {
    auto* it = kv.second.begin();
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
      LOG(DFATAL) << quic::EncryptionLevelToString(level);
      long_header_type_ = quic::INVALID_PACKET_TYPE;
  }
}

spdy::Http2HeaderBlock QuicTestPacketMaker::GetRequestHeaders(
    const std::string& method,
    const std::string& scheme,
    const std::string& path) const {
  spdy::Http2HeaderBlock headers;
  headers[":method"] = method;
  headers[":authority"] = host_;
  headers[":scheme"] = scheme;
  headers[":path"] = path;
  return headers;
}

spdy::Http2HeaderBlock QuicTestPacketMaker::ConnectRequestHeaders(
    const std::string& host_port) const {
  spdy::Http2HeaderBlock headers;
  headers[":method"] = "CONNECT";
  headers[":authority"] = host_port;
  return headers;
}

spdy::Http2HeaderBlock QuicTestPacketMaker::GetResponseHeaders(
    const std::string& status) const {
  spdy::Http2HeaderBlock headers;
  headers[":status"] = status;
  headers["content-type"] = "text/plain";
  return headers;
}

spdy::Http2HeaderBlock QuicTestPacketMaker::GetResponseHeaders(
    const std::string& status,
    const std::string& alt_svc) const {
  spdy::Http2HeaderBlock headers;
  headers[":status"] = status;
  headers["alt-svc"] = alt_svc;
  headers["content-type"] = "text/plain";
  return headers;
}

void QuicTestPacketMaker::Reset() {
  stream_offsets_.clear();
}

std::string QuicTestPacketMaker::QpackEncodeHeaders(
    quic::QuicStreamId stream_id,
    spdy::Http2HeaderBlock headers,
    size_t* encoded_data_length) {
  std::string data;

  std::string encoded_headers =
      qpack_encoder_.EncodeHeaderList(stream_id, headers, nullptr);

  // Generate HEADERS frame header.
  const std::string headers_frame_header =
      quic::HttpEncoder::SerializeHeadersFrameHeader(encoded_headers.size());

  // Possible add a PUSH stream type.
  if (!quic::QuicUtils::IsBidirectionalStreamId(stream_id, version_) &&
      stream_offsets_[stream_id] == 0) {
    // Push stream type header
    data += "\x01";
  }

  // Add the HEADERS frame header.
  data += headers_frame_header;
  // Add the HEADERS frame payload.
  data += encoded_headers;

  // Compute the total data length.
  if (encoded_data_length) {
    *encoded_data_length = data.length();
  }
  return data;
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
          quiche::VARIABLE_LENGTH_INTEGER_LENGTH_1;
    }
    header_.length_length = quiche::VARIABLE_LENGTH_INTEGER_LENGTH_2;
  }
}

void QuicTestPacketMaker::AddQuicPaddingFrame() {
  quic::QuicPaddingFrame padding_frame;
  frames_.push_back(quic::QuicFrame(padding_frame));
  DVLOG(1) << "Adding frame: " << frames_.back();
}

void QuicTestPacketMaker::AddQuicPingFrame() {
  quic::QuicPingFrame ping_frame;
  frames_.push_back(quic::QuicFrame(ping_frame));
  DVLOG(1) << "Adding frame: " << frames_.back();
}

void QuicTestPacketMaker::AddQuicRetireConnectionIdFrame(
    uint64_t sequence_number) {
  auto* retire_cid_frame = new quic::QuicRetireConnectionIdFrame();
  retire_cid_frame->sequence_number = sequence_number;
  frames_.push_back(quic::QuicFrame(retire_cid_frame));
  DVLOG(1) << "Adding frame: " << frames_.back();
}

void QuicTestPacketMaker::AddQuicNewConnectionIdFrame(
    const quic::QuicConnectionId& cid,
    uint64_t sequence_number,
    uint64_t retire_prior_to,
    quic::StatelessResetToken reset_token) {
  auto* new_cid_frame = new quic::QuicNewConnectionIdFrame();
  new_cid_frame->connection_id = cid;
  new_cid_frame->sequence_number = sequence_number;
  new_cid_frame->retire_prior_to = retire_prior_to;
  new_cid_frame->stateless_reset_token = reset_token;
  frames_.push_back(quic::QuicFrame(new_cid_frame));
  DVLOG(1) << "Adding frame: " << frames_.back();
}

void QuicTestPacketMaker::AddQuicMaxStreamsFrame(
    quic::QuicControlFrameId control_frame_id,
    quic::QuicStreamCount stream_count,
    bool unidirectional) {
  quic::QuicMaxStreamsFrame max_streams_frame(control_frame_id, stream_count,
                                              unidirectional);
  frames_.push_back(quic::QuicFrame(max_streams_frame));
  DVLOG(1) << "Adding frame: " << frames_.back();
}

void QuicTestPacketMaker::AddQuicStreamsBlockedFrame(
    quic::QuicControlFrameId control_frame_id,
    quic::QuicStreamCount stream_count,
    bool unidirectional) {
  quic::QuicStreamsBlockedFrame streams_blocked_frame(
      control_frame_id, stream_count, unidirectional);
  frames_.push_back(quic::QuicFrame(streams_blocked_frame));
  DVLOG(1) << "Adding frame: " << frames_.back();
}

void QuicTestPacketMaker::AddQuicStreamFrame(quic::QuicStreamId stream_id,
                                             bool fin,
                                             absl::string_view data) {
  AddQuicStreamFrameWithOffset(stream_id, fin, stream_offsets_[stream_id],
                               data);
  stream_offsets_[stream_id] += data.length();
}

void QuicTestPacketMaker::AddQuicStreamFrameWithOffset(
    quic::QuicStreamId stream_id,
    bool fin,
    quic::QuicStreamOffset offset,
    absl::string_view data) {
  // Save the stream data so that callers can use temporary objects for data.
  saved_stream_data_.push_back(std::make_unique<std::string>(data));
  absl::string_view saved_data = *saved_stream_data_.back();

  quic::QuicStreamFrame stream_frame(stream_id, fin, offset, saved_data);
  frames_.push_back(quic::QuicFrame(stream_frame));
  DVLOG(1) << "Adding frame: " << frames_.back();
}

void QuicTestPacketMaker::AddQuicAckFrame(uint64_t largest_received,
                                          uint64_t smallest_received) {
  AddQuicAckFrame(1, largest_received, smallest_received);
}

void QuicTestPacketMaker::AddQuicAckFrame(uint64_t first_received,
                                          uint64_t largest_received,
                                          uint64_t smallest_received) {
  auto* ack_frame = new quic::QuicAckFrame;
  ack_frame->largest_acked = quic::QuicPacketNumber(largest_received);
  ack_frame->ack_delay_time = quic::QuicTime::Delta::Zero();
  for (uint64_t i = smallest_received; i <= largest_received; ++i) {
    ack_frame->received_packet_times.push_back(
        std::make_pair(quic::QuicPacketNumber(i), clock_->Now()));
  }
  if (largest_received > 0) {
    DCHECK_GE(largest_received, first_received);
    ack_frame->packets.AddRange(quic::QuicPacketNumber(first_received),
                                quic::QuicPacketNumber(largest_received + 1));
  }
  frames_.push_back(quic::QuicFrame(ack_frame));
  DVLOG(1) << "Adding frame: " << frames_.back();
}

void QuicTestPacketMaker::AddQuicRstStreamFrame(
    quic::QuicStreamId stream_id,
    quic::QuicRstStreamErrorCode error_code) {
  auto* rst_stream_frame = new quic::QuicRstStreamFrame(
      1, stream_id, error_code, stream_offsets_[stream_id]);
  frames_.push_back(quic::QuicFrame(rst_stream_frame));
  DVLOG(1) << "Adding frame: " << frames_.back();
}

void QuicTestPacketMaker::AddQuicConnectionCloseFrame(
    quic::QuicErrorCode quic_error,
    const std::string& quic_error_details) {
  AddQuicConnectionCloseFrame(quic_error, quic_error_details, 0);
}

void QuicTestPacketMaker::AddQuicConnectionCloseFrame(
    quic::QuicErrorCode quic_error,
    const std::string& quic_error_details,
    uint64_t frame_type) {
  auto* close_frame = new quic::QuicConnectionCloseFrame(
      version_.transport_version, quic_error, quic::NO_IETF_QUIC_ERROR,
      quic_error_details, frame_type);
  frames_.push_back(quic::QuicFrame(close_frame));
  DVLOG(1) << "Adding frame: " << frames_.back();
}

void QuicTestPacketMaker::AddQuicGoAwayFrame(quic::QuicErrorCode error_code,
                                             std::string reason_phrase) {
  auto* goaway_frame = new quic::QuicGoAwayFrame();
  goaway_frame->error_code = error_code;
  goaway_frame->last_good_stream_id = 0;
  goaway_frame->reason_phrase = reason_phrase;
  frames_.push_back(quic::QuicFrame(goaway_frame));
  DVLOG(1) << "Adding frame: " << frames_.back();
}

void QuicTestPacketMaker::AddQuicPathResponseFrame() {
  quic::test::MockRandom rand(0);
  quic::QuicPathFrameBuffer payload;
  rand.RandBytes(payload.data(), payload.size());
  auto path_response_frame = quic::QuicPathResponseFrame(0, payload);
  frames_.push_back(quic::QuicFrame(path_response_frame));
  DVLOG(1) << "Adding frame: " << frames_.back();
}

void QuicTestPacketMaker::AddQuicPathChallengeFrame() {
  quic::test::MockRandom rand(0);
  quic::QuicPathFrameBuffer payload;
  rand.RandBytes(payload.data(), payload.size());
  auto path_challenge_frame = quic::QuicPathChallengeFrame(0, payload);
  frames_.push_back(quic::QuicFrame(path_challenge_frame));
  DVLOG(1) << "Adding frame: " << frames_.back();
}

void QuicTestPacketMaker::AddQuicStopSendingFrame(
    quic::QuicStreamId stream_id,
    quic::QuicRstStreamErrorCode error_code) {
  auto stop_sending_frame =
      quic::QuicStopSendingFrame(1, stream_id, error_code);
  frames_.push_back(quic::QuicFrame(stop_sending_frame));
  DVLOG(1) << "Adding frame: " << frames_.back();
}

void QuicTestPacketMaker::AddQuicCryptoFrame(
    quic::EncryptionLevel level,
    quic::QuicStreamOffset offset,
    quic::QuicPacketLength data_length) {
  auto* crypto_frame = new quic::QuicCryptoFrame(level, offset, data_length);
  frames_.push_back(quic::QuicFrame(crypto_frame));
  DVLOG(1) << "Adding frame: " << frames_.back();
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::BuildPacket() {
  auto packet = BuildPacketImpl(frames_, data_producer_.get());

  DeleteFrames(&frames_);
  data_producer_.reset();

  return packet;
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::BuildPacketImpl(
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
    framer.SetEncrypter(
        encryption_level_,
        std::make_unique<quic::test::TaggingEncrypter>(  // IN-TEST
            encryption_level_));
  }
  if (data_producer != nullptr) {
    framer.set_data_producer(data_producer);
  }
  quic::QuicFrames frames_copy = CloneFrames(frames);
  size_t max_plaintext_size =
      framer.GetMaxPlaintextSize(quic::kDefaultMaxPacketSize);
  if (version_.HasHeaderProtection()) {
    size_t packet_size =
        quic::GetPacketHeaderSize(version_.transport_version, header_);
    size_t frames_size = 0;
    for (size_t i = 0; i < frames.size(); ++i) {
      bool first_frame = i == 0;
      bool last_frame = i == frames.size() - 1;
      const size_t frame_size = framer.GetSerializedFrameLength(
          frames[i], max_plaintext_size - packet_size, first_frame, last_frame,
          header_.packet_number_length);
      packet_size += frame_size;
      frames_size += frame_size;
    }

    const size_t min_plaintext_packet_size =
        quic::QuicPacketCreator::MinPlaintextPacketSize(
            version_, header_.packet_number_length);
    if (frames_size < min_plaintext_packet_size) {
      if (GetQuicRestartFlag(quic_allow_smaller_packets)) {
        frames_copy.insert(frames_copy.begin(),
                           quic::QuicFrame(quic::QuicPaddingFrame(
                               min_plaintext_packet_size - frames_size)));
      } else {
        const size_t expansion_on_new_frame =
            frames.empty()
                ? 0
                : quic::QuicPacketCreator::ExpansionOnNewFrameWithLastFrame(
                      frames.back(), version_.transport_version);
        const size_t padding_length =
            std::max(1 + expansion_on_new_frame,
                     min_plaintext_packet_size - frames_size) -
            expansion_on_new_frame;
        CHECK_LE(padding_length + packet_size + expansion_on_new_frame,
                 max_plaintext_size);
        frames_copy.push_back(
            quic::QuicFrame(quic::QuicPaddingFrame(padding_length)));
      }
    }
  }
  std::unique_ptr<quic::QuicPacket> packet(quic::test::BuildUnsizedDataPacket(
      &framer, header_, frames_copy, max_plaintext_size));
  char buffer[quic::kMaxOutgoingPacketSize];
  size_t encrypted_size =
      framer.EncryptPayload(encryption_level_, header_.packet_number, *packet,
                            buffer, quic::kMaxOutgoingPacketSize);
  EXPECT_NE(0u, encrypted_size);
  quic::QuicReceivedPacket encrypted(buffer, encrypted_size, clock_->Now(),
                                     false);
  if (save_packet_frames_) {
    saved_frames_[header_.packet_number] = frames_copy;
  } else {
    saved_stream_data_.clear();
    DeleteFrames(&frames_copy);
  }

  return encrypted.Clone();
}

bool QuicTestPacketMaker::ShouldIncludeVersion(bool include_version) const {
  if (version_.HasIetfInvariantHeader()) {
    return encryption_level_ < quic::ENCRYPTION_FORWARD_SECURE;
  }
  return include_version;
}

quic::QuicPacketNumberLength QuicTestPacketMaker::GetPacketNumberLength()
    const {
  if (version_.HasIetfInvariantHeader() &&
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

quic::QuicStreamId QuicTestPacketMaker::GetFirstBidirectionalStreamId() const {
  return quic::QuicUtils::GetFirstBidirectionalStreamId(
      version_.transport_version, perspective_);
}

quic::QuicStreamId QuicTestPacketMaker::GetHeadersStreamId() const {
  return quic::QuicUtils::GetHeadersStreamId(version_.transport_version);
}

std::string QuicTestPacketMaker::GenerateHttp3SettingsData() {
  quic::SettingsFrame settings;
  settings.values[quic::SETTINGS_MAX_FIELD_SECTION_SIZE] =
      kQuicMaxHeaderListSize;
  settings.values[quic::SETTINGS_QPACK_MAX_TABLE_CAPACITY] =
      quic::kDefaultQpackMaxDynamicTableCapacity;
  settings.values[quic::SETTINGS_QPACK_BLOCKED_STREAMS] =
      quic::kDefaultMaximumBlockedStreams;
  // Greased setting.
  settings.values[0x40] = 20;
  return quic::HttpEncoder::SerializeSettingsFrame(settings);
}

std::string QuicTestPacketMaker::GenerateHttp3PriorityData(
    spdy::SpdyPriority spdy_priority,
    quic::QuicStreamId stream_id) {
  std::string priority_data;
  quic::PriorityUpdateFrame priority_update;
  quic::QuicStreamPriority priority{
      spdy_priority, quic::QuicStreamPriority::kDefaultIncremental};
  if (client_priority_uses_incremental_ &&
      base::FeatureList::IsEnabled(features::kPriorityIncremental)) {
    priority.incremental = kDefaultPriorityIncremental;
  }

  if (priority.urgency != quic::QuicStreamPriority::kDefaultUrgency ||
      priority.incremental != quic::QuicStreamPriority::kDefaultIncremental) {
    priority_update.priority_field_value =
        quic::SerializePriorityFieldValue(priority);
  }

  // Only generate a frame if a non-empty string was generated.
  if (!priority_update.priority_field_value.empty()) {
    priority_update.prioritized_element_id = stream_id;
    priority_data =
        quic::HttpEncoder::SerializePriorityUpdateFrame(priority_update);
  }

  return priority_data;
}

std::string QuicTestPacketMaker::GenerateHttp3GreaseData() {
  return quic::HttpEncoder::SerializeGreasingFrame();
}

void QuicTestPacketMaker::MaybeAddHttp3SettingsFrames() {
  quic::QuicStreamId stream_id =
      quic::QuicUtils::GetFirstUnidirectionalStreamId(
          version_.transport_version, perspective_);

  if (stream_offsets_[stream_id] != 0)
    return;

  // A stream frame containing stream type will be written on the control
  // stream first.
  std::string type(1, 0x00);
  std::string settings_data = GenerateHttp3SettingsData();
  std::string grease_data = GenerateHttp3GreaseData();

  // The type and the SETTINGS frame may be sent in multiple QUIC STREAM
  // frames.
  std::string data = type + settings_data + grease_data;

  AddQuicStreamFrame(stream_id, false, data);
}

bool QuicTestPacketMaker::MaybeCoalesceStreamFrame(
    const quic::QuicFrame& frame) {
  if (frames_.empty()) {
    return false;
  }
  if (frame.type != quic::STREAM_FRAME ||
      frames_.back().type != quic::STREAM_FRAME) {
    return false;
  }

  // Make sure they are congruent data segments in the stream.
  const quic::QuicStreamFrame* new_frame = &frame.stream_frame;
  quic::QuicStreamFrame* previous_frame = &frames_.back().stream_frame;
  if (new_frame->stream_id != previous_frame->stream_id ||
      new_frame->offset !=
          previous_frame->offset + previous_frame->data_length) {
    return false;
  }

  // Extend the data buffer to include the data from both frames (into a copy
  // buffer). This doesn't attempt to limit coalescing to a particular packet
  // size limit and may need to be updated if a test comes along that
  // retransmits enough stream data to span multiple packets.
  std::string data(previous_frame->data_buffer, previous_frame->data_length);
  data += std::string(new_frame->data_buffer, new_frame->data_length);
  saved_stream_data_.push_back(std::make_unique<std::string>(data));
  absl::string_view saved_data = *saved_stream_data_.back();
  previous_frame->data_buffer = saved_data.data();
  previous_frame->data_length = saved_data.length();

  // Copy the fin state from the last frame.
  previous_frame->fin = new_frame->fin;

  return true;
}

}  // namespace net::test
