// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_connection_logger.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "net/base/ip_address.h"
#include "net/cert/x509_certificate.h"
#include "net/quic/address_utils.h"
#include "net/quic/quic_address_mismatch.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/crypto_handshake_message.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/crypto_protocol.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_connection_id.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_socket_address_coder.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_time.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_utils.h"

using quic::kMaxOutgoingPacketSize;
using std::string;

namespace net {

namespace {

// If |address| is an IPv4-mapped IPv6 address, returns ADDRESS_FAMILY_IPV4
// instead of ADDRESS_FAMILY_IPV6. Othewise, behaves like GetAddressFamily().
AddressFamily GetRealAddressFamily(const IPAddress& address) {
  return address.IsIPv4MappedIPv6() ? ADDRESS_FAMILY_IPV4
                                    : GetAddressFamily(address);
}

}  // namespace

QuicConnectionLogger::QuicConnectionLogger(
    quic::QuicSession* session,
    const char* const connection_description,
    std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
    const NetLogWithSource& net_log)
    : session_(session),
      connection_description_(connection_description),
      socket_performance_watcher_(std::move(socket_performance_watcher)),
      event_logger_(session, net_log) {}

QuicConnectionLogger::~QuicConnectionLogger() {
  UMA_HISTOGRAM_COUNTS_1M("Net.QuicSession.OutOfOrderPacketsReceived",
                          num_out_of_order_received_packets_);
  UMA_HISTOGRAM_COUNTS_1M("Net.QuicSession.OutOfOrderLargePacketsReceived",
                          num_out_of_order_large_received_packets_);
  UMA_HISTOGRAM_COUNTS_1M("Net.QuicSession.IncorrectConnectionIDsReceived",
                          num_incorrect_connection_ids_);
  UMA_HISTOGRAM_COUNTS_1M("Net.QuicSession.UndecryptablePacketsReceived",
                          num_undecryptable_packets_);
  UMA_HISTOGRAM_COUNTS_1M("Net.QuicSession.DuplicatePacketsReceived",
                          num_duplicate_packets_);
  UMA_HISTOGRAM_COUNTS_1M("Net.QuicSession.BlockedFrames.Received",
                          num_blocked_frames_received_);
  UMA_HISTOGRAM_COUNTS_1M("Net.QuicSession.BlockedFrames.Sent",
                          num_blocked_frames_sent_);

  const quic::QuicConnectionStats& stats = session_->connection()->GetStats();
  UMA_HISTOGRAM_TIMES("Net.QuicSession.MinRTT",
                      base::Microseconds(stats.min_rtt_us));
  UMA_HISTOGRAM_TIMES("Net.QuicSession.SmoothedRTT",
                      base::Microseconds(stats.srtt_us));

  if (num_frames_received_ > 0) {
    int duplicate_stream_frame_per_thousand =
        num_duplicate_frames_received_ * 1000 / num_frames_received_;
    if (num_packets_received_ < 100) {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Net.QuicSession.StreamFrameDuplicatedShortConnection",
          duplicate_stream_frame_per_thousand, 1, 1000, 75);
    } else {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Net.QuicSession.StreamFrameDuplicatedLongConnection",
          duplicate_stream_frame_per_thousand, 1, 1000, 75);
    }
  }

  RecordAggregatePacketLossRate();
}

