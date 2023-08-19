// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_EVENT_LOGGER_H_
#define NET_QUIC_QUIC_EVENT_LOGGER_H_

#include "base/memory/raw_ptr.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/cert/cert_verify_result.h"
#include "net/log/net_log_with_source.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_connection.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_packet_creator.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_session.h"

namespace net {

// Logs QUIC-related events into the supplied NetLog.  Note that while most of
// the logging is done by registering this object as a debug delegate, some of
// the methods have to be called manually.
class NET_EXPORT_PRIVATE QuicEventLogger
    : public quic::QuicConnectionDebugVisitor,
      public quic::QuicPacketCreator::DebugDelegate {
 public:
  QuicEventLogger(quic::QuicSession* session, const NetLogWithSource& net_log);

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
  void OnPacketReceived(const quic::QuicSocketAddress& self_address,
                        const quic::QuicSocketAddress& peer_address,
                        const quic::QuicEncryptedPacket& packet) override;
  void OnUnauthenticatedHeader(const quic::QuicPacketHeader& header) override;
  void OnUndecryptablePacket(quic::EncryptionLevel decryption_level,
                             bool dropped) override;
  void OnAttemptingToProcessUndecryptablePacket(
      quic::EncryptionLevel decryption_level) override;
  void OnDuplicatePacket(quic::QuicPacketNumber packet_number) override;
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
  void OnTransportParametersSent(
      const quic::TransportParameters& transport_parameters) override;
  void OnTransportParametersReceived(
      const quic::TransportParameters& transport_parameters) override;
  void OnTransportParametersResumed(
      const quic::TransportParameters& transport_parameters) override;
  void OnZeroRttRejected(int reason) override;
  void OnEncryptedClientHelloSent(std::string_view client_hello) override;

  // Events that are not received via the visitor and have to be called manually
  // from the session.
  void OnCryptoHandshakeMessageReceived(
      const quic::CryptoHandshakeMessage& message);
  void OnCryptoHandshakeMessageSent(
      const quic::CryptoHandshakeMessage& message);
  void OnCertificateVerified(const CertVerifyResult& result);

 private:
  raw_ptr<quic::QuicSession> session_;  // Unowned.
  NetLogWithSource net_log_;

  // The quic::kCADR value provided by the server in ServerHello.
  IPEndPoint local_address_from_shlo_;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_EVENT_LOGGER_H_
