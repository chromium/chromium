// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_event_logger.h"

#include "net/cert/x509_certificate.h"
#include "net/log/net_log_values.h"
#include "net/quic/address_utils.h"
#include "net/third_party/quiche/src/quic/core/quic_socket_address_coder.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace net {

namespace {

base::Value NetLogQuicPacketParams(const quic::QuicSocketAddress& self_address,
                                   const quic::QuicSocketAddress& peer_address,
                                   size_t packet_size) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("self_address", self_address.ToString());
  dict.SetStringKey("peer_address", peer_address.ToString());
  dict.SetIntKey("size", packet_size);
  return dict;
}

base::Value NetLogQuicPacketSentParams(
    const quic::SerializedPacket& serialized_packet,
    quic::TransmissionType transmission_type,
    quic::QuicTime sent_time) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("transmission_type",
                    quic::TransmissionTypeToString(transmission_type));
  dict.SetKey("packet_number",
              NetLogNumberValue(serialized_packet.packet_number.ToUint64()));
  dict.SetIntKey("size", serialized_packet.encrypted_length);
  dict.SetKey("sent_time_us", NetLogNumberValue(sent_time.ToDebuggingValue()));
  dict.SetStringKey(
      "encryption_level",
      quic::EncryptionLevelToString(serialized_packet.encryption_level));
  return dict;
}

base::Value NetLogQuicPacketLostParams(quic::QuicPacketNumber packet_number,
                                       quic::TransmissionType transmission_type,
                                       quic::QuicTime detection_time) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("transmission_type",
                    quic::TransmissionTypeToString(transmission_type));
  dict.SetKey("packet_number", NetLogNumberValue(packet_number.ToUint64()));
  dict.SetKey("detection_time_us",
              NetLogNumberValue(detection_time.ToDebuggingValue()));
  return dict;
}

base::Value NetLogQuicDuplicatePacketParams(
    quic::QuicPacketNumber packet_number) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("packet_number", NetLogNumberValue(packet_number.ToUint64()));
  return dict;
}

base::Value NetLogReceivedQuicPacketHeaderParams(
    const quic::QuicPacketHeader& header,
    const quic::ParsedQuicVersion& session_version,
    const quic::QuicConnectionId& connection_id,
    const quic::QuicConnectionId& client_connection_id) {
  base::Value dict(base::Value::Type::DICTIONARY);
  quic::ParsedQuicVersion version = session_version;
  if (header.version_flag &&
      header.version != quic::ParsedQuicVersion::Unsupported()) {
    version = header.version;
    if (version != session_version) {
      dict.SetStringKey("version", quic::ParsedQuicVersionToString(version));
    }
  }
  dict.SetStringKey("connection_id", connection_id.ToString());
  if (!client_connection_id.IsEmpty()) {
    dict.SetStringKey("client_connection_id", client_connection_id.ToString());
  }
  if (version.HasIetfInvariantHeader()) {
    if (header.destination_connection_id_included ==
            quic::CONNECTION_ID_PRESENT &&
        header.destination_connection_id != client_connection_id &&
        !header.destination_connection_id.IsEmpty()) {
      dict.SetStringKey("destination_connection_id",
                        header.destination_connection_id.ToString());
    }
    if (header.source_connection_id_included == quic::CONNECTION_ID_PRESENT &&
        header.source_connection_id != connection_id &&
        !header.source_connection_id.IsEmpty()) {
      dict.SetStringKey("source_connection_id",
                        header.source_connection_id.ToString());
    }
  } else {
    if (header.destination_connection_id_included ==
            quic::CONNECTION_ID_PRESENT &&
        header.destination_connection_id != connection_id &&
        !header.destination_connection_id.IsEmpty()) {
      dict.SetStringKey("destination_connection_id",
                        header.destination_connection_id.ToString());
    }
    dict.SetIntKey("reset_flag", header.reset_flag);
    dict.SetIntKey("version_flag", header.version_flag);
  }
  dict.SetKey("packet_number",
              NetLogNumberValue(header.packet_number.ToUint64()));
  dict.SetStringKey("header_format",
                    quic::PacketHeaderFormatToString(header.form));
  if (header.form == quic::IETF_QUIC_LONG_HEADER_PACKET) {
    dict.SetStringKey("long_header_type", quic::QuicLongHeaderTypeToString(
                                              header.long_packet_type));
  }
  return dict;
}