void QuicConnectionLogger::OnFrameAddedToPacket(const quic::QuicFrame& frame) {
  switch (frame.type) {
    case quic::PADDING_FRAME:
      break;
    case quic::STREAM_FRAME:
      break;
    case quic::ACK_FRAME: {
      break;
    }
    case quic::RST_STREAM_FRAME:
      base::UmaHistogramSparse("Net.QuicSession.RstStreamErrorCodeClient",
                               frame.rst_stream_frame->error_code);
      break;
    case quic::CONNECTION_CLOSE_FRAME:
      break;
    case quic::GOAWAY_FRAME:
      break;
    case quic::WINDOW_UPDATE_FRAME:
      break;
    case quic::BLOCKED_FRAME:
      ++num_blocked_frames_sent_;
      break;
    case quic::STOP_WAITING_FRAME:
      break;
    case quic::PING_FRAME:
      UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.ConnectionFlowControlBlocked",
                            session_->IsConnectionFlowControlBlocked());
      UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.StreamFlowControlBlocked",
                            session_->IsStreamFlowControlBlocked());
      break;
    case quic::MTU_DISCOVERY_FRAME:
      break;
    case quic::NEW_CONNECTION_ID_FRAME:
      break;
    case quic::MAX_STREAMS_FRAME:
      break;
    case quic::STREAMS_BLOCKED_FRAME:
      break;
    case quic::PATH_RESPONSE_FRAME:
      break;
    case quic::PATH_CHALLENGE_FRAME:
      break;
    case quic::STOP_SENDING_FRAME:
      base::UmaHistogramSparse("Net.QuicSession.StopSendingErrorCodeClient",
                               frame.stop_sending_frame.error_code);
      break;
    case quic::MESSAGE_FRAME:
      break;
    case quic::CRYPTO_FRAME:
      break;
    case quic::NEW_TOKEN_FRAME:
      break;
    case quic::RETIRE_CONNECTION_ID_FRAME:
      break;
    default:
      DCHECK(false) << "Illegal frame type: " << frame.type;
  }
  event_logger_.OnFrameAddedToPacket(frame);
}

void QuicConnectionLogger::OnStreamFrameCoalesced(
    const quic::QuicStreamFrame& frame) {
  event_logger_.OnStreamFrameCoalesced(frame);
}

void QuicConnectionLogger::OnPacketSent(
    quic::QuicPacketNumber packet_number,
    quic::QuicPacketLength packet_length,
    bool has_crypto_handshake,
    quic::TransmissionType transmission_type,
    quic::EncryptionLevel encryption_level,
    const quic::QuicFrames& retransmittable_frames,
    const quic::QuicFrames& nonretransmittable_frames,
    quic::QuicTime sent_time,
    uint32_t batch_id) {
  // 4.4.1.4.  Minimum Packet Size
  // The payload of a UDP datagram carrying the Initial packet MUST be
  // expanded to at least 1200 octets
  const quic::QuicPacketLength kMinClientInitialPacketLength = 1200;
  switch (encryption_level) {
    case quic::ENCRYPTION_INITIAL:
      UMA_HISTOGRAM_CUSTOM_COUNTS("Net.QuicSession.SendPacketSize.Initial",
                                  packet_length, 1, kMaxOutgoingPacketSize, 50);
      if (packet_length < kMinClientInitialPacketLength) {
        UMA_HISTOGRAM_CUSTOM_COUNTS(
            "Net.QuicSession.TooSmallInitialSentPacket",
            kMinClientInitialPacketLength - packet_length, 1,
            kMinClientInitialPacketLength, 50);
      }
      break;
    case quic::ENCRYPTION_HANDSHAKE:
      UMA_HISTOGRAM_CUSTOM_COUNTS("Net.QuicSession.SendPacketSize.Handshake",
                                  packet_length, 1, kMaxOutgoingPacketSize, 50);
      break;
    case quic::ENCRYPTION_ZERO_RTT:
      UMA_HISTOGRAM_CUSTOM_COUNTS("Net.QuicSession.SendPacketSize.0RTT",
                                  packet_length, 1, kMaxOutgoingPacketSize, 50);
      break;
    case quic::ENCRYPTION_FORWARD_SECURE:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Net.QuicSession.SendPacketSize.ForwardSecure", packet_length, 1,
          kMaxOutgoingPacketSize, 50);
      break;
    case quic::NUM_ENCRYPTION_LEVELS:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  event_logger_.OnPacketSent(packet_number, packet_length, has_crypto_handshake,
                             transmission_type, encryption_level,
                             retransmittable_frames, nonretransmittable_frames,
                             sent_time, batch_id);
}

