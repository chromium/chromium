// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/test_tools/simple_quic_framer.h"

#include <memory>

#include "base/macros.h"
#include "net/third_party/quic/core/crypto/quic_decrypter.h"
#include "net/third_party/quic/core/crypto/quic_encrypter.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"

namespace quic {
namespace test {

class SimpleFramerVisitor : public QuicFramerVisitorInterface {
 public:
  SimpleFramerVisitor() : error_(QUIC_NO_ERROR) {}
  SimpleFramerVisitor(const SimpleFramerVisitor&) = delete;
  SimpleFramerVisitor& operator=(const SimpleFramerVisitor&) = delete;

  ~SimpleFramerVisitor() override {}

  void OnError(QuicFramer* framer) override { error_ = framer->error(); }

  bool OnProtocolVersionMismatch(ParsedQuicVersion version) override {
    return false;
  }

  void OnPacket() override {}
  void OnPublicResetPacket(const QuicPublicResetPacket& packet) override {
    public_reset_packet_ = QuicMakeUnique<QuicPublicResetPacket>((packet));
  }
  void OnVersionNegotiationPacket(
      const QuicVersionNegotiationPacket& packet) override {
    version_negotiation_packet_ =
        QuicMakeUnique<QuicVersionNegotiationPacket>((packet));
  }

  bool OnUnauthenticatedPublicHeader(const QuicPacketHeader& header) override {
    return true;
  }
  bool OnUnauthenticatedHeader(const QuicPacketHeader& header) override {
    return true;
  }
  void OnDecryptedPacket(EncryptionLevel level) override {}
  bool OnPacketHeader(const QuicPacketHeader& header) override {
    has_header_ = true;
    header_ = header;
    return true;
  }

  bool OnStreamFrame(const QuicStreamFrame& frame) override {
    // Save a copy of the data so it is valid after the packet is processed.
    QuicString* string_data =
        new QuicString(frame.data_buffer, frame.data_length);
    stream_data_.push_back(QuicWrapUnique(string_data));
    // TODO(ianswett): A pointer isn't necessary with emplace_back.
    stream_frames_.push_back(QuicMakeUnique<QuicStreamFrame>(
        frame.stream_id, frame.fin, frame.offset,
        QuicStringPiece(*string_data)));
    return true;
  }

  bool OnCryptoFrame(const QuicCryptoFrame& frame) override {
    // TODO(nharper): Implement this.
    return false;
  }

  bool OnAckFrameStart(QuicPacketNumber largest_acked,
                       QuicTime::Delta ack_delay_time) override {
    QuicAckFrame ack_frame;
    ack_frame.largest_acked = largest_acked;
    ack_frame.ack_delay_time = ack_delay_time;
    ack_frames_.push_back(ack_frame);
    return true;
  }

  bool OnAckRange(QuicPacketNumber start, QuicPacketNumber end) override {
    DCHECK(!ack_frames_.empty());
    ack_frames_[ack_frames_.size() - 1].packets.AddRange(start, end);
    return true;
  }

  bool OnAckTimestamp(QuicPacketNumber packet_number,
                      QuicTime timestamp) override {
    return true;
  }

  bool OnAckFrameEnd(QuicPacketNumber /*start*/) override { return true; }

  bool OnStopWaitingFrame(const QuicStopWaitingFrame& frame) override {
    stop_waiting_frames_.push_back(frame);
    return true;
  }

  bool OnPaddingFrame(const QuicPaddingFrame& frame) override {
    padding_frames_.push_back(frame);
    return true;
  }

  bool OnPingFrame(const QuicPingFrame& frame) override {
    ping_frames_.push_back(frame);
    return true;
  }

  bool OnRstStreamFrame(const QuicRstStreamFrame& frame) override {
    rst_stream_frames_.push_back(frame);
    return true;
  }

  bool OnConnectionCloseFrame(const QuicConnectionCloseFrame& frame) override {
    connection_close_frames_.push_back(frame);
    return true;
  }

  bool OnApplicationCloseFrame(
      const QuicApplicationCloseFrame& frame) override {
    application_close_frames_.push_back(frame);
    return true;
  }

  bool OnNewConnectionIdFrame(const QuicNewConnectionIdFrame& frame) override {
    new_connection_id_frames_.push_back(frame);
    return true;
  }

  bool OnRetireConnectionIdFrame(
      const QuicRetireConnectionIdFrame& frame) override {
    retire_connection_id_frames_.push_back(frame);
    return true;
  }

  bool OnNewTokenFrame(const QuicNewTokenFrame& frame) override {
    new_token_frames_.push_back(frame);
    return true;
  }