base::Value NetLogQuicStreamFrameParams(const quic::QuicStreamFrame& frame) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("stream_id", frame.stream_id);
  dict.SetBoolKey("fin", frame.fin);
  dict.SetKey("offset", NetLogNumberValue(frame.offset));
  dict.SetIntKey("length", frame.data_length);
  return dict;
}

base::Value NetLogQuicAckFrameParams(const quic::QuicAckFrame* frame) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("largest_observed",
              NetLogNumberValue(frame->largest_acked.ToUint64()));
  dict.SetKey("delta_time_largest_observed_us",
              NetLogNumberValue(frame->ack_delay_time.ToMicroseconds()));

  base::Value missing(base::Value::Type::LIST);
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
  dict.SetKey("smallest_observed",
              NetLogNumberValue(smallest_observed.ToUint64()));
  dict.SetKey("missing_packets", std::move(missing));

  base::Value received(base::Value::Type::LIST);
  const quic::PacketTimeVector& received_times = frame->received_packet_times;
  for (auto it = received_times.begin(); it != received_times.end(); ++it) {
    base::Value info(base::Value::Type::DICTIONARY);
    info.SetKey("packet_number", NetLogNumberValue(it->first.ToUint64()));
    info.SetKey("received", NetLogNumberValue(it->second.ToDebuggingValue()));
    received.Append(std::move(info));
  }
  dict.SetKey("received_packet_times", std::move(received));

  return dict;
}

base::Value NetLogQuicRstStreamFrameParams(
    const quic::QuicRstStreamFrame* frame) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("stream_id", frame->stream_id);
  dict.SetIntKey("quic_rst_stream_error", frame->error_code);
  dict.SetKey("offset", NetLogNumberValue(frame->byte_offset));
  return dict;
}

base::Value NetLogQuicConnectionCloseFrameParams(
    const quic::QuicConnectionCloseFrame* frame) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("quic_error", frame->quic_error_code);
  dict.SetStringKey("details", frame->error_details);
  return dict;
}

base::Value NetLogQuicWindowUpdateFrameParams(
    const quic::QuicWindowUpdateFrame* frame) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("stream_id", frame->stream_id);
  dict.SetKey("byte_offset", NetLogNumberValue(frame->max_data));
  return dict;
}

base::Value NetLogQuicBlockedFrameParams(const quic::QuicBlockedFrame* frame) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("stream_id", frame->stream_id);
  return dict;
}

base::Value NetLogQuicGoAwayFrameParams(const quic::QuicGoAwayFrame* frame) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("quic_error", frame->error_code);
  dict.SetIntKey("last_good_stream_id", frame->last_good_stream_id);
  dict.SetStringKey("reason_phrase", frame->reason_phrase);
  return dict;
}

base::Value NetLogQuicStopWaitingFrameParams(
    const quic::QuicStopWaitingFrame* frame) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("least_unacked",
              NetLogNumberValue(frame->least_unacked.ToUint64()));
  return dict;
}

base::Value NetLogQuicVersionNegotiationPacketParams(
    const quic::QuicVersionNegotiationPacket* packet) {
  base::Value dict(base::Value::Type::DICTIONARY);
  base::Value versions(base::Value::Type::LIST);
  for (auto it = packet->versions.begin(); it != packet->versions.end(); ++it) {
    versions.Append(ParsedQuicVersionToString(*it));
  }
  dict.SetKey("versions", std::move(versions));
  return dict;
}

base::Value NetLogQuicPublicResetPacketParams(
    const IPEndPoint& server_hello_address,
    const quic::QuicSocketAddress& public_reset_address) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("server_hello_address", server_hello_address.ToString());
  dict.SetStringKey("public_reset_address", public_reset_address.ToString());
  return dict;
}

base::Value NetLogQuicPathData(const quic::QuicPathFrameBuffer& buffer) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("data",
              NetLogBinaryValue(reinterpret_cast<const char*>(buffer.data()),
                                buffer.size()));
  return dict;
}

