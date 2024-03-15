// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_event_logger.h"

#include "base/containers/span.h"
#include "base/strings/string_number_conversions.h"
#include "net/cert/x509_certificate.h"
#include "net/log/net_log_values.h"
#include "net/quic/address_utils.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_socket_address_coder.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace net {

namespace {

base::Value::Dict NetLogQuicPacketParams(
    const quic::QuicSocketAddress& self_address,
    const quic::QuicSocketAddress& peer_address,
    size_t packet_size) {
  return base::Value::Dict()
      .Set("self_address", self_address.ToString())
      .Set("peer_address", peer_address.ToString())
      .Set("size", static_cast<int>(packet_size));
}

base::Value::Dict NetLogQuicPacketSentParams(
    quic::QuicPacketNumber packet_number,
    quic::QuicPacketLength packet_length,
    quic::TransmissionType transmission_type,
    quic::EncryptionLevel encryption_level,
    quic::QuicTime sent_time,
    uint32_t batch_id) {
  return base::Value::Dict()
      .Set("transmission_type",
           quic::TransmissionTypeToString(transmission_type))
      .Set("packet_number", NetLogNumberValue(packet_number.ToUint64()))
      .Set("size", packet_length)
      .Set("sent_time_us", NetLogNumberValue(sent_time.ToDebuggingValue()))
      .Set("encryption_level", quic::EncryptionLevelToString(encryption_level))
      .Set("batch_id", NetLogNumberValue(batch_id));
}

base::Value::Dict NetLogQuicPacketLostParams(
    quic::QuicPacketNumber packet_number,
    quic::TransmissionType transmission_type,
    quic::QuicTime detection_time) {
  return base::Value::Dict()
      .Set("transmission_type",
           quic::TransmissionTypeToString(transmission_type))
      .Set("packet_number", NetLogNumberValue(packet_number.ToUint64()))
      .Set("detection_time_us",
           NetLogNumberValue(detection_time.ToDebuggingValue()));
}

base::Value::Dict NetLogQuicConfigProcessed(
    const quic::QuicSentPacketManager::DebugDelegate::SendParameters&
        parameters) {
  return base::Value::Dict()
      .Set("congestion_control_type", quic::CongestionControlTypeToString(
                                          parameters.congestion_control_type))
      .Set("use_pacing", parameters.use_pacing)
      .Set("initial_congestion_window",
           NetLogNumberValue(parameters.initial_congestion_window));
}

base::Value::Dict NetLogQuicDuplicatePacketParams(
    quic::QuicPacketNumber packet_number) {
  return base::Value::Dict().Set("packet_number",
                                 NetLogNumberValue(packet_number.ToUint64()));
}

base::Value::Dict NetLogReceivedQuicPacketHeaderParams(
    const quic::QuicPacketHeader& header,
    const quic::ParsedQuicVersion& session_version,
    const quic::QuicConnectionId& connection_id,
    const quic::QuicConnectionId& client_connection_id) {
  base::Value::Dict dict;
  quic::ParsedQuicVersion version = session_version;
  if (header.version_flag &&
      header.version != quic::ParsedQuicVersion::Unsupported()) {
    version = header.version;
    if (version != session_version) {
      dict.Set("version", quic::ParsedQuicVersionToString(version));
    }
  }
  dict.Set("connection_id", connection_id.ToString());
  if (!client_connection_id.IsEmpty()) {
    dict.Set("client_connection_id", client_connection_id.ToString());
  }
  if (header.destination_connection_id_included ==
          quic::CONNECTION_ID_PRESENT &&
      header.destination_connection_id != client_connection_id &&
      !header.destination_connection_id.IsEmpty()) {
    dict.Set("destination_connection_id",
             header.destination_connection_id.ToString());
  }
  if (header.source_connection_id_included == quic::CONNECTION_ID_PRESENT &&
      header.source_connection_id != connection_id &&
      !header.source_connection_id.IsEmpty()) {
    dict.Set("source_connection_id", header.source_connection_id.ToString());
  }
  dict.Set("packet_number", NetLogNumberValue(header.packet_number.ToUint64()));
  dict.Set("header_format", quic::PacketHeaderFormatToString(header.form));
  if (header.form == quic::IETF_QUIC_LONG_HEADER_PACKET) {
    dict.Set("long_header_type",
             quic::QuicLongHeaderTypeToString(header.long_packet_type));
  }
  return dict;
}

base::Value::Dict NetLogQuicStreamFrameParams(
    const quic::QuicStreamFrame& frame) {
  return base::Value::Dict()
      .Set("stream_id", static_cast<int>(frame.stream_id))
      .Set("fin", frame.fin)
      .Set("offset", NetLogNumberValue(frame.offset))
      .Set("length", frame.data_length);
}

base::Value::Dict NetLogQuicAckFrameParams(const quic::QuicAckFrame* frame) {
  base::Value::List missing;
  quic::QuicPacketNumber smallest_observed;
  if (!frame->packets.Empty()) {
    // V34 and above express acked packets, but only print
    // missing packets, because it's typically a shorter list.
    smallest_observed = frame->packets.Min();
    for (quic::QuicPacketNumber packet = smallest_observed;
         packet < frame->largest_acked; ++packet) {
      if (!frame->packets.Contains(packet)) {
        missing.Append(NetLogNumberValue(packet.ToUint64()));
      }
    }
  } else {
    smallest_observed = frame->largest_acked;
  }

  base::Value::List received;
  for (const auto& packet_time : frame->received_packet_times) {
    received.Append(
        base::Value::Dict()
            .Set("packet_number",
                 NetLogNumberValue(packet_time.first.ToUint64()))
            .Set("received",
                 NetLogNumberValue(packet_time.second.ToDebuggingValue())));
  }

  base::Value::Dict rv;
  rv.Set("largest_observed",
         NetLogNumberValue(frame->largest_acked.ToUint64()));
  rv.Set("delta_time_largest_observed_us",
         NetLogNumberValue(frame->ack_delay_time.ToMicroseconds()));
  rv.Set("smallest_observed", NetLogNumberValue(smallest_observed.ToUint64()));
  rv.Set("missing_packets", std::move(missing));
  rv.Set("received_packet_times", std::move(received));
  if (frame->ecn_counters.has_value()) {
    rv.Set("ECT0", NetLogNumberValue(frame->ecn_counters->ect0));
    rv.Set("ECT1", NetLogNumberValue(frame->ecn_counters->ect1));
    rv.Set("CE", NetLogNumberValue(frame->ecn_counters->ce));
  }
  return rv;
}

base::Value::Dict NetLogQuicRstStreamFrameParams(
    const quic::QuicRstStreamFrame* frame) {
  return base::Value::Dict()
      .Set("stream_id", static_cast<int>(frame->stream_id))
      .Set("quic_rst_stream_error", static_cast<int>(frame->error_code))
      .Set("ietf_error_code", static_cast<int>(frame->ietf_error_code))
      .Set("offset", NetLogNumberValue(frame->byte_offset));
}

base::Value::Dict NetLogQuicConnectionCloseFrameParams(
    const quic::QuicConnectionCloseFrame* frame) {
  base::Value::Dict dict;
  dict.Set("quic_error", static_cast<int>(frame->quic_error_code));
  if (frame->wire_error_code != frame->quic_error_code) {
    dict.Set("quic_wire_error", static_cast<int>(frame->wire_error_code));
  }
  std::string close_type;
  switch (frame->close_type) {
    case quic::GOOGLE_QUIC_CONNECTION_CLOSE:
      close_type = "gQUIC";
      break;
    case quic::IETF_QUIC_TRANSPORT_CONNECTION_CLOSE:
      close_type = "Transport";
      break;
    case quic::IETF_QUIC_APPLICATION_CONNECTION_CLOSE:
      close_type = "Application";
      break;
  }
  dict.Set("close_type", close_type);
  if (frame->transport_close_frame_type != 0) {
    dict.Set("transport_close_frame_type",
             NetLogNumberValue(frame->transport_close_frame_type));
  }
  dict.Set("details", frame->error_details);
  return dict;
}

base::Value::Dict NetLogQuicWindowUpdateFrameParams(
    const quic::QuicWindowUpdateFrame& frame) {
  return base::Value::Dict()
      .Set("stream_id", static_cast<int>(frame.stream_id))
      .Set("byte_offset", NetLogNumberValue(frame.max_data));
}

base::Value::Dict NetLogQuicBlockedFrameParams(
    const quic::QuicBlockedFrame& frame) {
  return base::Value::Dict().Set("stream_id",
                                 static_cast<int>(frame.stream_id));
}

base::Value::Dict NetLogQuicGoAwayFrameParams(
    const quic::QuicGoAwayFrame* frame) {
  return base::Value::Dict()
      .Set("quic_error", static_cast<int>(frame->error_code))
      .Set("last_good_stream_id", static_cast<int>(frame->last_good_stream_id))
      .Set("reason_phrase", frame->reason_phrase);
}

base::Value::Dict NetLogQuicStopWaitingFrameParams(
    const quic::QuicStopWaitingFrame* frame) {
  return base::Value::Dict().Set(
      "least_unacked", NetLogNumberValue(frame->least_unacked.ToUint64()));
}

base::Value::Dict NetLogQuicVersionNegotiationPacketParams(
    const quic::QuicVersionNegotiationPacket* packet) {
  base::Value::List versions;
  for (const auto& version : packet->versions) {
    versions.Append(ParsedQuicVersionToString(version));
  }
  return base::Value::Dict().Set("versions", std::move(versions));
}

base::Value::Dict NetLogQuicPathData(const quic::QuicPathFrameBuffer& buffer) {
  return base::Value::Dict().Set("data", NetLogBinaryValue(buffer));
}

base::Value::Dict NetLogQuicCryptoHandshakeMessageParams(
    const quic::CryptoHandshakeMessage* message) {
  return base::Value::Dict().Set("quic_crypto_handshake_message",
                                 message->DebugString());
}

base::Value::Dict NetLogQuicTransportParametersParams(
    const quic::TransportParameters& transport_parameters) {
  return base::Value::Dict().Set("quic_transport_parameters",
                                 transport_parameters.ToString());
}

base::Value::Dict NetLogQuicZeroRttRejectReason(int reason) {
  base::Value::Dict dict;
  const char* reason_detail = SSL_early_data_reason_string(
      static_cast<ssl_early_data_reason_t>(reason));
  if (reason_detail) {
    dict.Set("reason", reason_detail);
  } else {
    dict.Set("reason", "Unknown reason " + base::NumberToString(reason));
  }
  return dict;
}

base::Value::Dict NetLogQuicOnConnectionClosedParams(
    quic::QuicErrorCode error,
    std::string error_details,
    quic::ConnectionCloseSource source) {
  return base::Value::Dict()
      .Set("quic_error", static_cast<int>(error))
      .Set("details", error_details)
      .Set("from_peer",
           source == quic::ConnectionCloseSource::FROM_PEER ? true : false);
}

base::Value::Dict NetLogQuicCertificateVerifiedParams(
    scoped_refptr<X509Certificate> cert) {
  // Only the subjects are logged so that we can investigate connection pooling.
  // More fields could be logged in the future.
  std::vector<std::string> dns_names;
  cert->GetSubjectAltName(&dns_names, nullptr);
  base::Value::List subjects;
  for (auto& dns_name : dns_names) {
    subjects.Append(std::move(dns_name));
  }
  return base::Value::Dict().Set("subjects", std::move(subjects));
}

base::Value::Dict NetLogQuicCryptoFrameParams(
    const quic::QuicCryptoFrame* frame,
    bool has_buffer) {
  auto dict =
      base::Value::Dict()
          .Set("encryption_level", quic::EncryptionLevelToString(frame->level))
          .Set("data_length", frame->data_length)
          .Set("offset", NetLogNumberValue(frame->offset));
  if (has_buffer) {
    dict.Set("bytes", NetLogBinaryValue(
                          reinterpret_cast<const void*>(frame->data_buffer),
                          frame->data_length));
  }
  return dict;
}

base::Value::Dict NetLogQuicStopSendingFrameParams(
    const quic::QuicStopSendingFrame& frame) {
  return base::Value::Dict()
      .Set("stream_id", static_cast<int>(frame.stream_id))
      .Set("quic_rst_stream_error", static_cast<int>(frame.error_code))
      .Set("ietf_error_code", static_cast<int>(frame.ietf_error_code));
}

base::Value::Dict NetLogQuicStreamsBlockedFrameParams(
    const quic::QuicStreamsBlockedFrame& frame) {
  return base::Value::Dict()
      .Set("stream_count", static_cast<int>(frame.stream_count))
      .Set("is_unidirectional", frame.unidirectional);
}

base::Value::Dict NetLogQuicMaxStreamsFrameParams(
    const quic::QuicMaxStreamsFrame& frame) {
  return base::Value::Dict()
      .Set("stream_count", static_cast<int>(frame.stream_count))
      .Set("is_unidirectional", frame.unidirectional);
}

base::Value::Dict NetLogQuicNewConnectionIdFrameParams(
    const quic::QuicNewConnectionIdFrame* frame) {
  return base::Value::Dict()
      .Set("connection_id", frame->connection_id.ToString())
      .Set("sequence_number", NetLogNumberValue(frame->sequence_number))
      .Set("retire_prior_to", NetLogNumberValue(frame->retire_prior_to));
}

base::Value::Dict NetLogQuicRetireConnectionIdFrameParams(
    const quic::QuicRetireConnectionIdFrame* frame) {
  return base::Value::Dict().Set("sequence_number",
                                 NetLogNumberValue(frame->sequence_number));
}

base::Value::Dict NetLogQuicNewTokenFrameParams(
    const quic::QuicNewTokenFrame* frame) {
  return base::Value::Dict().Set(
      "token",
      NetLogBinaryValue(reinterpret_cast<const void*>(frame->token.data()),
                        frame->token.length()));
}

}  // namespace

