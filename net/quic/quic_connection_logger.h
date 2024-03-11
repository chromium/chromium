// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_CONNECTION_LOGGER_H_
#define NET_QUIC_QUIC_CONNECTION_LOGGER_H_

#include <stddef.h>

#include <bitset>

#include "base/memory/raw_ptr.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/quic_event_logger.h"
#include "net/socket/socket_performance_watcher.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/crypto_handshake_message.h"
#include "net/third_party/quiche/src/quiche/quic/core/http/quic_spdy_session.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_connection.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_packets.h"

namespace net {

// Handles both NetLog support and UMA histograms for QUIC.
class NET_EXPORT_PRIVATE QuicConnectionLogger
    : public quic::QuicConnectionDebugVisitor,
      public quic::QuicPacketCreator::DebugDelegate {
 public:
  QuicConnectionLogger(
      quic::QuicSession* session,
      const char* const connection_description,
      std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
      const NetLogWithSource& net_log);

  QuicConnectionLogger(const QuicConnectionLogger&) = delete;
  QuicConnectionLogger& operator=(const QuicConnectionLogger&) = delete;

  ~QuicConnectionLogger() override;

  // quic::QuicPacketCreator::DebugDelegateInterface
  void OnFrameAddedToPacket(const quic::QuicFrame& frame) override;
  void OnStreamFrameCoalesced(const quic::QuicStreamFrame& frame) override;

  // quic::QuicConnectionDebugVisitor Interface
  void OnPacketSent(quic::QuicPacketNumber packet_number,
                    quic::QuicPacketLength packet_length,
                    bool has_crypto_handshake,
                    quic::TransmissionType transmission_type,
                    quic::EncryptionLevel encryption_level,
                    const quic::QuicFrames& retransmittable_frames,
                    const quic::QuicFrames& nonretransmittable_frames,
                    quic::QuicTime sent_time,
                    uint32_t batch_id) override;
  void OnIncomingAck(quic::QuicPacketNumber ack_packet_number,
                     quic::EncryptionLevel ack_decrypted_level,
                     const quic::QuicAckFrame& frame,
                     quic::QuicTime ack_receive_time,
                     quic::QuicPacketNumber largest_observed,
                     bool rtt_updated,
                     quic::QuicPacketNumber least_unacked_sent_packet) override;
  void OnPacketLoss(quic::QuicPacketNumber lost_packet_number,
                    quic::EncryptionLevel encryption_level,
                    quic::TransmissionType transmission_type,
                    quic::QuicTime detection_time) override;
  void OnConfigProcessed(const SendParameters& parameters) override;
  void OnPingSent() override;
  void OnPacketReceived(const quic::QuicSocketAddress& self_address,
                        const quic::QuicSocketAddress& peer_address,
                        const quic::QuicEncryptedPacket& packet) override;
  void OnUnauthenticatedHeader(const quic::QuicPacketHeader& header) override;
  void OnIncorrectConnectionId(quic::QuicConnectionId connection_id) override;
  void OnUndecryptablePacket(quic::EncryptionLevel decryption_level,
                             bool dropped) override;
  void OnAttemptingToProcessUndecryptablePacket(
      quic::EncryptionLevel decryption_level) override;
  void OnDuplicatePacket(quic::QuicPacketNumber packet_number) override;
  void OnProtocolVersionMismatch(quic::ParsedQuicVersion version) override;
  void OnPacketHeader(const quic::QuicPacketHeader& header,
                      quic::QuicTime receive_time,
                      quic::EncryptionLevel level) override;
  void OnPathChallengeFrame(const quic::QuicPathChallengeFrame& frame) override;
  void OnPathResponseFrame(const quic::QuicPathResponseFrame& frame) override;
  void OnCryptoFrame(const quic::QuicCryptoFrame& frame) override;
  void OnStopSendingFrame(const quic::QuicStopSendingFrame& frame) override;
  void OnStreamsBlockedFrame(
      const quic::QuicStreamsBlockedFrame& frame) override;
  void OnMaxStreamsFrame(const quic::QuicMaxStreamsFrame& frame) override;
  void OnStreamFrame(const quic::QuicStreamFrame& frame) override;
  void OnRstStreamFrame(const quic::QuicRstStreamFrame& frame) override;
  void OnConnectionCloseFrame(
      const quic::QuicConnectionCloseFrame& frame) override;
  void OnWindowUpdateFrame(const quic::QuicWindowUpdateFrame& frame,
                           const quic::QuicTime& receive_time) override;
  void OnBlockedFrame(const quic::QuicBlockedFrame& frame) override;
  void OnGoAwayFrame(const quic::QuicGoAwayFrame& frame) override;
  void OnPingFrame(const quic::QuicPingFrame& frame,
                   quic::QuicTime::Delta ping_received_delay) override;
  void OnPaddingFrame(const quic::QuicPaddingFrame& frame) override;
  void OnNewConnectionIdFrame(
      const quic::QuicNewConnectionIdFrame& frame) override;
  void OnNewTokenFrame(const quic::QuicNewTokenFrame& frame) override;
  void OnRetireConnectionIdFrame(
      const quic::QuicRetireConnectionIdFrame& frame) override;
  void OnMessageFrame(const quic::QuicMessageFrame& frame) override;
  void OnHandshakeDoneFrame(const quic::QuicHandshakeDoneFrame& frame) override;
  void OnCoalescedPacketSent(const quic::QuicCoalescedPacket& coalesced_packet,
                             size_t length) override;
  void OnVersionNegotiationPacket(
      const quic::QuicVersionNegotiationPacket& packet) override;
  void OnConnectionClosed(const quic::QuicConnectionCloseFrame& frame,
                          quic::ConnectionCloseSource source) override;
  void OnSuccessfulVersionNegotiation(
      const quic::ParsedQuicVersion& version) override;
  void OnRttChanged(quic::QuicTime::Delta rtt) const override;
  void OnTransportParametersSent(
      const quic::TransportParameters& transport_parameters) override;
  void OnTransportParametersReceived(
      const quic::TransportParameters& transport_parameters) override;
  void OnTransportParametersResumed(
      const quic::TransportParameters& transport_parameters) override;
  void OnZeroRttRejected(int reason) override;
  void OnEncryptedClientHelloSent(std::string_view client_hello) override;

  void OnCryptoHandshakeMessageReceived(
      const quic::CryptoHandshakeMessage& message);
  void OnCryptoHandshakeMessageSent(
      const quic::CryptoHandshakeMessage& message);
  void UpdateReceivedFrameCounts(quic::QuicStreamId stream_id,
                                 int num_frames_received,
                                 int num_duplicate_frames_received);
  void OnCertificateVerified(const CertVerifyResult& result);

  // Returns connection's overall packet loss rate in fraction.
  float ReceivedPacketLossRate() const;

 private:
  // For connections longer than 21 received packets, this call will calculate
  // the overall packet loss rate, and record it into a histogram.
  void RecordAggregatePacketLossRate() const;

  raw_ptr<quic::QuicSession> session_;  // Unowned.
  // The last packet number received.
  quic::QuicPacketNumber last_received_packet_number_;
  // The size of the most recently received packet.
  size_t last_received_packet_size_ = 0;
  // True if a PING frame has been sent and no packet has been received.
  bool no_packet_received_after_ping_ = false;
  // The size of the previously received packet.
  size_t previous_received_packet_size_ = 0;
  // The first received packet number. Used as the left edge of
  // received_packets_ and received_acks_. In the case where packets are
  // received out of order, packets with numbers smaller than
  // first_received_packet_number_ will not be logged.
  quic::QuicPacketNumber first_received_packet_number_;
  // The largest packet number received.  In the case where a packet is
  // received late (out of order), this value will not be updated.
  quic::QuicPacketNumber largest_received_packet_number_;
  // Number of times that the current received packet number is
  // smaller than the last received packet number.
  size_t num_out_of_order_received_packets_ = 0;
  // Number of times that the current received packet number is
  // smaller than the last received packet number and where the
  // size of the current packet is larger than the size of the previous
  // packet.
  size_t num_out_of_order_large_received_packets_ = 0;
  // The number of times that OnPacketHeader was called.
  // If the network replicates packets, then this number may be slightly
  // different from the real number of distinct packets received.
  quic::QuicPacketCount num_packets_received_ = 0;
  // The quic::kCADR value provided by the server in ServerHello.
  IPEndPoint local_address_from_shlo_;
  // The first local address from which a packet was received.
  IPEndPoint local_address_from_self_;
  // Count of the number of frames received.
  int num_frames_received_ = 0;
  // Count of the number of duplicate frames received.
  int num_duplicate_frames_received_ = 0;
  // Count of the number of packets received with incorrect connection IDs.
  int num_incorrect_connection_ids_ = 0;
  // Count of the number of undecryptable packets received.
  int num_undecryptable_packets_ = 0;
  // Count of the number of duplicate packets received.
  int num_duplicate_packets_ = 0;
  // Count of the number of BLOCKED frames received.
  int num_blocked_frames_received_ = 0;
  // Count of the number of BLOCKED frames sent.
  int num_blocked_frames_sent_ = 0;
  // Vector of initial packets status' indexed by packet numbers, where
  // false means never received. We track 150 packets starting from
  // first_received_packet_number_.
  std::bitset<150> received_packets_;
  // Vector to indicate which of the initial 150 received packets turned out to
  // contain solo ACK frames.  An element is true iff an ACK frame was in the
  // corresponding packet, and there was very little else.
  std::bitset<150> received_acks_;
  // The available type of connection (WiFi, 3G, etc.) when connection was first
  // used.
  const char* const connection_description_;
  // Receives notifications regarding the performance of the underlying socket
  // for the QUIC connection. May be null.
  const std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher_;

  QuicEventLogger event_logger_;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CONNECTION_LOGGER_H_