  bool OnStopSendingFrame(const QuicStopSendingFrame& frame) override {
    stop_sending_frames_.push_back(frame);
    return true;
  }

  bool OnPathChallengeFrame(const QuicPathChallengeFrame& frame) override {
    path_challenge_frames_.push_back(frame);
    return true;
  }

  bool OnPathResponseFrame(const QuicPathResponseFrame& frame) override {
    path_response_frames_.push_back(frame);
    return true;
  }

  bool OnGoAwayFrame(const QuicGoAwayFrame& frame) override {
    goaway_frames_.push_back(frame);
    return true;
  }
  bool OnMaxStreamIdFrame(const QuicMaxStreamIdFrame& frame) override {
    max_stream_id_frames_.push_back(frame);
    return true;
  }

  bool OnStreamIdBlockedFrame(const QuicStreamIdBlockedFrame& frame) override {
    stream_id_blocked_frames_.push_back(frame);
    return true;
  }

  bool OnWindowUpdateFrame(const QuicWindowUpdateFrame& frame) override {
    window_update_frames_.push_back(frame);
    return true;
  }

  bool OnBlockedFrame(const QuicBlockedFrame& frame) override {
    blocked_frames_.push_back(frame);
    return true;
  }

  bool OnMessageFrame(const QuicMessageFrame& frame) override {
    message_frames_.push_back(frame);
    return true;
  }

  void OnPacketComplete() override {}

  bool IsValidStatelessResetToken(QuicUint128 token) const override {
    return false;
  }

  void OnAuthenticatedIetfStatelessResetPacket(
      const QuicIetfStatelessResetPacket& packet) override {
    stateless_reset_packet_ =
        QuicMakeUnique<QuicIetfStatelessResetPacket>(packet);
  }

  const QuicPacketHeader& header() const { return header_; }
  const std::vector<QuicAckFrame>& ack_frames() const { return ack_frames_; }
  const std::vector<QuicConnectionCloseFrame>& connection_close_frames() const {
    return connection_close_frames_;
  }
  const std::vector<QuicApplicationCloseFrame>& application_close_frames()
      const {
    return application_close_frames_;
  }
  const std::vector<QuicGoAwayFrame>& goaway_frames() const {
    return goaway_frames_;
  }
  const std::vector<QuicMaxStreamIdFrame>& max_stream_id_frames() const {
    return max_stream_id_frames_;
  }
  const std::vector<QuicStreamIdBlockedFrame>& stream_id_blocked_frames()
      const {
    return stream_id_blocked_frames_;
  }
  const std::vector<QuicRstStreamFrame>& rst_stream_frames() const {
    return rst_stream_frames_;
  }
  const std::vector<std::unique_ptr<QuicStreamFrame>>& stream_frames() const {
    return stream_frames_;
  }
  const std::vector<QuicStopWaitingFrame>& stop_waiting_frames() const {
    return stop_waiting_frames_;
  }
  const std::vector<QuicPingFrame>& ping_frames() const { return ping_frames_; }
  const std::vector<QuicMessageFrame>& message_frames() const {
    return message_frames_;
  }
  const std::vector<QuicWindowUpdateFrame>& window_update_frames() const {
    return window_update_frames_;
  }
  const std::vector<QuicPaddingFrame>& padding_frames() const {
    return padding_frames_;
  }
  const std::vector<QuicPathChallengeFrame>& path_challenge_frames() const {
    return path_challenge_frames_;
  }
  const std::vector<QuicPathResponseFrame>& path_response_frames() const {
    return path_response_frames_;
  }
  const QuicVersionNegotiationPacket* version_negotiation_packet() const {
    return version_negotiation_packet_.get();
  }