void QuicConnectionLogger::OnPacketLoss(
    quic::QuicPacketNumber lost_packet_number,
    quic::EncryptionLevel encryption_level,
    quic::TransmissionType transmission_type,
    quic::QuicTime detection_time) {
  event_logger_.OnPacketLoss(lost_packet_number, encryption_level,
                             transmission_type, detection_time);
}

void QuicConnectionLogger::OnConfigProcessed(
    const quic::QuicSentPacketManager::DebugDelegate::SendParameters&
        parameters) {
  event_logger_.OnConfigProcessed(parameters);
}

void QuicConnectionLogger::OnPingSent() {
  no_packet_received_after_ping_ = true;
}

void QuicConnectionLogger::OnPacketReceived(
    const quic::QuicSocketAddress& self_address,
    const quic::QuicSocketAddress& peer_address,
    const quic::QuicEncryptedPacket& packet) {
  if (local_address_from_self_.GetFamily() == ADDRESS_FAMILY_UNSPECIFIED) {
    local_address_from_self_ = ToIPEndPoint(self_address);
    UMA_HISTOGRAM_ENUMERATION(
        "Net.QuicSession.ConnectionTypeFromSelf",
        GetRealAddressFamily(ToIPEndPoint(self_address).address()),
        ADDRESS_FAMILY_LAST);
  }

  previous_received_packet_size_ = last_received_packet_size_;
  last_received_packet_size_ = packet.length();
  event_logger_.OnPacketReceived(self_address, peer_address, packet);
}

void QuicConnectionLogger::OnUnauthenticatedHeader(
    const quic::QuicPacketHeader& header) {
  event_logger_.OnUnauthenticatedHeader(header);
}

void QuicConnectionLogger::OnIncorrectConnectionId(
    quic::QuicConnectionId connection_id) {
  ++num_incorrect_connection_ids_;
}

void QuicConnectionLogger::OnUndecryptablePacket(
    quic::EncryptionLevel decryption_level,
    bool dropped) {
  ++num_undecryptable_packets_;
  event_logger_.OnUndecryptablePacket(decryption_level, dropped);
}

void QuicConnectionLogger::OnAttemptingToProcessUndecryptablePacket(
    quic::EncryptionLevel decryption_level) {
  event_logger_.OnAttemptingToProcessUndecryptablePacket(decryption_level);
}

void QuicConnectionLogger::OnDuplicatePacket(
    quic::QuicPacketNumber packet_number) {
  ++num_duplicate_packets_;
  event_logger_.OnDuplicatePacket(packet_number);
}

void QuicConnectionLogger::OnProtocolVersionMismatch(
    quic::ParsedQuicVersion received_version) {
  // TODO(rtenneti): Add logging.
}

