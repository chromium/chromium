// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_test_packet_printer.h"
#include "base/memory/raw_ptr.h"

#include <ostream>

#include "base/strings/string_number_conversions.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_framer.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quiche/quic/platform/api/quic_flags.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_test_utils.h"

namespace quic {

namespace {

class QuicPacketPrinter : public QuicFramerVisitorInterface {
 public:
  explicit QuicPacketPrinter(QuicFramer* framer, std::ostream* output)
      : framer_(framer), output_(output) {}

  // QuicFramerVisitorInterface implementation.
  void OnError(QuicFramer* framer) override {
    *output_ << "OnError: " << QuicErrorCodeToString(framer->error())
             << " detail: " << framer->detailed_error() << "\n";
  }
  bool OnProtocolVersionMismatch(ParsedQuicVersion received_version) override {
    framer_->set_version(received_version);
    *output_ << "OnProtocolVersionMismatch: "
             << ParsedQuicVersionToString(received_version) << "\n";
    return true;
  }
  void OnPacket() override { *output_ << "OnPacket\n"; }
  void OnVersionNegotiationPacket(
      const QuicVersionNegotiationPacket& packet) override {
    *output_ << "OnVersionNegotiationPacket\n";
  }
  void OnRetryPacket(QuicConnectionId original_connection_id,
                     QuicConnectionId new_connection_id,
                     std::string_view retry_token,
                     std::string_view retry_integrity_tag,
                     std::string_view retry_without_tag) override {
    *output_ << "OnRetryPacket\n";
  }
  bool OnUnauthenticatedPublicHeader(const QuicPacketHeader& header) override {
    *output_ << "OnUnauthenticatedPublicHeader: " << header;
    return true;
  }
  bool OnUnauthenticatedHeader(const QuicPacketHeader& header) override {
    *output_ << "OnUnauthenticatedHeader: " << header;
    return true;
  }
  void OnDecryptedPacket(size_t length, EncryptionLevel level) override {
    *output_ << "OnDecryptedPacket\n";
  }
  bool OnPacketHeader(const QuicPacketHeader& header) override {
    *output_ << "OnPacketHeader\n";
    return true;
  }
  void OnCoalescedPacket(const QuicEncryptedPacket& packet) override {
    *output_ << "OnCoalescedPacket\n";
  }
  void OnUndecryptablePacket(const QuicEncryptedPacket& packet,
                             EncryptionLevel decryption_level,
                             bool has_decryption_key) override {
    *output_ << "OnUndecryptablePacket, decryption_level: " << decryption_level
             << "\n";
  }
  bool OnStreamFrame(const QuicStreamFrame& frame) override {
    *output_ << "OnStreamFrame: " << frame;
    *output_ << "         data: { "
             << base::HexEncode(frame.data_buffer, frame.data_length) << " }\n";
    return true;
  }
  bool OnCryptoFrame(const QuicCryptoFrame& frame) override {
    *output_ << "OnCryptoFrame: " << frame;
    *output_ << "         data: { "
             << base::HexEncode(frame.data_buffer, frame.data_length) << " }\n";
    return true;
  }
  bool OnAckFrameStart(QuicPacketNumber largest_acked,
                       QuicTime::Delta /*ack_delay_time*/) override {
    *output_ << "OnAckFrameStart, largest_acked: " << largest_acked << "\n";
    return true;
  }
  bool OnAckRange(QuicPacketNumber start, QuicPacketNumber end) override {
    *output_ << "OnAckRange: [" << start << ", " << end << ")\n";
    return true;
  }
  bool OnAckTimestamp(QuicPacketNumber packet_number,
                      QuicTime timestamp) override {
    *output_ << "OnAckTimestamp: [" << packet_number << ", "
             << timestamp.ToDebuggingValue() << ")\n";
    return true;
  }
  bool OnAckFrameEnd(QuicPacketNumber start,
                     const std::optional<QuicEcnCounts>& ecn_counts) override {
    *output_ << "OnAckFrameEnd, start: " << start << ", "
             << ecn_counts.value_or(QuicEcnCounts()).ToString() << "\n";
    return true;
  }
  bool OnStopWaitingFrame(const QuicStopWaitingFrame& frame) override {
    *output_ << "OnStopWaitingFrame: " << frame;
    return true;
  }
  bool OnPaddingFrame(const QuicPaddingFrame& frame) override {
    *output_ << "OnPaddingFrame: " << frame;
    return true;
  }
  bool OnPingFrame(const QuicPingFrame& frame) override {
    *output_ << "OnPingFrame\n";
    return true;
  }
  bool OnRstStreamFrame(const QuicRstStreamFrame& frame) override {
    *output_ << "OnRstStreamFrame: " << frame;
    return true;
  }
  bool OnConnectionCloseFrame(const QuicConnectionCloseFrame& frame) override {
    // The frame printout will indicate whether it's a Google QUIC
    // CONNECTION_CLOSE, IETF QUIC CONNECTION_CLOSE/Transport, or IETF QUIC
    // CONNECTION_CLOSE/Application frame.
    *output_ << "OnConnectionCloseFrame: " << frame;
    return true;
  }
  bool OnNewConnectionIdFrame(const QuicNewConnectionIdFrame& frame) override {
    *output_ << "OnNewConnectionIdFrame: " << frame;
    return true;
  }
  bool OnRetireConnectionIdFrame(
      const QuicRetireConnectionIdFrame& frame) override {
    *output_ << "OnRetireConnectionIdFrame: " << frame;
    return true;
  }
  bool OnNewTokenFrame(const QuicNewTokenFrame& frame) override {
    *output_ << "OnNewTokenFrame: " << frame;
    return true;
  }
  bool OnStopSendingFrame(const QuicStopSendingFrame& frame) override {
    *output_ << "OnStopSendingFrame: " << frame;
    return true;
  }
  bool OnPathChallengeFrame(const QuicPathChallengeFrame& frame) override {
    *output_ << "OnPathChallengeFrame: " << frame;
    return true;
  }
  bool OnPathResponseFrame(const QuicPathResponseFrame& frame) override {
    *output_ << "OnPathResponseFrame: " << frame;
    return true;
  }
  bool OnGoAwayFrame(const QuicGoAwayFrame& frame) override {
    *output_ << "OnGoAwayFrame: " << frame;
    return true;
  }
  bool OnMaxStreamsFrame(const QuicMaxStreamsFrame& frame) override {
    *output_ << "OnMaxStreamsFrame: " << frame;
    return true;
  }
  bool OnStreamsBlockedFrame(const QuicStreamsBlockedFrame& frame) override {
    *output_ << "OnStreamsBlockedFrame: " << frame;
    return true;
  }
  void OnKeyUpdate(KeyUpdateReason reason) override {
    *output_ << "OnKeyUpdate: " << reason << "\n";
  }
  void OnDecryptedFirstPacketInKeyPhase() override {
    *output_ << "OnDecryptedFirstPacketInKeyPhase\n";
  }
  std::unique_ptr<QuicDecrypter> AdvanceKeysAndCreateCurrentOneRttDecrypter()
      override {
    *output_ << "AdvanceKeysAndCreateCurrentOneRttDecrypter\n";
    return nullptr;
  }
  std::unique_ptr<QuicEncrypter> CreateCurrentOneRttEncrypter() override {
    *output_ << "CreateCurrentOneRttEncrypter\n";
    return nullptr;
  }
  bool OnWindowUpdateFrame(const QuicWindowUpdateFrame& frame) override {
    *output_ << "OnWindowUpdateFrame: " << frame;
    return true;
  }
  bool OnBlockedFrame(const QuicBlockedFrame& frame) override {
    *output_ << "OnBlockedFrame: " << frame;
    return true;
  }
  bool OnMessageFrame(const QuicMessageFrame& frame) override {
    *output_ << "OnMessageFrame: " << frame;
    // In a test context, `frame.data` should always be set.
    CHECK(frame.data);
    *output_ << "         data: { "
             << base::HexEncode(frame.data, frame.message_length) << " }\n";
    return true;
  }
  bool OnHandshakeDoneFrame(const QuicHandshakeDoneFrame& frame) override {
    *output_ << "OnHandshakeDoneFrame: " << frame;
    return true;
  }
  bool OnAckFrequencyFrame(const QuicAckFrequencyFrame& frame) override {
    *output_ << "OnAckFrequencyFrame: " << frame;
    return true;
  }
  bool OnResetStreamAtFrame(const QuicResetStreamAtFrame& frame) override {
    *output_ << "OnResetStreamAtFrame: " << frame;
    return true;
  }
  void OnPacketComplete() override { *output_ << "OnPacketComplete\n"; }
  bool IsValidStatelessResetToken(
      const StatelessResetToken& token) const override {
    *output_ << "IsValidStatelessResetToken\n";
    return false;
  }
  void OnAuthenticatedIetfStatelessResetPacket(
      const QuicIetfStatelessResetPacket& packet) override {
    *output_ << "OnAuthenticatedIetfStatelessResetPacket\n";
  }