base::Value NetLogQuicCryptoHandshakeMessageParams(
    const quic::CryptoHandshakeMessage* message) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("quic_crypto_handshake_message", message->DebugString());
  return dict;
}

base::Value NetLogQuicTransportParametersParams(
    const quic::TransportParameters& transport_parameters) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("quic_transport_parameters",
                    transport_parameters.ToString());
  return dict;
}

base::Value NetLogQuicZeroRttRejectReason(int reason) {
  base::Value dict(base::Value::Type::DICTIONARY);
  std::string reason_detail = "";
  switch (reason) {
    case ssl_early_data_unknown:
      reason_detail = "The handshake has not progressed far enough";
      break;
    case ssl_early_data_disabled:
      reason_detail = "0-RTT is disabled for this connection.";
      break;
    case ssl_early_data_accepted:
      reason_detail = "0-RTT was accepted";
      break;
    case ssl_early_data_protocol_version:
      reason_detail = "The negotiated protocol doesn't support 0-RTT.";
      break;
    case ssl_early_data_peer_declined:
      reason_detail =
          "The peer declined to offer or accept 0-RTT for an unknown reason.";
      break;
    case ssl_early_data_no_session_offered:
      reason_detail = "The client did not offer a session.";
      break;
    case ssl_early_data_session_not_resumed:
      reason_detail = "The server declined to resume the session.";
      break;
    case ssl_early_data_unsupported_for_session:
      reason_detail = "The session does not support 0-RTT.";
      break;
    case ssl_early_data_hello_retry_request:
      reason_detail = "The server sent a HelloRetryRequest.";
      break;
    case ssl_early_data_alpn_mismatch:
      reason_detail = "The negotiated ALPN protocol did not match the session.";
      break;
    case ssl_early_data_channel_id:
      reason_detail =
          "The connection negotiated Channel ID, which is incompatible with "
          "0-RTT.";
      break;
    case ssl_early_data_token_binding:
      reason_detail =
          "The connection negotiated token binding, which is incompatible with "
          "0-RTT.";
      break;
    case ssl_early_data_ticket_age_skew:
      reason_detail = "The client and server ticket age were too far apart.";
      break;
    case ssl_early_data_quic_parameter_mismatch:
      reason_detail =
          "QUIC parameters differ between this connection and the original.";
      break;
    default:
      reason_detail = "Unknown reason " + base::NumberToString(reason);
  }
  dict.SetStringKey("reason", reason_detail);
  return dict;
}

base::Value NetLogQuicOnConnectionClosedParams(
    quic::QuicErrorCode error,
    std::string error_details,
    quic::ConnectionCloseSource source) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("quic_error", error);
  dict.SetStringKey("details", error_details);
  dict.SetBoolKey("from_peer", source == quic::ConnectionCloseSource::FROM_PEER
                                   ? true
                                   : false);
  return dict;
}

base::Value NetLogQuicCertificateVerifiedParams(
    scoped_refptr<X509Certificate> cert) {
  // Only the subjects are logged so that we can investigate connection pooling.
  // More fields could be logged in the future.
  std::vector<std::string> dns_names;
  cert->GetSubjectAltName(&dns_names, nullptr);
  base::Value dict(base::Value::Type::DICTIONARY);
  base::Value subjects(base::Value::Type::LIST);
  for (auto& dns_name : dns_names) {
    subjects.Append(std::move(dns_name));
  }
  dict.SetKey("subjects", std::move(subjects));
  return dict;
}

base::Value NetLogQuicCryptoFrameParams(const quic::QuicCryptoFrame* frame,
                                        bool has_buffer) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("encryption_level",
                    quic::EncryptionLevelToString(frame->level));
  dict.SetIntKey("data_length", frame->data_length);
  dict.SetKey("offset", NetLogNumberValue(frame->offset));
  if (has_buffer) {
    dict.SetKey("bytes", NetLogBinaryValue(
                             reinterpret_cast<const void*>(frame->data_buffer),
                             frame->data_length));
  }
  return dict;
}

base::Value NetLogQuicStopSendingFrameParams(
    const quic::QuicStopSendingFrame* frame) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("stream_id", frame->stream_id);
  dict.SetIntKey("error_code", frame->error_code);
  return dict;
}