QuicEventLogger::QuicEventLogger(quic::QuicSession* session,
                                 const NetLogWithSource& net_log)
    : session_(session), net_log_(net_log) {}

void QuicEventLogger::OnFrameAddedToPacket(const quic::QuicFrame& frame) {
  if (!net_log_.IsCapturing())
    return;
  switch (frame.type) {
    case quic::PADDING_FRAME:
      net_log_.AddEventWithIntParams(
          NetLogEventType::QUIC_SESSION_PADDING_FRAME_SENT, "num_padding_bytes",
          frame.padding_frame.num_padding_bytes);
      break;
    case quic::STREAM_FRAME:
      net_log_.AddEvent(NetLogEventType::QUIC_SESSION_STREAM_FRAME_SENT, [&] {
        return NetLogQuicStreamFrameParams(frame.stream_frame);
      });
      break;
    case quic::ACK_FRAME: {
      net_log_.AddEvent(NetLogEventType::QUIC_SESSION_ACK_FRAME_SENT, [&] {
        return NetLogQuicAckFrameParams(frame.ack_frame);
      });
      break;
    }
    case quic::RST_STREAM_FRAME:
      net_log_.AddEvent(
          NetLogEventType::QUIC_SESSION_RST_STREAM_FRAME_SENT, [&] {
            return NetLogQuicRstStreamFrameParams(frame.rst_stream_frame);
          });
      break;
    case quic::CONNECTION_CLOSE_FRAME:
      net_log_.AddEvent(
          NetLogEventType::QUIC_SESSION_CONNECTION_CLOSE_FRAME_SENT, [&] {
            return NetLogQuicConnectionCloseFrameParams(
                frame.connection_close_frame);
          });
      break;
    case quic::GOAWAY_FRAME:
      net_log_.AddEvent(NetLogEventType::QUIC_SESSION_GOAWAY_FRAME_SENT, [&] {
        return NetLogQuicGoAwayFrameParams(frame.goaway_frame);
      });
      break;
    case quic::WINDOW_UPDATE_FRAME:
      net_log_.AddEvent(
          NetLogEventType::QUIC_SESSION_WINDOW_UPDATE_FRAME_SENT, [&] {
            return NetLogQuicWindowUpdateFrameParams(frame.window_update_frame);
          });
      break;
    case quic::BLOCKED_FRAME:
      net_log_.AddEvent(NetLogEventType::QUIC_SESSION_BLOCKED_FRAME_SENT, [&] {
        return NetLogQuicBlockedFrameParams(frame.blocked_frame);
      });
      break;
    case quic::STOP_WAITING_FRAME:
      net_log_.AddEvent(
          NetLogEventType::QUIC_SESSION_STOP_WAITING_FRAME_SENT, [&] {
            return NetLogQuicStopWaitingFrameParams(&frame.stop_waiting_frame);
          });
      break;
    case quic::PING_FRAME:
      // PingFrame has no contents to log, so just record that it was sent.
      net_log_.AddEvent(NetLogEventType::QUIC_SESSION_PING_FRAME_SENT);
      break;
    case quic::MTU_DISCOVERY_FRAME:
      // MtuDiscoveryFrame is PingFrame on wire, it does not have any payload.
      net_log_.AddEvent(NetLogEventType::QUIC_SESSION_MTU_DISCOVERY_FRAME_SENT);
      break;
    case quic::NEW_CONNECTION_ID_FRAME:
      net_log_.AddEvent(
          NetLogEventType::QUIC_SESSION_NEW_CONNECTION_ID_FRAME_SENT, [&] {
            return NetLogQuicNewConnectionIdFrameParams(
                frame.new_connection_id_frame);
          });
      break;
    case quic::MAX_STREAMS_FRAME:
      net_log_.AddEvent(
          NetLogEventType::QUIC_SESSION_MAX_STREAMS_FRAME_SENT, [&] {
            return NetLogQuicMaxStreamsFrameParams(frame.max_streams_frame);
          });
      break;
    case quic::STREAMS_BLOCKED_FRAME:
      net_log_.AddEvent(
          NetLogEventType::QUIC_SESSION_STREAMS_BLOCKED_FRAME_SENT, [&] {
            return NetLogQuicStreamsBlockedFrameParams(
                frame.streams_blocked_frame);
          });
      break;
    case quic::PATH_RESPONSE_FRAME:
      net_log_.AddEvent(
          NetLogEventType::QUIC_SESSION_PATH_RESPONSE_FRAME_SENT, [&] {
            return NetLogQuicPathData(frame.path_response_frame.data_buffer);
          });
      break;
    case quic::PATH_CHALLENGE_FRAME:
      net_log_.AddEvent(
          NetLogEventType::QUIC_SESSION_PATH_CHALLENGE_FRAME_SENT, [&] {
            return NetLogQuicPathData(frame.path_challenge_frame.data_buffer);
          });
      break;
    case quic::STOP_SENDING_FRAME:
      net_log_.AddEvent(
          NetLogEventType::QUIC_SESSION_STOP_SENDING_FRAME_SENT, [&] {
            return NetLogQuicStopSendingFrameParams(frame.stop_sending_frame);
          });
      break;
    case quic::MESSAGE_FRAME:
      net_log_.AddEventWithIntParams(
          NetLogEventType::QUIC_SESSION_MESSAGE_FRAME_SENT, "message_length",
          frame.message_frame->message_length);
      break;
    case quic::CRYPTO_FRAME:
      net_log_.AddEvent(NetLogEventType::QUIC_SESSION_CRYPTO_FRAME_SENT, [&] {
        return NetLogQuicCryptoFrameParams(frame.crypto_frame,
                                           /*has_buffer = */ false);
      });
      break;
    case quic::NEW_TOKEN_FRAME:
      net_log_.AddEvent(
          NetLogEventType::QUIC_SESSION_NEW_TOKEN_FRAME_SENT,
          [&] { return NetLogQuicNewTokenFrameParams(frame.new_token_frame); });
      break;
    case quic::RETIRE_CONNECTION_ID_FRAME:
      net_log_.AddEvent(
          NetLogEventType::QUIC_SESSION_RETIRE_CONNECTION_ID_FRAME_SENT, [&] {
            return NetLogQuicRetireConnectionIdFrameParams(
                frame.retire_connection_id_frame);
          });
      break;
    default:
      DCHECK(false) << "Illegal frame type: " << frame.type;
  }
}