 private:
  raw_ptr<QuicFramer> framer_;  // Unowned.
  mutable raw_ptr<std::ostream> output_;
};

}  // namespace

}  // namespace quic

namespace net {

std::string QuicPacketPrinter::PrintWrite(const std::string& data) {
  quic::ParsedQuicVersionVector versions = {version_};
  // Fake a time since we're not actually generating acks.
  quic::QuicTime start(quic::QuicTime::Zero());
  // Construct a server framer as this will be processing packets from
  // the client.
  quic::QuicFramer framer(versions, start, quic::Perspective::IS_SERVER,
                          quic::kQuicDefaultConnectionIdLength);
  std::ostringstream stream;
  quic::QuicPacketPrinter visitor(&framer, &stream);
  framer.set_visitor(&visitor);

  if (version_.KnowsWhichDecrypterToUse()) {
    framer.InstallDecrypter(
        quic::ENCRYPTION_FORWARD_SECURE,
        std::make_unique<quic::test::TaggingDecrypter>());  // IN-TEST
  } else {
    framer.SetDecrypter(
        quic::ENCRYPTION_FORWARD_SECURE,
        std::make_unique<quic::test::TaggingDecrypter>());  // IN-TEST
  }

  quic::QuicEncryptedPacket encrypted(data.c_str(), data.length());
  framer.ProcessPacket(encrypted);
  return stream.str() + "\n\n";
}

}  // namespace net