base::Value NetLogQuicStreamsBlockedFrameParams(
    const quic::QuicStreamsBlockedFrame& frame) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("stream_count", frame.stream_count);
  dict.SetBoolKey("is_unidirectional", frame.unidirectional);
  return dict;
}

base::Value NetLogQuicMaxStreamsFrameParams(
    const quic::QuicMaxStreamsFrame& frame) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("stream_count", frame.stream_count);
  dict.SetBoolKey("is_unidirectional", frame.unidirectional);
  return dict;
}

base::Value NetLogQuicNewConnectionIdFrameParams(
    const quic::QuicNewConnectionIdFrame* frame) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("connection_id", frame->connection_id.ToString());
  dict.SetKey("sequence_number", NetLogNumberValue(frame->sequence_number));
  dict.SetKey("retire_prior_to", NetLogNumberValue(frame->retire_prior_to));
  return dict;
}

base::Value NetLogQuicRetireConnectionIdFrameParams(
    const quic::QuicRetireConnectionIdFrame* frame) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("sequence_number", NetLogNumberValue(frame->sequence_number));
  return dict;
}

base::Value NetLogQuicNewTokenFrameParams(
    const quic::QuicNewTokenFrame* frame) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("token", NetLogBinaryValue(
                           reinterpret_cast<const void*>(frame->token.data()),
                           frame->token.length()));
  return dict;
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
            return NetLogQuicPathData(frame.path_response_frame->data_buffer);
          });
      break;
    case quic::PATH_CHALLENGE_FRAME:
      net_log_.AddEvent(
          NetLogEventType::QUIC_SESSION_PATH_CHALLENGE_FRAME_SENT, [&] {
            return NetLogQuicPathData(frame.path_challenge_frame->data_buffer);
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
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_STREAM_FRAME_COALESCED,
                    [&] { return NetLogQuicStreamFrameParams(frame); });
}

void QuicEventLogger::OnPacketSent(
    const quic::SerializedPacket& serialized_packet,
    quic::TransmissionType transmission_type,
    quic::QuicTime sent_time) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_PACKET_SENT, [&] {
    return NetLogQuicPacketSentParams(serialized_packet, transmission_type,
                                      sent_time);
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
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_ACK_FRAME_RECEIVED,
                    [&] { return NetLogQuicAckFrameParams(&frame); });
}

void QuicEventLogger::OnPacketLoss(quic::QuicPacketNumber lost_packet_number,
                                   quic::EncryptionLevel /*encryption_level*/,
                                   quic::TransmissionType transmission_type,
                                   quic::QuicTime detection_time) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_PACKET_LOST, [&] {
    return NetLogQuicPacketLostParams(lost_packet_number, transmission_type,
                                      detection_time);
  });
}

void QuicEventLogger::OnPacketReceived(
    const quic::QuicSocketAddress& self_address,
    const quic::QuicSocketAddress& peer_address,
    const quic::QuicEncryptedPacket& packet) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_PACKET_RECEIVED, [&] {
    return NetLogQuicPacketParams(self_address, peer_address, packet.length());
  });
}

void QuicEventLogger::OnUnauthenticatedHeader(
    const quic::QuicPacketHeader& header) {
  if (!net_log_.IsCapturing())
    return;
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
  if (!net_log_.IsCapturing())
    return;
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
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEventWithStringParams(
      NetLogEventType::QUIC_SESSION_ATTEMPTING_TO_PROCESS_UNDECRYPTABLE_PACKET,
      "encryption_level", quic::EncryptionLevelToString(decryption_level));
}

void QuicEventLogger::OnDuplicatePacket(quic::QuicPacketNumber packet_number) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(
      NetLogEventType::QUIC_SESSION_DUPLICATE_PACKET_RECEIVED,
      [&] { return NetLogQuicDuplicatePacketParams(packet_number); });
}

void QuicEventLogger::OnPacketHeader(const quic::QuicPacketHeader& header) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_PACKET_AUTHENTICATED);
}

void QuicEventLogger::OnStreamFrame(const quic::QuicStreamFrame& frame) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_STREAM_FRAME_RECEIVED,
                    [&] { return NetLogQuicStreamFrameParams(frame); });
}