void QuicEventLogger::OnStreamFrameCoalesced(
    const quic::QuicStreamFrame& frame) {
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_STREAM_FRAME_COALESCED,
                    [&] { return NetLogQuicStreamFrameParams(frame); });
}

void QuicEventLogger::OnPacketSent(
    quic::QuicPacketNumber packet_number,
    quic::QuicPacketLength packet_length,
    bool /*has_crypto_handshake*/,
    quic::TransmissionType transmission_type,
    quic::EncryptionLevel encryption_level,
    const quic::QuicFrames& /*retransmittable_frames*/,
    const quic::QuicFrames& /*nonretransmittable_frames*/,
    quic::QuicTime sent_time,
    uint32_t batch_id) {
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_PACKET_SENT, [&] {
    return NetLogQuicPacketSentParams(packet_number, packet_length,
                                      transmission_type, encryption_level,
                                      sent_time, batch_id);
  });
}

void QuicEventLogger::OnIncomingAck(
    quic::QuicPacketNumber ack_packet_number,
    quic::EncryptionLevel /*ack_decrypted_level*/,
    const quic::QuicAckFrame& frame,
    quic::QuicTime ack_receive_time,
    quic::QuicPacketNumber largest_observed,
    bool rtt_updated,
    quic::QuicPacketNumber least_unacked_sent_packet) {
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_ACK_FRAME_RECEIVED,
                    [&] { return NetLogQuicAckFrameParams(&frame); });
}

