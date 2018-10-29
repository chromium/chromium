// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off

// Dumps out the decryptable contents of a QUIC packet in a human-readable way.
// If the packet is null encrypted, this will dump full packet contents.
// Otherwise it will dump the header, and fail with an error that the
// packet is undecryptable.
//
// Usage: quic_packet_printer server|client <hex dump of packet>
//
// Example input:
// quic_packet_printer server 0c6b810308320f24c004a939a38a2e3fd6ca589917f200400
// 201b80b0100501c0700060003023d0000001c00556e656e637279707465642073747265616d2
// 064617461207365656e
//
// Example output:
// OnPacket
// OnUnauthenticatedPublicHeader
// OnUnauthenticatedHeader: { connection_id: 13845207862000976235,
// connection_id_length:8, packet_number_length:1, multipath_flag: 0,
// reset_flag: 0, version_flag: 0, path_id: , packet_number: 4}
// OnDecryptedPacket
// OnPacketHeader
// OnAckFrame:  largest_observed: 1 ack_delay_time: 3000
// missing_packets: [  ] is_truncated: 0 received_packets: [ 1 at 466016  ]
// OnStopWaitingFrame
// OnConnectionCloseFrame: error_code { 61 } error_details { Unencrypted stream
// data seen }

// clang-format on

#include <iostream>
#include <string>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "net/third_party/quic/core/quic_framer.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/platform/api/quic_text_utils.h"

// If set, specify the QUIC version to use.
quic::QuicString FLAGS_quic_version = "";

namespace {

quic::QuicString ArgToString(base::CommandLine::StringType arg) {
#if defined(OS_WIN)
  return base::UTF16ToASCII(arg);
#else
  return arg;
#endif
}
}  // namespace

namespace quic {

class QuicPacketPrinter : public QuicFramerVisitorInterface {
 public:
  explicit QuicPacketPrinter(QuicFramer* framer) : framer_(framer) {}