void QuicEventLogger::OnPathChallengeFrame(
    const quic::QuicPathChallengeFrame& frame) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_PATH_CHALLENGE_FRAME_RECEIVED,
                    [&] { return NetLogQuicPathData(frame.data_buffer); });
}

void QuicEventLogger::OnPathResponseFrame(
    const quic::QuicPathResponseFrame& frame) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_PATH_RESPONSE_FRAME_RECEIVED,
                    [&] { return NetLogQuicPathData(frame.data_buffer); });
}

void QuicEventLogger::OnCryptoFrame(const quic::QuicCryptoFrame& frame) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_CRYPTO_FRAME_RECEIVED, [&] {
    return NetLogQuicCryptoFrameParams(&frame, /*has_buffer = */ true);
  });
}

void QuicEventLogger::OnStopSendingFrame(
    const quic::QuicStopSendingFrame& frame) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_STOP_SENDING_FRAME_RECEIVED,
                    [&] { return NetLogQuicStopSendingFrameParams(&frame); });
}

void QuicEventLogger::OnStreamsBlockedFrame(
    const quic::QuicStreamsBlockedFrame& frame) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(
      NetLogEventType::QUIC_SESSION_STREAMS_BLOCKED_FRAME_RECEIVED,
      [&] { return NetLogQuicStreamsBlockedFrameParams(frame); });
}

void QuicEventLogger::OnMaxStreamsFrame(
    const quic::QuicMaxStreamsFrame& frame) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_MAX_STREAMS_FRAME_RECEIVED,
                    [&] { return NetLogQuicMaxStreamsFrameParams(frame); });
}

void QuicEventLogger::OnStopWaitingFrame(
    const quic::QuicStopWaitingFrame& frame) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_STOP_WAITING_FRAME_RECEIVED,
                    [&] { return NetLogQuicStopWaitingFrameParams(&frame); });
}

void QuicEventLogger::OnRstStreamFrame(const quic::QuicRstStreamFrame& frame) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_RST_STREAM_FRAME_RECEIVED,
                    [&] { return NetLogQuicRstStreamFrameParams(&frame); });
}

void QuicEventLogger::OnConnectionCloseFrame(
    const quic::QuicConnectionCloseFrame& frame) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(
      NetLogEventType::QUIC_SESSION_CONNECTION_CLOSE_FRAME_RECEIVED,
      [&] { return NetLogQuicConnectionCloseFrameParams(&frame); });
}

void QuicEventLogger::OnWindowUpdateFrame(
    const quic::QuicWindowUpdateFrame& frame,
    const quic::QuicTime& receive_time) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_WINDOW_UPDATE_FRAME_RECEIVED,
                    [&] { return NetLogQuicWindowUpdateFrameParams(&frame); });
}

void QuicEventLogger::OnBlockedFrame(const quic::QuicBlockedFrame& frame) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_BLOCKED_FRAME_RECEIVED,
                    [&] { return NetLogQuicBlockedFrameParams(&frame); });
}

void QuicEventLogger::OnGoAwayFrame(const quic::QuicGoAwayFrame& frame) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_GOAWAY_FRAME_RECEIVED,
                    [&] { return NetLogQuicGoAwayFrameParams(&frame); });
}

void QuicEventLogger::OnPingFrame(
    const quic::QuicPingFrame& frame,
    quic::QuicTime::Delta /*ping_received_delay*/) {
  // PingFrame has no contents to log, so just record that it was received.
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_PING_FRAME_RECEIVED);
}

void QuicEventLogger::OnPaddingFrame(const quic::QuicPaddingFrame& frame) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEventWithIntParams(
      NetLogEventType::QUIC_SESSION_PADDING_FRAME_RECEIVED, "num_padding_bytes",
      frame.num_padding_bytes);
}

void QuicEventLogger::OnNewConnectionIdFrame(
    const quic::QuicNewConnectionIdFrame& frame) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(
      NetLogEventType::QUIC_SESSION_NEW_CONNECTION_ID_FRAME_RECEIVED,
      [&] { return NetLogQuicNewConnectionIdFrameParams(&frame); });
}

void QuicEventLogger::OnNewTokenFrame(const quic::QuicNewTokenFrame& frame) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_NEW_TOKEN_FRAME_RECEIVED,
                    [&] { return NetLogQuicNewTokenFrameParams(&frame); });
}