void QuicEventLogger::OnPacketLoss(quic::QuicPacketNumber lost_packet_number,
                                   quic::EncryptionLevel /*encryption_level*/,
                                   quic::TransmissionType transmission_type,
                                   quic::QuicTime detection_time) {
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_PACKET_LOST, [&] {
    return NetLogQuicPacketLostParams(lost_packet_number, transmission_type,
                                      detection_time);
  });
}

void QuicEventLogger::OnConfigProcessed(
    const quic::QuicSentPacketManager::DebugDelegate::SendParameters&
        parameters) {
  net_log_.AddEvent(NetLogEventType::QUIC_CONGESTION_CONTROL_CONFIGURED,
                    [&] { return NetLogQuicConfigProcessed(parameters); });
}

void QuicEventLogger::OnPacketReceived(
    const quic::QuicSocketAddress& self_address,
    const quic::QuicSocketAddress& peer_address,
    const quic::QuicEncryptedPacket& packet) {
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_PACKET_RECEIVED, [&] {
    return NetLogQuicPacketParams(self_address, peer_address, packet.length());
  });
}

void QuicEventLogger::OnUnauthenticatedHeader(
    const quic::QuicPacketHeader& header) {
  net_log_.AddEvent(
      NetLogEventType::QUIC_SESSION_UNAUTHENTICATED_PACKET_HEADER_RECEIVED,
      [&] {
        return NetLogReceivedQuicPacketHeaderParams(
            header, session_->version(),
            session_->connection()->connection_id(),
            session_->connection()->client_connection_id());
      });
}