  void OnError(QuicFramer* framer) override {
    std::cerr << "OnError: " << QuicErrorCodeToString(framer->error())
              << " detail: " << framer->detailed_error() << "\n";
  }
  bool OnProtocolVersionMismatch(ParsedQuicVersion received_version) override {
    framer_->set_version(received_version);
    std::cerr << "OnProtocolVersionMismatch: "
              << ParsedQuicVersionToString(received_version) << "\n";
    return true;
  }
  void OnPacket() override { std::cerr << "OnPacket\n"; }
  void OnPublicResetPacket(const QuicPublicResetPacket& packet) override {
    std::cerr << "OnPublicResetPacket\n";
  }
  void OnVersionNegotiationPacket(
      const QuicVersionNegotiationPacket& packet) override {
    std::cerr << "OnVersionNegotiationPacket\n";
  }
  bool OnUnauthenticatedPublicHeader(const QuicPacketHeader& header) override {
    std::cerr << "OnUnauthenticatedPublicHeader\n";
    return true;
  }
  bool OnUnauthenticatedHeader(const QuicPacketHeader& header) override {
    std::cerr << "OnUnauthenticatedHeader: " << header;
    return true;
  }
  void OnDecryptedPacket(EncryptionLevel level) override {
    // This only currently supports "decrypting" null encrypted packets.
    DCHECK_EQ(ENCRYPTION_NONE, level);
    std::cerr << "OnDecryptedPacket\n";
  }
  bool OnPacketHeader(const QuicPacketHeader& header) override {
    std::cerr << "OnPacketHeader\n";
    return true;
  }
  bool OnStreamFrame(const QuicStreamFrame& frame) override {
    std::cerr << "OnStreamFrame: " << frame;
    std::cerr << "         data: { "
              << QuicTextUtils::HexEncode(frame.data_buffer, frame.data_length)
              << " }\n";
    return true;
  }
  bool OnCryptoFrame(const QuicCryptoFrame& frame) override {
    std::cerr << "OnCryptoFrame: " << frame;
    std::cerr << "         data: { "
              << QuicTextUtils::HexEncode(frame.data_buffer, frame.data_length)
              << " }\n";
    return true;
  }
  bool OnAckFrameStart(QuicPacketNumber largest_acked,
                       QuicTime::Delta /*ack_delay_time*/) override {
    std::cerr << "OnAckFrameStart, largest_acked: " << largest_acked;
    return true;
  }
  bool OnAckRange(QuicPacketNumber start, QuicPacketNumber end) override {
    std::cerr << "OnAckRange: [" << start << ", " << end << ")";
    return true;
  }
  bool OnAckTimestamp(QuicPacketNumber packet_number,
                      QuicTime timestamp) override {
    std::cerr << "OnAckTimestamp: [" << packet_number << ", "
              << timestamp.ToDebuggingValue() << ")";
    return true;
  }
  bool OnAckFrameEnd(QuicPacketNumber start) override {
    std::cerr << "OnAckFrameEnd, start: " << start;
    return true;
  }
  bool OnStopWaitingFrame(const QuicStopWaitingFrame& frame) override {
    std::cerr << "OnStopWaitingFrame: " << frame;
    return true;
  }
  bool OnPaddingFrame(const QuicPaddingFrame& frame) override {
    std::cerr << "OnPaddingFrame: " << frame;
    return true;
  }
  bool OnPingFrame(const QuicPingFrame& frame) override {
    std::cerr << "OnPingFrame\n";
    return true;
  }
  bool OnRstStreamFrame(const QuicRstStreamFrame& frame) override {
    std::cerr << "OnRstStreamFrame: " << frame;
    return true;
  }
  bool OnConnectionCloseFrame(const QuicConnectionCloseFrame& frame) override {
    std::cerr << "OnConnectionCloseFrame: " << frame;
    return true;
  }
  bool OnApplicationCloseFrame(
      const QuicApplicationCloseFrame& frame) override {
    std::cerr << "OnApplicationCloseFrame: " << frame;
    return true;
  }
  bool OnNewConnectionIdFrame(const QuicNewConnectionIdFrame& frame) override {
    std::cerr << "OnNewConnectionIdFrame: " << frame;
    return true;
  }
  bool OnRetireConnectionIdFrame(
      const QuicRetireConnectionIdFrame& frame) override {
    std::cerr << "OnRetireConnectionIdFrame: " << frame;
    return true;
  }
  bool OnNewTokenFrame(const QuicNewTokenFrame& frame) override {
    std::cerr << "OnNewTokenFrame: " << frame;
    return true;
  }
  bool OnStopSendingFrame(const QuicStopSendingFrame& frame) override {
    std::cerr << "OnStopSendingFrame: " << frame;
    return true;
  }
  bool OnPathChallengeFrame(const QuicPathChallengeFrame& frame) override {
    std::cerr << "OnPathChallengeFrame: " << frame;
    return true;
  }
  bool OnPathResponseFrame(const QuicPathResponseFrame& frame) override {
    std::cerr << "OnPathResponseFrame: " << frame;
    return true;
  }
  bool OnGoAwayFrame(const QuicGoAwayFrame& frame) override {
    std::cerr << "OnGoAwayFrame: " << frame;
    return true;
  }
  bool OnMaxStreamIdFrame(const QuicMaxStreamIdFrame& frame) override {
    std::cerr << "OnMaxStreamIdFrame: " << frame;
    return true;
  }
  bool OnStreamIdBlockedFrame(const QuicStreamIdBlockedFrame& frame) override {
    std::cerr << "OnStreamIdBlockedFrame: " << frame;
    return true;
  }
  bool OnWindowUpdateFrame(const QuicWindowUpdateFrame& frame) override {
    std::cerr << "OnWindowUpdateFrame: " << frame;
    return true;
  }
  bool OnBlockedFrame(const QuicBlockedFrame& frame) override {
    std::cerr << "OnBlockedFrame: " << frame;
    return true;
  }
  bool OnMessageFrame(const QuicMessageFrame& frame) override {
    std::cerr << "OnMessageFrame: " << frame;
    return true;
  }
  void OnPacketComplete() override { std::cerr << "OnPacketComplete\n"; }
  bool IsValidStatelessResetToken(QuicUint128 token) const override {
    std::cerr << "IsValidStatelessResetToken\n";
    return false;
  }
  void OnAuthenticatedIetfStatelessResetPacket(
      const QuicIetfStatelessResetPacket& packet) override {
    std::cerr << "OnAuthenticatedIetfStatelessResetPacket\n";
  }

 private:
  QuicFramer* framer_;  // Unowned.
};

}  // namespace quic

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine* line = base::CommandLine::ForCurrentProcess();
  const base::CommandLine::StringVector& args = line->GetArgs();

  if (args.size() != 3) {
    std::cerr << "Missing argument " << argc << ". (Usage: " << argv[0]
              << " client|server <hex>\n";
    return 1;
  }

  if (line->HasSwitch("quic_version")) {
    FLAGS_quic_version = line->GetSwitchValueASCII("quic_version");
  }

  quic::QuicString perspective_string = ArgToString(args[0]);
  quic::Perspective perspective;
  if (perspective_string == "client") {
    perspective = quic::Perspective::IS_CLIENT;
  } else if (perspective_string == "server") {
    perspective = quic::Perspective::IS_SERVER;
  } else {
    std::cerr << "Invalid perspective. " << perspective_string
              << " Usage: " << args[0] << " client|server <hex>\n";
    return 1;
  }
  quic::QuicString hex = quic::QuicTextUtils::HexDecode(argv[2]);
  quic::ParsedQuicVersionVector versions = quic::AllSupportedVersions();
  // Fake a time since we're not actually generating acks.
  quic::QuicTime start(quic::QuicTime::Zero());
  quic::QuicFramer framer(versions, start, perspective);
  if (!FLAGS_quic_version.empty()) {
    for (quic::ParsedQuicVersion version : versions) {
      if (quic::QuicVersionToString(version.transport_version) ==
          FLAGS_quic_version) {
        framer.set_version(version);
      }
    }
  }
  quic::QuicPacketPrinter visitor(&framer);
  framer.set_visitor(&visitor);
  quic::QuicEncryptedPacket encrypted(hex.c_str(), hex.length());
  return framer.ProcessPacket(encrypted);
}