void QuicEventLogger::OnRetireConnectionIdFrame(
    const quic::QuicRetireConnectionIdFrame& frame) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(
      NetLogEventType::QUIC_SESSION_RETIRE_CONNECTION_ID_FRAME_RECEIVED,
      [&] { return NetLogQuicRetireConnectionIdFrameParams(&frame); });
}

void QuicEventLogger::OnMessageFrame(const quic::QuicMessageFrame& frame) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEventWithIntParams(
      NetLogEventType::QUIC_SESSION_MESSAGE_FRAME_RECEIVED, "message_length",
      frame.message_length);
}

void QuicEventLogger::OnHandshakeDoneFrame(
    const quic::QuicHandshakeDoneFrame& frame) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(
      NetLogEventType::QUIC_SESSION_HANDSHAKE_DONE_FRAME_RECEIVED);
}

void QuicEventLogger::OnCoalescedPacketSent(
    const quic::QuicCoalescedPacket& coalesced_packet,
    size_t length) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEventWithStringParams(
      NetLogEventType::QUIC_SESSION_COALESCED_PACKET_SENT, "info",
      coalesced_packet.ToString(length));
}

void QuicEventLogger::OnPublicResetPacket(
    const quic::QuicPublicResetPacket& packet) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_PUBLIC_RESET_PACKET_RECEIVED,
                    [&] {
                      return NetLogQuicPublicResetPacketParams(
                          local_address_from_shlo_, packet.client_address);
                    });
}

void QuicEventLogger::OnVersionNegotiationPacket(
    const quic::QuicVersionNegotiationPacket& packet) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(
      NetLogEventType::QUIC_SESSION_VERSION_NEGOTIATION_PACKET_RECEIVED,
      [&] { return NetLogQuicVersionNegotiationPacketParams(&packet); });
}

void QuicEventLogger::OnCryptoHandshakeMessageReceived(
    const quic::CryptoHandshakeMessage& message) {
  if (message.tag() == quic::kSHLO) {
    quiche::QuicheStringPiece address;
    quic::QuicSocketAddressCoder decoder;
    if (message.GetStringPiece(quic::kCADR, &address) &&
        decoder.Decode(address.data(), address.size())) {
      local_address_from_shlo_ =
          IPEndPoint(ToIPAddress(decoder.ip()), decoder.port());
    }
  }

  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(
      NetLogEventType::QUIC_SESSION_CRYPTO_HANDSHAKE_MESSAGE_RECEIVED,
      [&] { return NetLogQuicCryptoHandshakeMessageParams(&message); });
}

void QuicEventLogger::OnCryptoHandshakeMessageSent(
    const quic::CryptoHandshakeMessage& message) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(
      NetLogEventType::QUIC_SESSION_CRYPTO_HANDSHAKE_MESSAGE_SENT,
      [&] { return NetLogQuicCryptoHandshakeMessageParams(&message); });
}

void QuicEventLogger::OnConnectionClosed(
    const quic::QuicConnectionCloseFrame& frame,
    quic::ConnectionCloseSource source) {
  if (!net_log_.IsCapturing())
    return;
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
  if (!net_log_.IsCapturing())
    return;
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
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(
      NetLogEventType::QUIC_SESSION_TRANSPORT_PARAMETERS_SENT, [&] {
        return NetLogQuicTransportParametersParams(transport_parameters);
      });
}

void QuicEventLogger::OnTransportParametersReceived(
    const quic::TransportParameters& transport_parameters) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(
      NetLogEventType::QUIC_SESSION_TRANSPORT_PARAMETERS_RECEIVED, [&] {
        return NetLogQuicTransportParametersParams(transport_parameters);
      });
}

void QuicEventLogger::OnTransportParametersResumed(
    const quic::TransportParameters& transport_parameters) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(
      NetLogEventType::QUIC_SESSION_TRANSPORT_PARAMETERS_RESUMED, [&] {
        return NetLogQuicTransportParametersParams(transport_parameters);
      });
}

void QuicEventLogger::OnZeroRttRejected(int reason) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_ZERO_RTT_REJECTED,
                    [reason] { return NetLogQuicZeroRttRejectReason(reason); });
}

}  // namespace net