void QuicEventLogger::OnUndecryptablePacket(
    quic::EncryptionLevel decryption_level,
    bool dropped) {
  if (dropped) {
    net_log_.AddEventWithStringParams(
        NetLogEventType::QUIC_SESSION_DROPPED_UNDECRYPTABLE_PACKET,
        "encryption_level", quic::EncryptionLevelToString(decryption_level));
    return;
  }
  net_log_.AddEventWithStringParams(
      NetLogEventType::QUIC_SESSION_BUFFERED_UNDECRYPTABLE_PACKET,
      "encryption_level", quic::EncryptionLevelToString(decryption_level));
}

void QuicEventLogger::OnAttemptingToProcessUndecryptablePacket(
    quic::EncryptionLevel decryption_level) {
  net_log_.AddEventWithStringParams(
      NetLogEventType::QUIC_SESSION_ATTEMPTING_TO_PROCESS_UNDECRYPTABLE_PACKET,
      "encryption_level", quic::EncryptionLevelToString(decryption_level));
}

void QuicEventLogger::OnDuplicatePacket(quic::QuicPacketNumber packet_number) {
  net_log_.AddEvent(
      NetLogEventType::QUIC_SESSION_DUPLICATE_PACKET_RECEIVED,
      [&] { return NetLogQuicDuplicatePacketParams(packet_number); });
}