 private:
  QuicErrorCode error_;
  bool has_header_;
  QuicPacketHeader header_;
  std::unique_ptr<QuicVersionNegotiationPacket> version_negotiation_packet_;
  std::unique_ptr<QuicPublicResetPacket> public_reset_packet_;
  std::unique_ptr<QuicIetfStatelessResetPacket> stateless_reset_packet_;
  std::vector<QuicAckFrame> ack_frames_;
  std::vector<QuicStopWaitingFrame> stop_waiting_frames_;
  std::vector<QuicPaddingFrame> padding_frames_;
  std::vector<QuicPingFrame> ping_frames_;
  std::vector<std::unique_ptr<QuicStreamFrame>> stream_frames_;
  std::vector<QuicRstStreamFrame> rst_stream_frames_;
  std::vector<QuicGoAwayFrame> goaway_frames_;
  std::vector<QuicStreamIdBlockedFrame> stream_id_blocked_frames_;
  std::vector<QuicMaxStreamIdFrame> max_stream_id_frames_;
  std::vector<QuicConnectionCloseFrame> connection_close_frames_;
  std::vector<QuicApplicationCloseFrame> application_close_frames_;
  std::vector<QuicStopSendingFrame> stop_sending_frames_;
  std::vector<QuicPathChallengeFrame> path_challenge_frames_;
  std::vector<QuicPathResponseFrame> path_response_frames_;
  std::vector<QuicWindowUpdateFrame> window_update_frames_;
  std::vector<QuicBlockedFrame> blocked_frames_;
  std::vector<QuicNewConnectionIdFrame> new_connection_id_frames_;
  std::vector<QuicRetireConnectionIdFrame> retire_connection_id_frames_;
  std::vector<QuicNewTokenFrame> new_token_frames_;
  std::vector<QuicMessageFrame> message_frames_;
  std::vector<std::unique_ptr<QuicString>> stream_data_;
};

SimpleQuicFramer::SimpleQuicFramer()
    : framer_(AllSupportedVersions(),
              QuicTime::Zero(),
              Perspective::IS_SERVER) {}

SimpleQuicFramer::SimpleQuicFramer(
    const ParsedQuicVersionVector& supported_versions)
    : framer_(supported_versions, QuicTime::Zero(), Perspective::IS_SERVER) {}

SimpleQuicFramer::SimpleQuicFramer(
    const ParsedQuicVersionVector& supported_versions,
    Perspective perspective)
    : framer_(supported_versions, QuicTime::Zero(), perspective) {}

SimpleQuicFramer::~SimpleQuicFramer() {}

bool SimpleQuicFramer::ProcessPacket(const QuicEncryptedPacket& packet) {
  visitor_ = QuicMakeUnique<SimpleFramerVisitor>();
  framer_.set_visitor(visitor_.get());
  return framer_.ProcessPacket(packet);
}

void SimpleQuicFramer::Reset() {
  visitor_ = QuicMakeUnique<SimpleFramerVisitor>();
}

const QuicPacketHeader& SimpleQuicFramer::header() const {
  return visitor_->header();
}

const QuicVersionNegotiationPacket*
SimpleQuicFramer::version_negotiation_packet() const {
  return visitor_->version_negotiation_packet();
}

QuicFramer* SimpleQuicFramer::framer() {
  return &framer_;
}

size_t SimpleQuicFramer::num_frames() const {
  return ack_frames().size() + goaway_frames().size() +
         rst_stream_frames().size() + stop_waiting_frames().size() +
         path_challenge_frames().size() + path_response_frames().size() +
         stream_frames().size() + ping_frames().size() +
         connection_close_frames().size() + padding_frames().size();
}

const std::vector<QuicAckFrame>& SimpleQuicFramer::ack_frames() const {
  return visitor_->ack_frames();
}

const std::vector<QuicStopWaitingFrame>& SimpleQuicFramer::stop_waiting_frames()
    const {
  return visitor_->stop_waiting_frames();
}

const std::vector<QuicPathChallengeFrame>&
SimpleQuicFramer::path_challenge_frames() const {
  return visitor_->path_challenge_frames();
}
const std::vector<QuicPathResponseFrame>&
SimpleQuicFramer::path_response_frames() const {
  return visitor_->path_response_frames();
}

const std::vector<QuicPingFrame>& SimpleQuicFramer::ping_frames() const {
  return visitor_->ping_frames();
}

const std::vector<QuicMessageFrame>& SimpleQuicFramer::message_frames() const {
  return visitor_->message_frames();
}

const std::vector<QuicWindowUpdateFrame>&
SimpleQuicFramer::window_update_frames() const {
  return visitor_->window_update_frames();
}

const std::vector<std::unique_ptr<QuicStreamFrame>>&
SimpleQuicFramer::stream_frames() const {
  return visitor_->stream_frames();
}

const std::vector<QuicRstStreamFrame>& SimpleQuicFramer::rst_stream_frames()
    const {
  return visitor_->rst_stream_frames();
}

const std::vector<QuicGoAwayFrame>& SimpleQuicFramer::goaway_frames() const {
  return visitor_->goaway_frames();
}

const std::vector<QuicConnectionCloseFrame>&
SimpleQuicFramer::connection_close_frames() const {
  return visitor_->connection_close_frames();
}

const std::vector<QuicPaddingFrame>& SimpleQuicFramer::padding_frames() const {
  return visitor_->padding_frames();
}

}  // namespace test
}  // namespace quic
