// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/quic_trace_visitor.h"

#include "net/third_party/quic/platform/api/quic_endian.h"
#include "net/third_party/quic/platform/api/quic_string.h"

namespace quic {

quic_trace::EncryptionLevel EncryptionLevelToProto(EncryptionLevel level) {
  switch (level) {
    case ENCRYPTION_NONE:
      return quic_trace::ENCRYPTION_INITIAL;
    case ENCRYPTION_INITIAL:
      return quic_trace::ENCRYPTION_0RTT;
    case ENCRYPTION_FORWARD_SECURE:
      return quic_trace::ENCRYPTION_1RTT;
    case NUM_ENCRYPTION_LEVELS:
      QUIC_BUG << "Invalid encryption level specified";
      return quic_trace::ENCRYPTION_UNKNOWN;
  }
}

QuicTraceVisitor::QuicTraceVisitor(const QuicConnection* connection)
    : connection_(connection),
      start_time_(connection_->clock()->ApproximateNow()) {
  // QUIC CIDs are currently represented in memory as a converted representation
  // of the on-wire ID.  Convert it back to wire format before recording, since
  // the standard treats it as an opaque blob.
  QuicConnectionId connection_id =
      QuicEndian::HostToNet64(connection->connection_id());
  QuicString binary_connection_id(reinterpret_cast<const char*>(&connection_id),
                                  sizeof(connection_id));

  // We assume that the connection ID in gQUIC is equivalent to the
  // server-chosen client-selected ID.
  switch (connection->perspective()) {
    case Perspective::IS_CLIENT:
      trace_.set_destination_connection_id(binary_connection_id);
      break;
    case Perspective::IS_SERVER:
      trace_.set_source_connection_id(binary_connection_id);
      break;
  }
}

void QuicTraceVisitor::OnPacketSent(const SerializedPacket& serialized_packet,
                                    QuicPacketNumber /*original_packet_number*/,
                                    TransmissionType /*transmission_type*/,
                                    QuicTime sent_time) {
  quic_trace::Event* event = trace_.add_events();
  event->set_event_type(quic_trace::PACKET_SENT);
  event->set_time_us(ConvertTimestampToRecordedFormat(sent_time));
  event->set_packet_number(serialized_packet.packet_number);
  event->set_packet_size(serialized_packet.encrypted_length);
  event->set_encryption_level(
      EncryptionLevelToProto(serialized_packet.encryption_level));

  for (const QuicFrame& frame : serialized_packet.retransmittable_frames) {
    switch (frame.type) {
      case STREAM_FRAME:
      case RST_STREAM_FRAME:
      case CONNECTION_CLOSE_FRAME:
      case WINDOW_UPDATE_FRAME:
      case BLOCKED_FRAME:
      case PING_FRAME:
        PopulateFrameInfo(frame, event->add_frames());
        break;

      case PADDING_FRAME:
      case MTU_DISCOVERY_FRAME:
      case STOP_WAITING_FRAME:
      case ACK_FRAME:
        QUIC_BUG
            << "Frames of type are not retransmittable and are not supposed "
               "to be in retransmittable_frames";
        break;

      // New IETF frames, not used in current gQUIC version.
      case APPLICATION_CLOSE_FRAME:
      case NEW_CONNECTION_ID_FRAME:
      case RETIRE_CONNECTION_ID_FRAME:
      case MAX_STREAM_ID_FRAME:
      case STREAM_ID_BLOCKED_FRAME:
      case PATH_RESPONSE_FRAME:
      case PATH_CHALLENGE_FRAME:
      case STOP_SENDING_FRAME:
      case MESSAGE_FRAME:
      case CRYPTO_FRAME:
      case NEW_TOKEN_FRAME:
        break;

      // Ignore gQUIC-specific frames.
      case GOAWAY_FRAME:
        break;

      case NUM_FRAME_TYPES:
        QUIC_BUG << "Unknown frame type encountered";
        break;
    }
  }

  // Output PCC DebugState on packet sent for analysis.
  if (connection_->sent_packet_manager()
          .GetSendAlgorithm()
          ->GetCongestionControlType() == kPCC) {
    PopulateTransportState(event->mutable_transport_state());
  }
}

void QuicTraceVisitor::PopulateFrameInfo(const QuicFrame& frame,
                                         quic_trace::Frame* frame_record) {
  switch (frame.type) {
    case STREAM_FRAME: {
      frame_record->set_frame_type(quic_trace::STREAM);

      quic_trace::StreamFrameInfo* info =
          frame_record->mutable_stream_frame_info();
      info->set_stream_id(frame.stream_frame.stream_id);
      info->set_fin(frame.stream_frame.fin);
      info->set_offset(frame.stream_frame.offset);
      info->set_length(frame.stream_frame.data_length);
      break;
    }

    case ACK_FRAME: {
      frame_record->set_frame_type(quic_trace::ACK);

      quic_trace::AckInfo* info = frame_record->mutable_ack_info();
      info->set_ack_delay_us(frame.ack_frame->ack_delay_time.ToMicroseconds());
      for (const auto& interval : frame.ack_frame->packets) {
        quic_trace::AckBlock* block = info->add_acked_packets();
        // We record intervals as [a, b], whereas the in-memory representation
        // we currently use is [a, b).
        block->set_first_packet(interval.min());
        block->set_last_packet(interval.max() - 1);
      }
      break;
    }

    case RST_STREAM_FRAME: {
      frame_record->set_frame_type(quic_trace::RESET_STREAM);

      quic_trace::ResetStreamInfo* info =
          frame_record->mutable_reset_stream_info();
      info->set_stream_id(frame.rst_stream_frame->stream_id);
      info->set_final_offset(frame.rst_stream_frame->byte_offset);
      info->set_application_error_code(frame.rst_stream_frame->error_code);
      break;
    }

    case CONNECTION_CLOSE_FRAME: {
      frame_record->set_frame_type(quic_trace::CONNECTION_CLOSE);

      quic_trace::CloseInfo* info = frame_record->mutable_close_info();
      info->set_error_code(frame.connection_close_frame->error_code);
      info->set_reason_phrase(frame.connection_close_frame->error_details);
      break;
    }

    case GOAWAY_FRAME:
      // Do not bother logging this since the frame in question is
      // gQUIC-specific.
      break;

    case WINDOW_UPDATE_FRAME: {
      bool is_connection = frame.window_update_frame->stream_id == 0;
      frame_record->set_frame_type(is_connection ? quic_trace::MAX_DATA
                                                 : quic_trace::MAX_STREAM_DATA);

      quic_trace::FlowControlInfo* info =
          frame_record->mutable_flow_control_info();
      info->set_max_data(frame.window_update_frame->byte_offset);
      if (!is_connection) {
        info->set_stream_id(frame.window_update_frame->stream_id);
      }
      break;
    }

    case BLOCKED_FRAME: {
      bool is_connection = frame.blocked_frame->stream_id == 0;
      frame_record->set_frame_type(is_connection ? quic_trace::BLOCKED
                                                 : quic_trace::STREAM_BLOCKED);

      quic_trace::FlowControlInfo* info =
          frame_record->mutable_flow_control_info();
      if (!is_connection) {
        info->set_stream_id(frame.window_update_frame->stream_id);
      }
      break;
    }

    case PING_FRAME:
    case MTU_DISCOVERY_FRAME:
      frame_record->set_frame_type(quic_trace::PING);
      break;

    case PADDING_FRAME:
      frame_record->set_frame_type(quic_trace::PADDING);
      break;

    case STOP_WAITING_FRAME:
      // We're going to pretend those do not exist.
      break;

    // New IETF frames, not used in current gQUIC version.
    case APPLICATION_CLOSE_FRAME:
    case NEW_CONNECTION_ID_FRAME:
    case RETIRE_CONNECTION_ID_FRAME:
    case MAX_STREAM_ID_FRAME:
    case STREAM_ID_BLOCKED_FRAME:
    case PATH_RESPONSE_FRAME:
    case PATH_CHALLENGE_FRAME:
    case STOP_SENDING_FRAME:
    case MESSAGE_FRAME:
    case CRYPTO_FRAME:
    case NEW_TOKEN_FRAME:
      break;

    case NUM_FRAME_TYPES:
      QUIC_BUG << "Unknown frame type encountered";
      break;
  }
}

void QuicTraceVisitor::OnIncomingAck(
    const QuicAckFrame& ack_frame,
    QuicTime ack_receive_time,
    QuicPacketNumber /*largest_observed*/,
    bool /*rtt_updated*/,
    QuicPacketNumber /*least_unacked_sent_packet*/) {
  quic_trace::Event* event = trace_.add_events();
  event->set_time_us(ConvertTimestampToRecordedFormat(ack_receive_time));
  event->set_packet_number(
      connection_->received_packet_manager().GetLargestObserved());
  event->set_event_type(quic_trace::PACKET_RECEIVED);

  // TODO(vasilvv): consider removing this copy.
  QuicAckFrame copy_of_ack = ack_frame;
  PopulateFrameInfo(QuicFrame(&copy_of_ack), event->add_frames());
  PopulateTransportState(event->mutable_transport_state());
}

void QuicTraceVisitor::OnPacketLoss(QuicPacketNumber lost_packet_number,
                                    TransmissionType transmission_type,
                                    QuicTime detection_time) {
  quic_trace::Event* event = trace_.add_events();
  event->set_time_us(ConvertTimestampToRecordedFormat(detection_time));
  event->set_event_type(quic_trace::PACKET_LOST);
  event->set_packet_number(lost_packet_number);
  PopulateTransportState(event->mutable_transport_state());
}

void QuicTraceVisitor::OnWindowUpdateFrame(const QuicWindowUpdateFrame& frame,
                                           const QuicTime& receive_time) {
  quic_trace::Event* event = trace_.add_events();
  event->set_time_us(ConvertTimestampToRecordedFormat(receive_time));
  event->set_event_type(quic_trace::PACKET_RECEIVED);
  event->set_packet_number(
      connection_->received_packet_manager().GetLargestObserved());

  // TODO(vasilvv): consider removing this copy.
  QuicWindowUpdateFrame copy_of_update = frame;
  PopulateFrameInfo(QuicFrame(&copy_of_update), event->add_frames());
}

void QuicTraceVisitor::OnSuccessfulVersionNegotiation(
    const ParsedQuicVersion& version) {
  uint32_t tag = QuicEndian::HostToNet32(CreateQuicVersionLabel(version));
  QuicString binary_tag(reinterpret_cast<const char*>(&tag), sizeof(tag));
  trace_.set_protocol_version(binary_tag);
}

void QuicTraceVisitor::OnApplicationLimited() {
  quic_trace::Event* event = trace_.add_events();
  event->set_time_us(
      ConvertTimestampToRecordedFormat(connection_->clock()->ApproximateNow()));
  event->set_event_type(quic_trace::APPLICATION_LIMITED);
}

void QuicTraceVisitor::OnAdjustNetworkParameters(QuicBandwidth bandwidth,
                                                 QuicTime::Delta rtt) {
  quic_trace::Event* event = trace_.add_events();
  event->set_time_us(
      ConvertTimestampToRecordedFormat(connection_->clock()->ApproximateNow()));
  event->set_event_type(quic_trace::EXTERNAL_PARAMETERS);

  quic_trace::ExternalNetworkParameters* parameters =
      event->mutable_external_network_parameters();
  if (!bandwidth.IsZero()) {
    parameters->set_bandwidth_bps(bandwidth.ToBitsPerSecond());
  }
  if (!rtt.IsZero()) {
    parameters->set_rtt_us(rtt.ToMicroseconds());
  }
}

uint64_t QuicTraceVisitor::ConvertTimestampToRecordedFormat(
    QuicTime timestamp) {
  if (timestamp < start_time_) {
    QUIC_BUG << "Timestamp went back in time while recording a trace";
    return 0;
  }

  return (timestamp - start_time_).ToMicroseconds();
}

void QuicTraceVisitor::PopulateTransportState(
    quic_trace::TransportState* state) {
  const RttStats* rtt_stats = connection_->sent_packet_manager().GetRttStats();
  state->set_min_rtt_us(rtt_stats->min_rtt().ToMicroseconds());
  state->set_smoothed_rtt_us(rtt_stats->smoothed_rtt().ToMicroseconds());
  state->set_last_rtt_us(rtt_stats->latest_rtt().ToMicroseconds());

  state->set_cwnd_bytes(
      connection_->sent_packet_manager().GetCongestionWindowInBytes());
  QuicByteCount in_flight =
      connection_->sent_packet_manager().GetBytesInFlight();
  state->set_in_flight_bytes(in_flight);
  state->set_pacing_rate_bps(connection_->sent_packet_manager()
                                 .GetSendAlgorithm()
                                 ->PacingRate(in_flight)
                                 .ToBitsPerSecond());

  if (connection_->sent_packet_manager()
          .GetSendAlgorithm()
          ->GetCongestionControlType() == kPCC) {
    state->set_congestion_control_state(
        connection_->sent_packet_manager().GetSendAlgorithm()->GetDebugState());
  }
}

};  // namespace quic