void QuicEventLogger::OnPacketHeader(const quic::QuicPacketHeader& header,
                                     quic::QuicTime /*receive_time*/,
                                     quic::EncryptionLevel /*level*/) {
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_PACKET_AUTHENTICATED);
}

void QuicEventLogger::OnStreamFrame(const quic::QuicStreamFrame& frame) {
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_STREAM_FRAME_RECEIVED,
                    [&] { return NetLogQuicStreamFrameParams(frame); });
}

void QuicEventLogger::OnPathChallengeFrame(
    const quic::QuicPathChallengeFrame& frame) {
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_PATH_CHALLENGE_FRAME_RECEIVED,
                    [&] { return NetLogQuicPathData(frame.data_buffer); });
}

void QuicEventLogger::OnPathResponseFrame(
    const quic::QuicPathResponseFrame& frame) {
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_PATH_RESPONSE_FRAME_RECEIVED,
                    [&] { return NetLogQuicPathData(frame.data_buffer); });
}

void QuicEventLogger::OnCryptoFrame(const quic::QuicCryptoFrame& frame) {
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_CRYPTO_FRAME_RECEIVED, [&] {
    return NetLogQuicCryptoFrameParams(&frame, /*has_buffer = */ true);
  });
}

void QuicEventLogger::OnStopSendingFrame(
    const quic::QuicStopSendingFrame& frame) {
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_STOP_SENDING_FRAME_RECEIVED,
                    [&] { return NetLogQuicStopSendingFrameParams(frame); });
}