void QuicConnectionLogger::OnPacketHeader(const quic::QuicPacketHeader& header,
                                          quic::QuicTime receive_time,
                                          quic::EncryptionLevel level) {
  if (!first_received_packet_number_.IsInitialized()) {
    first_received_packet_number_ = header.packet_number;
  } else if (header.packet_number < first_received_packet_number_) {
    // Ignore packets with packet numbers less than
    // first_received_packet_number_.
    return;
  }
  ++num_packets_received_;
  if (!largest_received_packet_number_.IsInitialized()) {
    largest_received_packet_number_ = header.packet_number;
  } else if (largest_received_packet_number_ < header.packet_number) {
    uint64_t delta = header.packet_number - largest_received_packet_number_;
    if (delta > 1) {
      // There is a gap between the largest packet previously received and
      // the current packet.  This indicates either loss, or out-of-order
      // delivery.
      UMA_HISTOGRAM_COUNTS_1M(
          "Net.QuicSession.PacketGapReceived",
          static_cast<base::HistogramBase::Sample>(delta - 1));
    }
    largest_received_packet_number_ = header.packet_number;
  }
  if (header.packet_number - first_received_packet_number_ <
      received_packets_.size()) {
    received_packets_[header.packet_number - first_received_packet_number_] =
        true;
  }
  if (last_received_packet_number_.IsInitialized() &&
      header.packet_number < last_received_packet_number_) {
    ++num_out_of_order_received_packets_;
    if (previous_received_packet_size_ < last_received_packet_size_)
      ++num_out_of_order_large_received_packets_;
    UMA_HISTOGRAM_COUNTS_1M(
        "Net.QuicSession.OutOfOrderGapReceived",
        static_cast<base::HistogramBase::Sample>(last_received_packet_number_ -
                                                 header.packet_number));
  } else if (no_packet_received_after_ping_) {
    if (last_received_packet_number_.IsInitialized()) {
      UMA_HISTOGRAM_COUNTS_1M(
          "Net.QuicSession.PacketGapReceivedNearPing",
          static_cast<base::HistogramBase::Sample>(
              header.packet_number - last_received_packet_number_));
    }
    no_packet_received_after_ping_ = false;
  }
  last_received_packet_number_ = header.packet_number;
  event_logger_.OnPacketHeader(header, receive_time, level);
}

void QuicConnectionLogger::OnStreamFrame(const quic::QuicStreamFrame& frame) {
  event_logger_.OnStreamFrame(frame);
}

void QuicConnectionLogger::OnPathChallengeFrame(
    const quic::QuicPathChallengeFrame& frame) {
  event_logger_.OnPathChallengeFrame(frame);
}

void QuicConnectionLogger::OnPathResponseFrame(
    const quic::QuicPathResponseFrame& frame) {
  event_logger_.OnPathResponseFrame(frame);
}

void QuicConnectionLogger::OnCryptoFrame(const quic::QuicCryptoFrame& frame) {
  event_logger_.OnCryptoFrame(frame);
}

void QuicConnectionLogger::OnStopSendingFrame(
    const quic::QuicStopSendingFrame& frame) {
  base::UmaHistogramSparse("Net.QuicSession.StopSendingErrorCodeServer",
                           frame.error_code);
  event_logger_.OnStopSendingFrame(frame);
}

void QuicConnectionLogger::OnStreamsBlockedFrame(
    const quic::QuicStreamsBlockedFrame& frame) {
  event_logger_.OnStreamsBlockedFrame(frame);
}

void QuicConnectionLogger::OnMaxStreamsFrame(
    const quic::QuicMaxStreamsFrame& frame) {
  event_logger_.OnMaxStreamsFrame(frame);
}

void QuicConnectionLogger::OnIncomingAck(
    quic::QuicPacketNumber ack_packet_number,
    quic::EncryptionLevel ack_decrypted_level,
    const quic::QuicAckFrame& frame,
    quic::QuicTime ack_receive_time,
    quic::QuicPacketNumber largest_observed,
    bool rtt_updated,
    quic::QuicPacketNumber least_unacked_sent_packet) {
  const size_t kApproximateLargestSoloAckBytes = 100;
  if (last_received_packet_number_ - first_received_packet_number_ <
          received_acks_.size() &&
      last_received_packet_size_ < kApproximateLargestSoloAckBytes) {
    received_acks_[last_received_packet_number_ -
                   first_received_packet_number_] = true;
  }

  event_logger_.OnIncomingAck(ack_packet_number, ack_decrypted_level, frame,
                              ack_receive_time, largest_observed, rtt_updated,
                              least_unacked_sent_packet);
}

void QuicConnectionLogger::OnRstStreamFrame(
    const quic::QuicRstStreamFrame& frame) {
  base::UmaHistogramSparse("Net.QuicSession.RstStreamErrorCodeServer",
                           frame.error_code);
  event_logger_.OnRstStreamFrame(frame);
}