void QuicEventLogger::OnStreamsBlockedFrame(
    const quic::QuicStreamsBlockedFrame& frame) {
  net_log_.AddEvent(
      NetLogEventType::QUIC_SESSION_STREAMS_BLOCKED_FRAME_RECEIVED,
      [&] { return NetLogQuicStreamsBlockedFrameParams(frame); });
}

void QuicEventLogger::OnMaxStreamsFrame(
    const quic::QuicMaxStreamsFrame& frame) {
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_MAX_STREAMS_FRAME_RECEIVED,
                    [&] { return NetLogQuicMaxStreamsFrameParams(frame); });
}

void QuicEventLogger::OnRstStreamFrame(const quic::QuicRstStreamFrame& frame) {
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_RST_STREAM_FRAME_RECEIVED,
                    [&] { return NetLogQuicRstStreamFrameParams(&frame); });
}

void QuicEventLogger::OnConnectionCloseFrame(
    const quic::QuicConnectionCloseFrame& frame) {
  net_log_.AddEvent(
      NetLogEventType::QUIC_SESSION_CONNECTION_CLOSE_FRAME_RECEIVED,
      [&] { return NetLogQuicConnectionCloseFrameParams(&frame); });
}

void QuicEventLogger::OnWindowUpdateFrame(
    const quic::QuicWindowUpdateFrame& frame,
    const quic::QuicTime& receive_time) {
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_WINDOW_UPDATE_FRAME_RECEIVED,
                    [&] { return NetLogQuicWindowUpdateFrameParams(frame); });
}

void QuicEventLogger::OnBlockedFrame(const quic::QuicBlockedFrame& frame) {
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_BLOCKED_FRAME_RECEIVED,
                    [&] { return NetLogQuicBlockedFrameParams(frame); });
}

void QuicEventLogger::OnGoAwayFrame(const quic::QuicGoAwayFrame& frame) {
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_GOAWAY_FRAME_RECEIVED,
                    [&] { return NetLogQuicGoAwayFrameParams(&frame); });
}

void QuicEventLogger::OnPingFrame(
    const quic::QuicPingFrame& frame,
    quic::QuicTime::Delta /*ping_received_delay*/) {
  // PingFrame has no contents to log, so just record that it was received.
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_PING_FRAME_RECEIVED);
}

void QuicEventLogger::OnPaddingFrame(const quic::QuicPaddingFrame& frame) {
  net_log_.AddEventWithIntParams(
      NetLogEventType::QUIC_SESSION_PADDING_FRAME_RECEIVED, "num_padding_bytes",
      frame.num_padding_bytes);
}

void QuicEventLogger::OnNewConnectionIdFrame(
    const quic::QuicNewConnectionIdFrame& frame) {
  net_log_.AddEvent(
      NetLogEventType::QUIC_SESSION_NEW_CONNECTION_ID_FRAME_RECEIVED,
      [&] { return NetLogQuicNewConnectionIdFrameParams(&frame); });
}

void QuicEventLogger::OnNewTokenFrame(const quic::QuicNewTokenFrame& frame) {
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_NEW_TOKEN_FRAME_RECEIVED,
                    [&] { return NetLogQuicNewTokenFrameParams(&frame); });
}

void QuicEventLogger::OnRetireConnectionIdFrame(
    const quic::QuicRetireConnectionIdFrame& frame) {
  net_log_.AddEvent(
      NetLogEventType::QUIC_SESSION_RETIRE_CONNECTION_ID_FRAME_RECEIVED,
      [&] { return NetLogQuicRetireConnectionIdFrameParams(&frame); });
}

void QuicEventLogger::OnMessageFrame(const quic::QuicMessageFrame& frame) {
  net_log_.AddEventWithIntParams(
      NetLogEventType::QUIC_SESSION_MESSAGE_FRAME_RECEIVED, "message_length",
      frame.message_length);
}

void QuicEventLogger::OnHandshakeDoneFrame(
    const quic::QuicHandshakeDoneFrame& frame) {
  net_log_.AddEvent(
      NetLogEventType::QUIC_SESSION_HANDSHAKE_DONE_FRAME_RECEIVED);
}