void QuicConnectionLogger::OnConnectionCloseFrame(
    const quic::QuicConnectionCloseFrame& frame) {
  event_logger_.OnConnectionCloseFrame(frame);
}

void QuicConnectionLogger::OnWindowUpdateFrame(
    const quic::QuicWindowUpdateFrame& frame,
    const quic::QuicTime& receive_time) {
  event_logger_.OnWindowUpdateFrame(frame, receive_time);
}

void QuicConnectionLogger::OnBlockedFrame(const quic::QuicBlockedFrame& frame) {
  ++num_blocked_frames_received_;
  event_logger_.OnBlockedFrame(frame);
}

void QuicConnectionLogger::OnGoAwayFrame(const quic::QuicGoAwayFrame& frame) {
  UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.GoAwayReceivedForConnectionMigration",
                        frame.error_code == quic::QUIC_ERROR_MIGRATING_PORT);

  event_logger_.OnGoAwayFrame(frame);
}

void QuicConnectionLogger::OnPingFrame(
    const quic::QuicPingFrame& frame,
    quic::QuicTime::Delta ping_received_delay) {
  event_logger_.OnPingFrame(frame, ping_received_delay);
}

void QuicConnectionLogger::OnPaddingFrame(const quic::QuicPaddingFrame& frame) {
  event_logger_.OnPaddingFrame(frame);
}

void QuicConnectionLogger::OnNewConnectionIdFrame(
    const quic::QuicNewConnectionIdFrame& frame) {
  event_logger_.OnNewConnectionIdFrame(frame);
}

void QuicConnectionLogger::OnNewTokenFrame(
    const quic::QuicNewTokenFrame& frame) {
  event_logger_.OnNewTokenFrame(frame);
}

void QuicConnectionLogger::OnRetireConnectionIdFrame(
    const quic::QuicRetireConnectionIdFrame& frame) {
  event_logger_.OnRetireConnectionIdFrame(frame);
}

void QuicConnectionLogger::OnMessageFrame(const quic::QuicMessageFrame& frame) {
  event_logger_.OnMessageFrame(frame);
}

void QuicConnectionLogger::OnHandshakeDoneFrame(
    const quic::QuicHandshakeDoneFrame& frame) {
  event_logger_.OnHandshakeDoneFrame(frame);
}

void QuicConnectionLogger::OnCoalescedPacketSent(
    const quic::QuicCoalescedPacket& coalesced_packet,
    size_t length) {
  event_logger_.OnCoalescedPacketSent(coalesced_packet, length);
}

void QuicConnectionLogger::OnVersionNegotiationPacket(
    const quic::QuicVersionNegotiationPacket& packet) {
  event_logger_.OnVersionNegotiationPacket(packet);
}

void QuicConnectionLogger::OnCryptoHandshakeMessageReceived(
    const quic::CryptoHandshakeMessage& message) {
  if (message.tag() == quic::kSHLO) {
    std::string_view address;
    quic::QuicSocketAddressCoder decoder;
    if (message.GetStringPiece(quic::kCADR, &address) &&
        decoder.Decode(address.data(), address.size())) {
      local_address_from_shlo_ =
          IPEndPoint(ToIPAddress(decoder.ip()), decoder.port());
      UMA_HISTOGRAM_ENUMERATION(
          "Net.QuicSession.ConnectionTypeFromPeer",
          GetRealAddressFamily(local_address_from_shlo_.address()),
          ADDRESS_FAMILY_LAST);

      int sample = GetAddressMismatch(local_address_from_shlo_,
                                      local_address_from_self_);
      // If `sample` is negative, we are seemingly talking to an older server
      // that does not support the feature, so we can't report the results in
      // the histogram.
      if (sample >= 0) {
        UMA_HISTOGRAM_ENUMERATION("Net.QuicSession.SelfShloAddressMismatch",
                                  static_cast<QuicAddressMismatch>(sample),
                                  QUIC_ADDRESS_MISMATCH_MAX);
      }
    }
  }
  event_logger_.OnCryptoHandshakeMessageReceived(message);
}