void QuicEventLogger::OnCoalescedPacketSent(
    const quic::QuicCoalescedPacket& coalesced_packet,
    size_t length) {
  net_log_.AddEventWithStringParams(
      NetLogEventType::QUIC_SESSION_COALESCED_PACKET_SENT, "info",
      coalesced_packet.ToString(length));
}

void QuicEventLogger::OnVersionNegotiationPacket(
    const quic::QuicVersionNegotiationPacket& packet) {
  net_log_.AddEvent(
      NetLogEventType::QUIC_SESSION_VERSION_NEGOTIATION_PACKET_RECEIVED,
      [&] { return NetLogQuicVersionNegotiationPacketParams(&packet); });
}

void QuicEventLogger::OnCryptoHandshakeMessageReceived(
    const quic::CryptoHandshakeMessage& message) {
  if (message.tag() == quic::kSHLO) {
    std::string_view address;
    quic::QuicSocketAddressCoder decoder;
    if (message.GetStringPiece(quic::kCADR, &address) &&
        decoder.Decode(address.data(), address.size())) {
      local_address_from_shlo_ =
          IPEndPoint(ToIPAddress(decoder.ip()), decoder.port());
    }
  }

  net_log_.AddEvent(
      NetLogEventType::QUIC_SESSION_CRYPTO_HANDSHAKE_MESSAGE_RECEIVED,
      [&] { return NetLogQuicCryptoHandshakeMessageParams(&message); });
}

void QuicEventLogger::OnCryptoHandshakeMessageSent(
    const quic::CryptoHandshakeMessage& message) {
  net_log_.AddEvent(
      NetLogEventType::QUIC_SESSION_CRYPTO_HANDSHAKE_MESSAGE_SENT,
      [&] { return NetLogQuicCryptoHandshakeMessageParams(&message); });
}

void QuicEventLogger::OnConnectionClosed(
    const quic::QuicConnectionCloseFrame& frame,
    quic::ConnectionCloseSource source) {
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_CLOSED, [&] {
    return NetLogQuicOnConnectionClosedParams(frame.quic_error_code,
                                              frame.error_details, source);
  });
}

void QuicEventLogger::OnSuccessfulVersionNegotiation(
    const quic::ParsedQuicVersion& version) {
  if (!net_log_.IsCapturing())
    return;
  std::string quic_version = quic::ParsedQuicVersionToString(version);
  net_log_.AddEventWithStringParams(
      NetLogEventType::QUIC_SESSION_VERSION_NEGOTIATED, "version",
      quic_version);
}

void QuicEventLogger::OnCertificateVerified(const CertVerifyResult& result) {
  if (result.cert_status == CERT_STATUS_INVALID) {
    net_log_.AddEvent(NetLogEventType::QUIC_SESSION_CERTIFICATE_VERIFY_FAILED);
    return;
  }
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_CERTIFICATE_VERIFIED, [&] {
    return NetLogQuicCertificateVerifiedParams(result.verified_cert);
  });
}

void QuicEventLogger::OnTransportParametersSent(
    const quic::TransportParameters& transport_parameters) {
  net_log_.AddEvent(
      NetLogEventType::QUIC_SESSION_TRANSPORT_PARAMETERS_SENT, [&] {
        return NetLogQuicTransportParametersParams(transport_parameters);
      });
}

void QuicEventLogger::OnTransportParametersReceived(
    const quic::TransportParameters& transport_parameters) {
  net_log_.AddEvent(
      NetLogEventType::QUIC_SESSION_TRANSPORT_PARAMETERS_RECEIVED, [&] {
        return NetLogQuicTransportParametersParams(transport_parameters);
      });
}

void QuicEventLogger::OnTransportParametersResumed(
    const quic::TransportParameters& transport_parameters) {
  net_log_.AddEvent(
      NetLogEventType::QUIC_SESSION_TRANSPORT_PARAMETERS_RESUMED, [&] {
        return NetLogQuicTransportParametersParams(transport_parameters);
      });
}

void QuicEventLogger::OnZeroRttRejected(int reason) {
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_ZERO_RTT_REJECTED,
                    [reason] { return NetLogQuicZeroRttRejectReason(reason); });
}

void QuicEventLogger::OnEncryptedClientHelloSent(
    std::string_view client_hello) {
  net_log_.AddEvent(NetLogEventType::SSL_ENCRYPTED_CLIENT_HELLO, [&] {
    return base::Value::Dict().Set(
        "bytes", NetLogBinaryValue(base::as_byte_span(client_hello)));
  });
}

}  // namespace net