void QuicConnectionLogger::OnCryptoHandshakeMessageSent(
    const quic::CryptoHandshakeMessage& message) {
  event_logger_.OnCryptoHandshakeMessageSent(message);
}

void QuicConnectionLogger::OnConnectionClosed(
    const quic::QuicConnectionCloseFrame& frame,
    quic::ConnectionCloseSource source) {
  event_logger_.OnConnectionClosed(frame, source);
}

void QuicConnectionLogger::OnSuccessfulVersionNegotiation(
    const quic::ParsedQuicVersion& version) {
  event_logger_.OnSuccessfulVersionNegotiation(version);
}

void QuicConnectionLogger::UpdateReceivedFrameCounts(
    quic::QuicStreamId stream_id,
    int num_frames_received,
    int num_duplicate_frames_received) {
  if (!quic::QuicUtils::IsCryptoStreamId(session_->transport_version(),
                                         stream_id)) {
    num_frames_received_ += num_frames_received;
    num_duplicate_frames_received_ += num_duplicate_frames_received;
  }
}

void QuicConnectionLogger::OnCertificateVerified(
    const CertVerifyResult& result) {
  event_logger_.OnCertificateVerified(result);
}

float QuicConnectionLogger::ReceivedPacketLossRate() const {
  if (!largest_received_packet_number_.IsInitialized())
    return 0.0f;
  float num_packets =
      largest_received_packet_number_ - first_received_packet_number_ + 1;
  float num_missing = num_packets - num_packets_received_;
  return num_missing / num_packets;
}

void QuicConnectionLogger::OnRttChanged(quic::QuicTime::Delta rtt) const {
  // Notify socket performance watcher of the updated RTT value.
  if (!socket_performance_watcher_)
    return;

  int64_t microseconds = rtt.ToMicroseconds();
  if (microseconds != 0 &&
      socket_performance_watcher_->ShouldNotifyUpdatedRTT()) {
    socket_performance_watcher_->OnUpdatedRTTAvailable(
        base::Microseconds(rtt.ToMicroseconds()));
  }
}

void QuicConnectionLogger::OnTransportParametersSent(
    const quic::TransportParameters& transport_parameters) {
  event_logger_.OnTransportParametersSent(transport_parameters);
}

void QuicConnectionLogger::OnTransportParametersReceived(
    const quic::TransportParameters& transport_parameters) {
  event_logger_.OnTransportParametersReceived(transport_parameters);
}

void QuicConnectionLogger::OnTransportParametersResumed(
    const quic::TransportParameters& transport_parameters) {
  event_logger_.OnTransportParametersResumed(transport_parameters);
}

void QuicConnectionLogger::OnZeroRttRejected(int reason) {
  event_logger_.OnZeroRttRejected(reason);
}

void QuicConnectionLogger::OnEncryptedClientHelloSent(
    std::string_view client_hello) {
  event_logger_.OnEncryptedClientHelloSent(client_hello);
}

void QuicConnectionLogger::RecordAggregatePacketLossRate() const {
  // We don't report packet loss rates for short connections under 22 packets in
  // length to avoid tremendously anomalous contributions to our histogram.
  // (e.g., if we only got 5 packets, but lost 1, we'd otherwise
  // record a 20% loss in this histogram!). We may still get some strange data
  // (1 loss in 22 is still high :-/).
  if (!largest_received_packet_number_.IsInitialized() ||
      largest_received_packet_number_ - first_received_packet_number_ < 22) {
    return;
  }

  string prefix("Net.QuicSession.PacketLossRate_");
  base::HistogramBase* histogram = base::Histogram::FactoryGet(
      prefix + connection_description_, 1, 1000, 75,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(static_cast<base::HistogramBase::Sample>(
      ReceivedPacketLossRate() * 1000));
}

}  // namespace net
