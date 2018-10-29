// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/quic_framer.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "net/third_party/quic/core/crypto/null_decrypter.h"
#include "net/third_party/quic/core/crypto/null_encrypter.h"
#include "net/third_party/quic/core/crypto/quic_decrypter.h"
#include "net/third_party/quic/core/crypto/quic_encrypter.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/platform/api/quic_arraysize.h"
#include "net/third_party/quic/platform/api/quic_expect_bug.h"
#include "net/third_party/quic/platform/api/quic_flags.h"
#include "net/third_party/quic/platform/api/quic_logging.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/test_tools/quic_framer_peer.h"
#include "net/third_party/quic/test_tools/quic_test_utils.h"
#include "net/third_party/quic/test_tools/simple_data_producer.h"

using testing::_;
using testing::Return;
using testing::Truly;

namespace quic {
namespace test {
namespace {

const QuicPacketNumber kEpoch = UINT64_C(1) << 32;
const QuicPacketNumber kMask = kEpoch - 1;

const QuicUint128 kTestStatelessResetToken = 1010101;  // 0x0F69B5

// Use fields in which each byte is distinct to ensure that every byte is
// framed correctly. The values are otherwise arbitrary.
const QuicConnectionId kConnectionId = UINT64_C(0xFEDCBA9876543210);
const QuicPacketNumber kPacketNumber = UINT64_C(0x12345678);
const QuicPacketNumber kSmallLargestObserved = UINT16_C(0x1234);
const QuicPacketNumber kSmallMissingPacket = UINT16_C(0x1233);
const QuicPacketNumber kLeastUnacked = UINT64_C(0x012345670);
const QuicStreamId kStreamId = UINT64_C(0x01020304);
// Note that the high 4 bits of the stream offset must be less than 0x40
// in order to ensure that the value can be encoded using VarInt62 encoding.
const QuicStreamOffset kStreamOffset = UINT64_C(0x3A98FEDC32107654);
const QuicPublicResetNonceProof kNonceProof = UINT64_C(0xABCDEF0123456789);

// In testing that we can ack the full range of packets...
// This is the largest packet number that can be represented in IETF QUIC
// varint62 format.
const QuicPacketNumber kLargestIetfLargestObserved =
    UINT64_C(0x3fffffffffffffff);
// Encodings for the two bits in a VarInt62 that
// describe the length of the VarInt61. For binary packet
// formats in this file, the convention is to code the
// first byte as
//   kVarInt62FourBytes + 0x<value_in_that_byte>
const uint8_t kVarInt62OneByte = 0x00;
const uint8_t kVarInt62TwoBytes = 0x40;
const uint8_t kVarInt62FourBytes = 0x80;
const uint8_t kVarInt62EightBytes = 0xc0;

class TestEncrypter : public QuicEncrypter {
 public:
  ~TestEncrypter() override {}
  bool SetKey(QuicStringPiece key) override { return true; }
  bool SetNoncePrefix(QuicStringPiece nonce_prefix) override { return true; }
  bool SetIV(QuicStringPiece iv) override { return true; }
  bool EncryptPacket(QuicTransportVersion version,
                     QuicPacketNumber packet_number,
                     QuicStringPiece associated_data,
                     QuicStringPiece plaintext,
                     char* output,
                     size_t* output_length,
                     size_t max_output_length) override {
    version_ = version;
    packet_number_ = packet_number;
    associated_data_ = QuicString(associated_data);
    plaintext_ = QuicString(plaintext);
    memcpy(output, plaintext.data(), plaintext.length());
    *output_length = plaintext.length();
    return true;
  }
  size_t GetKeySize() const override { return 0; }
  size_t GetNoncePrefixSize() const override { return 0; }
  size_t GetIVSize() const override { return 0; }
  size_t GetMaxPlaintextSize(size_t ciphertext_size) const override {
    return ciphertext_size;
  }
  size_t GetCiphertextSize(size_t plaintext_size) const override {
    return plaintext_size;
  }
  QuicStringPiece GetKey() const override { return QuicStringPiece(); }
  QuicStringPiece GetNoncePrefix() const override { return QuicStringPiece(); }

  QuicTransportVersion version_;
  QuicPacketNumber packet_number_;
  QuicString associated_data_;
  QuicString plaintext_;
};

class TestDecrypter : public QuicDecrypter {
 public:
  ~TestDecrypter() override {}
  bool SetKey(QuicStringPiece key) override { return true; }
  bool SetNoncePrefix(QuicStringPiece nonce_prefix) override { return true; }
  bool SetIV(QuicStringPiece iv) override { return true; }
  bool SetPreliminaryKey(QuicStringPiece key) override {
    QUIC_BUG << "should not be called";
    return false;
  }
  bool SetDiversificationNonce(const DiversificationNonce& key) override {
    return true;
  }
  bool DecryptPacket(QuicTransportVersion version,
                     QuicPacketNumber packet_number,
                     QuicStringPiece associated_data,
                     QuicStringPiece ciphertext,
                     char* output,
                     size_t* output_length,
                     size_t max_output_length) override {
    version_ = version;
    packet_number_ = packet_number;
    associated_data_ = QuicString(associated_data);
    ciphertext_ = QuicString(ciphertext);
    memcpy(output, ciphertext.data(), ciphertext.length());
    *output_length = ciphertext.length();
    return true;
  }
  size_t GetKeySize() const override { return 0; }
  size_t GetIVSize() const override { return 0; }
  QuicStringPiece GetKey() const override { return QuicStringPiece(); }
  QuicStringPiece GetNoncePrefix() const override { return QuicStringPiece(); }
  // Use a distinct value starting with 0xFFFFFF, which is never used by TLS.
  uint32_t cipher_id() const override { return 0xFFFFFFF2; }
  QuicTransportVersion version_;
  QuicPacketNumber packet_number_;
  QuicString associated_data_;
  QuicString ciphertext_;
};

class TestQuicVisitor : public QuicFramerVisitorInterface {
 public:
  TestQuicVisitor()
      : error_count_(0),
        version_mismatch_(0),
        packet_count_(0),
        frame_count_(0),
        complete_packets_(0),
        accept_packet_(true),
        accept_public_header_(true) {}

  ~TestQuicVisitor() override {}

  void OnError(QuicFramer* f) override {
    QUIC_DLOG(INFO) << "QuicFramer Error: " << QuicErrorCodeToString(f->error())
                    << " (" << f->error() << ")";
    ++error_count_;
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

  bool OnProtocolVersionMismatch(ParsedQuicVersion received_version) override {
    QUIC_DLOG(INFO) << "QuicFramer Version Mismatch, version: "
                    << received_version;
    ++version_mismatch_;
    return true;
  }

  bool OnUnauthenticatedPublicHeader(const QuicPacketHeader& header) override {
    header_ = QuicMakeUnique<QuicPacketHeader>((header));
    return accept_public_header_;
  }

  bool OnUnauthenticatedHeader(const QuicPacketHeader& header) override {
    return true;
  }

  void OnDecryptedPacket(EncryptionLevel level) override {}

  bool OnPacketHeader(const QuicPacketHeader& header) override {
    ++packet_count_;
    header_ = QuicMakeUnique<QuicPacketHeader>((header));
    return accept_packet_;
  }

  bool OnStreamFrame(const QuicStreamFrame& frame) override {
    ++frame_count_;
    // Save a copy of the data so it is valid after the packet is processed.
    QuicString* string_data =
        new QuicString(frame.data_buffer, frame.data_length);
    stream_data_.push_back(QuicWrapUnique(string_data));
    stream_frames_.push_back(QuicMakeUnique<QuicStreamFrame>(
        frame.stream_id, frame.fin, frame.offset, *string_data));
    return true;
  }

  bool OnCryptoFrame(const QuicCryptoFrame& frame) override {
    // TODO(nharper): Implement.
    return true;
  }

  bool OnAckFrameStart(QuicPacketNumber largest_acked,
                       QuicTime::Delta ack_delay_time) override {
    ++frame_count_;
    QuicAckFrame ack_frame;
    ack_frame.largest_acked = largest_acked;
    ack_frame.ack_delay_time = ack_delay_time;
    ack_frames_.push_back(QuicMakeUnique<QuicAckFrame>(ack_frame));
    return true;
  }

  bool OnAckRange(QuicPacketNumber start, QuicPacketNumber end) override {
    DCHECK(!ack_frames_.empty());
    ack_frames_[ack_frames_.size() - 1]->packets.AddRange(start, end);
    return true;
  }

  bool OnAckTimestamp(QuicPacketNumber packet_number,
                      QuicTime timestamp) override {
    ack_frames_[ack_frames_.size() - 1]->received_packet_times.push_back(
        std::make_pair(packet_number, timestamp));
    return true;
  }

  bool OnAckFrameEnd(QuicPacketNumber /*start*/) override { return true; }

  bool OnStopWaitingFrame(const QuicStopWaitingFrame& frame) override {
    ++frame_count_;
    stop_waiting_frames_.push_back(QuicMakeUnique<QuicStopWaitingFrame>(frame));
    return true;
  }

  bool OnPaddingFrame(const QuicPaddingFrame& frame) override {
    padding_frames_.push_back(QuicMakeUnique<QuicPaddingFrame>(frame));
    return true;
  }

  bool OnPingFrame(const QuicPingFrame& frame) override {
    ++frame_count_;
    ping_frames_.push_back(QuicMakeUnique<QuicPingFrame>(frame));
    return true;
  }

  bool OnMessageFrame(const QuicMessageFrame& frame) override {
    ++frame_count_;
    message_frames_.push_back(QuicMakeUnique<QuicMessageFrame>(frame));
    return true;
  }

  void OnPacketComplete() override { ++complete_packets_; }

  bool OnRstStreamFrame(const QuicRstStreamFrame& frame) override {
    rst_stream_frame_ = frame;
    return true;
  }

  bool OnConnectionCloseFrame(const QuicConnectionCloseFrame& frame) override {
    connection_close_frame_ = frame;
    return true;
  }

  bool OnApplicationCloseFrame(
      const QuicApplicationCloseFrame& frame) override {
    application_close_frame_ = frame;
    return true;
  }

  bool OnStopSendingFrame(const QuicStopSendingFrame& frame) override {
    stop_sending_frame_ = frame;
    return true;
  }

  bool OnPathChallengeFrame(const QuicPathChallengeFrame& frame) override {
    path_challenge_frame_ = frame;
    return true;
  }

  bool OnPathResponseFrame(const QuicPathResponseFrame& frame) override {
    path_response_frame_ = frame;
    return true;
  }

  bool OnGoAwayFrame(const QuicGoAwayFrame& frame) override {
    goaway_frame_ = frame;
    return true;
  }

  bool OnMaxStreamIdFrame(const QuicMaxStreamIdFrame& frame) override {
    max_stream_id_frame_ = frame;
    return true;
  }

  bool OnStreamIdBlockedFrame(const QuicStreamIdBlockedFrame& frame) override {
    stream_id_blocked_frame_ = frame;
    return true;
  }

  bool OnWindowUpdateFrame(const QuicWindowUpdateFrame& frame) override {
    window_update_frame_ = frame;
    return true;
  }

  bool OnBlockedFrame(const QuicBlockedFrame& frame) override {
    blocked_frame_ = frame;
    return true;
  }

  bool OnNewConnectionIdFrame(const QuicNewConnectionIdFrame& frame) override {
    new_connection_id_ = frame;
    return true;
  }

  bool OnRetireConnectionIdFrame(
      const QuicRetireConnectionIdFrame& frame) override {
    retire_connection_id_ = frame;
    return true;
  }

  bool OnNewTokenFrame(const QuicNewTokenFrame& frame) override {
    new_token_ = frame;
    return true;
  }

  bool IsValidStatelessResetToken(QuicUint128 token) const override {
    return token == kTestStatelessResetToken;
  }

  void OnAuthenticatedIetfStatelessResetPacket(
      const QuicIetfStatelessResetPacket& packet) override {
    stateless_reset_packet_ =
        QuicMakeUnique<QuicIetfStatelessResetPacket>(packet);
  }

  // Counters from the visitor_ callbacks.
  int error_count_;
  int version_mismatch_;
  int packet_count_;
  int frame_count_;
  int complete_packets_;
  bool accept_packet_;
  bool accept_public_header_;

  std::unique_ptr<QuicPacketHeader> header_;
  std::unique_ptr<QuicPublicResetPacket> public_reset_packet_;
  std::unique_ptr<QuicIetfStatelessResetPacket> stateless_reset_packet_;
  std::unique_ptr<QuicVersionNegotiationPacket> version_negotiation_packet_;
  std::vector<std::unique_ptr<QuicStreamFrame>> stream_frames_;
  std::vector<std::unique_ptr<QuicAckFrame>> ack_frames_;
  std::vector<std::unique_ptr<QuicStopWaitingFrame>> stop_waiting_frames_;
  std::vector<std::unique_ptr<QuicPaddingFrame>> padding_frames_;
  std::vector<std::unique_ptr<QuicPingFrame>> ping_frames_;
  std::vector<std::unique_ptr<QuicMessageFrame>> message_frames_;
  QuicRstStreamFrame rst_stream_frame_;
  QuicConnectionCloseFrame connection_close_frame_;
  QuicApplicationCloseFrame application_close_frame_;
  QuicStopSendingFrame stop_sending_frame_;
  QuicGoAwayFrame goaway_frame_;
  QuicPathChallengeFrame path_challenge_frame_;
  QuicPathResponseFrame path_response_frame_;
  QuicWindowUpdateFrame window_update_frame_;
  QuicBlockedFrame blocked_frame_;
  QuicStreamIdBlockedFrame stream_id_blocked_frame_;
  QuicMaxStreamIdFrame max_stream_id_frame_;
  QuicNewConnectionIdFrame new_connection_id_;
  QuicRetireConnectionIdFrame retire_connection_id_;
  QuicNewTokenFrame new_token_;
  std::vector<std::unique_ptr<QuicString>> stream_data_;
};

// Simple struct for defining a packet's content, and associated
// parse error.
struct PacketFragment {
  QuicString error_if_missing;
  std::vector<unsigned char> fragment;
};

using PacketFragments = std::vector<struct PacketFragment>;

ParsedQuicVersionVector AllSupportedVersionsIncludingTls() {
  QuicFlagSaver flags;
  SetQuicFlag(&FLAGS_quic_supports_tls_handshake, true);
  return AllSupportedVersions();
}

class QuicFramerTest : public QuicTestWithParam<ParsedQuicVersion> {
 public:
  QuicFramerTest()
      : encrypter_(new test::TestEncrypter()),
        decrypter_(new test::TestDecrypter()),
        version_(GetParam()),
        start_(QuicTime::Zero() + QuicTime::Delta::FromMicroseconds(0x10)),
        framer_(AllSupportedVersionsIncludingTls(),
                start_,
                Perspective::IS_SERVER) {
    SetQuicFlag(&FLAGS_quic_supports_tls_handshake, true);
    framer_.set_version(version_);
    framer_.SetDecrypter(ENCRYPTION_NONE,
                         std::unique_ptr<QuicDecrypter>(decrypter_));
    framer_.SetEncrypter(ENCRYPTION_NONE,
                         std::unique_ptr<QuicEncrypter>(encrypter_));

    framer_.set_visitor(&visitor_);
  }

  // Helper function to get unsigned char representation of the handshake
  // protocol byte of the current QUIC version number.
  unsigned char GetQuicVersionProtocolByte() {
    return (CreateQuicVersionLabel(version_) >> 24) & 0xff;
  }

  // Helper function to get unsigned char representation of digit in the
  // units place of the current QUIC version number.
  unsigned char GetQuicVersionDigitOnes() {
    return CreateQuicVersionLabel(version_) & 0xff;
  }

  // Helper function to get unsigned char representation of digit in the
  // tens place of the current QUIC version number.
  unsigned char GetQuicVersionDigitTens() {
    return (CreateQuicVersionLabel(version_) >> 8) & 0xff;
  }

  bool CheckEncryption(QuicPacketNumber packet_number, QuicPacket* packet) {
    EXPECT_EQ(version_.transport_version, encrypter_->version_);
    if (packet_number != encrypter_->packet_number_) {
      QUIC_LOG(ERROR) << "Encrypted incorrect packet number.  expected "
                      << packet_number
                      << " actual: " << encrypter_->packet_number_;
      return false;
    }
    if (packet->AssociatedData(framer_.transport_version()) !=
        encrypter_->associated_data_) {
      QUIC_LOG(ERROR) << "Encrypted incorrect associated data.  expected "
                      << packet->AssociatedData(framer_.transport_version())
                      << " actual: " << encrypter_->associated_data_;
      return false;
    }
    if (packet->Plaintext(framer_.transport_version()) !=
        encrypter_->plaintext_) {
      QUIC_LOG(ERROR) << "Encrypted incorrect plaintext data.  expected "
                      << packet->Plaintext(framer_.transport_version())
                      << " actual: " << encrypter_->plaintext_;
      return false;
    }
    return true;
  }

  bool CheckDecryption(const QuicEncryptedPacket& encrypted,
                       bool includes_version,
                       bool includes_diversification_nonce,
                       QuicConnectionIdLength destination_connection_id_length,
                       QuicConnectionIdLength source_connection_id_length) {
    EXPECT_EQ(version_.transport_version, decrypter_->version_);
    if (visitor_.header_->packet_number != decrypter_->packet_number_) {
      QUIC_LOG(ERROR) << "Decrypted incorrect packet number.  expected "
                      << visitor_.header_->packet_number
                      << " actual: " << decrypter_->packet_number_;
      return false;
    }
    if (QuicFramer::GetAssociatedDataFromEncryptedPacket(
            framer_.transport_version(), encrypted,
            destination_connection_id_length, source_connection_id_length,
            includes_version, includes_diversification_nonce,
            PACKET_4BYTE_PACKET_NUMBER) != decrypter_->associated_data_) {
      QUIC_LOG(ERROR) << "Decrypted incorrect associated data.  expected "
                      << QuicFramer::GetAssociatedDataFromEncryptedPacket(
                             framer_.transport_version(), encrypted,
                             destination_connection_id_length,
                             source_connection_id_length, includes_version,
                             includes_diversification_nonce,
                             PACKET_4BYTE_PACKET_NUMBER)
                      << " actual: " << decrypter_->associated_data_;
      return false;
    }
    QuicStringPiece ciphertext(
        encrypted.AsStringPiece().substr(GetStartOfEncryptedData(
            framer_.transport_version(), destination_connection_id_length,
            source_connection_id_length, includes_version,
            includes_diversification_nonce, PACKET_4BYTE_PACKET_NUMBER)));
    if (ciphertext != decrypter_->ciphertext_) {
      QUIC_LOG(ERROR) << "Decrypted incorrect ciphertext data.  expected "
                      << ciphertext << " actual: " << decrypter_->ciphertext_;
      return false;
    }
    return true;
  }

  char* AsChars(unsigned char* data) { return reinterpret_cast<char*>(data); }

  // Creates a new QuicEncryptedPacket by concatenating the various
  // packet fragments in |fragments|.
  std::unique_ptr<QuicEncryptedPacket> AssemblePacketFromFragments(
      const PacketFragments& fragments) {
    char* buffer = new char[kMaxPacketSize + 1];
    size_t len = 0;
    for (const auto& fragment : fragments) {
      memcpy(buffer + len, fragment.fragment.data(), fragment.fragment.size());
      len += fragment.fragment.size();
    }
    return QuicMakeUnique<QuicEncryptedPacket>(buffer, len, true);
  }

  void CheckFramingBoundaries(const PacketFragments& fragments,
                              QuicErrorCode error_code) {
    std::unique_ptr<QuicEncryptedPacket> packet(
        AssemblePacketFromFragments(fragments));
    // Check all the various prefixes of |packet| for the expected
    // parse error and error code.
    for (size_t i = 0; i < packet->length(); ++i) {
      QuicString expected_error;
      size_t len = 0;
      for (const auto& fragment : fragments) {
        len += fragment.fragment.size();
        if (i < len) {
          expected_error = fragment.error_if_missing;
          break;
        }
      }

      if (expected_error.empty())
        continue;

      CheckProcessingFails(*packet, i, expected_error, error_code);
    }
  }

  void CheckProcessingFails(const QuicEncryptedPacket& packet,
                            size_t len,
                            QuicString expected_error,
                            QuicErrorCode error_code) {
    QuicEncryptedPacket encrypted(packet.data(), len, false);
    EXPECT_FALSE(framer_.ProcessPacket(encrypted)) << "len: " << len;
    EXPECT_EQ(expected_error, framer_.detailed_error()) << "len: " << len;
    EXPECT_EQ(error_code, framer_.error()) << "len: " << len;
  }

  void CheckProcessingFails(unsigned char* packet,
                            size_t len,
                            QuicString expected_error,
                            QuicErrorCode error_code) {
    QuicEncryptedPacket encrypted(AsChars(packet), len, false);
    EXPECT_FALSE(framer_.ProcessPacket(encrypted)) << "len: " << len;
    EXPECT_EQ(expected_error, framer_.detailed_error()) << "len: " << len;
    EXPECT_EQ(error_code, framer_.error()) << "len: " << len;
  }

  // Checks if the supplied string matches data in the supplied StreamFrame.
  void CheckStreamFrameData(QuicString str, QuicStreamFrame* frame) {
    EXPECT_EQ(str, QuicString(frame->data_buffer, frame->data_length));
  }

  void CheckCalculatePacketNumber(QuicPacketNumber expected_packet_number,
                                  QuicPacketNumber last_packet_number) {
    QuicPacketNumber wire_packet_number = expected_packet_number & kMask;
    EXPECT_EQ(expected_packet_number,
              QuicFramerPeer::CalculatePacketNumberFromWire(
                  &framer_, PACKET_4BYTE_PACKET_NUMBER, last_packet_number,
                  wire_packet_number))
        << "last_packet_number: " << last_packet_number
        << " wire_packet_number: " << wire_packet_number;
  }

  std::unique_ptr<QuicPacket> BuildDataPacket(const QuicPacketHeader& header,
                                              const QuicFrames& frames) {
    return BuildUnsizedDataPacket(&framer_, header, frames);
  }

  std::unique_ptr<QuicPacket> BuildDataPacket(const QuicPacketHeader& header,
                                              const QuicFrames& frames,
                                              size_t packet_size) {
    return BuildUnsizedDataPacket(&framer_, header, frames, packet_size);
  }

  test::TestEncrypter* encrypter_;
  test::TestDecrypter* decrypter_;
  ParsedQuicVersion version_;
  QuicTime start_;
  QuicFramer framer_;
  test::TestQuicVisitor visitor_;
};

// Multiple test cases of QuicFramerTest use byte arrays to define packets for
// testing, and these byte arrays contain the QUIC version. This macro explodes
// the 32-bit version into four bytes in network order. Since it uses methods of
// QuicFramerTest, it is only valid to use this in a QuicFramerTest.
#define QUIC_VERSION_BYTES                                      \
  GetQuicVersionProtocolByte(), '0', GetQuicVersionDigitTens(), \
      GetQuicVersionDigitOnes()

// Run all framer tests with all supported versions of QUIC.
INSTANTIATE_TEST_CASE_P(
    QuicFramerTests,
    QuicFramerTest,
    ::testing::ValuesIn(AllSupportedVersionsIncludingTls()));

TEST_P(QuicFramerTest, CalculatePacketNumberFromWireNearEpochStart) {
  // A few quick manual sanity checks.
  CheckCalculatePacketNumber(UINT64_C(1), UINT64_C(0));
  CheckCalculatePacketNumber(kEpoch + 1, kMask);
  CheckCalculatePacketNumber(kEpoch, kMask);

  // Cases where the last number was close to the start of the range.
  for (uint64_t last = 0; last < 10; last++) {
    // Small numbers should not wrap (even if they're out of order).
    for (uint64_t j = 0; j < 10; j++) {
      CheckCalculatePacketNumber(j, last);
    }

    // Large numbers should not wrap either (because we're near 0 already).
    for (uint64_t j = 0; j < 10; j++) {
      CheckCalculatePacketNumber(kEpoch - 1 - j, last);
    }
  }
}

TEST_P(QuicFramerTest, CalculatePacketNumberFromWireNearEpochEnd) {
  // Cases where the last number was close to the end of the range
  for (uint64_t i = 0; i < 10; i++) {
    QuicPacketNumber last = kEpoch - i;

    // Small numbers should wrap.
    for (uint64_t j = 0; j < 10; j++) {
      CheckCalculatePacketNumber(kEpoch + j, last);
    }

    // Large numbers should not (even if they're out of order).
    for (uint64_t j = 0; j < 10; j++) {
      CheckCalculatePacketNumber(kEpoch - 1 - j, last);
    }
  }
}

// Next check where we're in a non-zero epoch to verify we handle
// reverse wrapping, too.
TEST_P(QuicFramerTest, CalculatePacketNumberFromWireNearPrevEpoch) {
  const uint64_t prev_epoch = 1 * kEpoch;
  const uint64_t cur_epoch = 2 * kEpoch;
  // Cases where the last number was close to the start of the range
  for (uint64_t i = 0; i < 10; i++) {
    uint64_t last = cur_epoch + i;
    // Small number should not wrap (even if they're out of order).
    for (uint64_t j = 0; j < 10; j++) {
      CheckCalculatePacketNumber(cur_epoch + j, last);
    }

    // But large numbers should reverse wrap.
    for (uint64_t j = 0; j < 10; j++) {
      uint64_t num = kEpoch - 1 - j;
      CheckCalculatePacketNumber(prev_epoch + num, last);
    }
  }
}

TEST_P(QuicFramerTest, CalculatePacketNumberFromWireNearNextEpoch) {
  const uint64_t cur_epoch = 2 * kEpoch;
  const uint64_t next_epoch = 3 * kEpoch;
  // Cases where the last number was close to the end of the range
  for (uint64_t i = 0; i < 10; i++) {
    QuicPacketNumber last = next_epoch - 1 - i;

    // Small numbers should wrap.
    for (uint64_t j = 0; j < 10; j++) {
      CheckCalculatePacketNumber(next_epoch + j, last);
    }

    // but large numbers should not (even if they're out of order).
    for (uint64_t j = 0; j < 10; j++) {
      uint64_t num = kEpoch - 1 - j;
      CheckCalculatePacketNumber(cur_epoch + num, last);
    }
  }
}

TEST_P(QuicFramerTest, CalculatePacketNumberFromWireNearNextMax) {
  const uint64_t max_number = std::numeric_limits<uint64_t>::max();
  const uint64_t max_epoch = max_number & ~kMask;

  // Cases where the last number was close to the end of the range
  for (uint64_t i = 0; i < 10; i++) {
    // Subtract 1, because the expected next packet number is 1 more than the
    // last packet number.
    QuicPacketNumber last = max_number - i - 1;

    // Small numbers should not wrap, because they have nowhere to go.
    for (uint64_t j = 0; j < 10; j++) {
      CheckCalculatePacketNumber(max_epoch + j, last);
    }

    // Large numbers should not wrap either.
    for (uint64_t j = 0; j < 10; j++) {
      uint64_t num = kEpoch - 1 - j;
      CheckCalculatePacketNumber(max_epoch + num, last);
    }
  }
}

TEST_P(QuicFramerTest, EmptyPacket) {
  char packet[] = {0x00};
  QuicEncryptedPacket encrypted(packet, 0, false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_INVALID_PACKET_HEADER, framer_.error());
}

TEST_P(QuicFramerTest, LargePacket) {
  // clang-format off
  unsigned char packet[kMaxPacketSize + 1] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78, 0x56, 0x34, 0x12,
    // private flags
    0x00,
  };
  unsigned char packet44[kMaxPacketSize + 1] = {
    // type (short header 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78, 0x56, 0x34, 0x12,
  };
  // clang-format on
  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  }

  const size_t header_size = GetPacketHeaderSize(
      framer_.transport_version(), PACKET_8BYTE_CONNECTION_ID,
      PACKET_0BYTE_CONNECTION_ID, !kIncludeVersion,
      !kIncludeDiversificationNonce, PACKET_4BYTE_PACKET_NUMBER);

  memset(p + header_size, 0, kMaxPacketSize - header_size);

  QuicEncryptedPacket encrypted(AsChars(p), p_size, false);
  EXPECT_QUIC_BUG(framer_.ProcessPacket(encrypted), "Packet too large:1");

  ASSERT_TRUE(visitor_.header_.get());
  // Make sure we've parsed the packet header, so we can send an error.
  EXPECT_EQ(kConnectionId, visitor_.header_->destination_connection_id);
  // Make sure the correct error is propagated.
  EXPECT_EQ(QUIC_PACKET_TOO_LARGE, framer_.error());
}

TEST_P(QuicFramerTest, PacketHeader) {
  // clang-format off
  PacketFragments packet38 = {
      // public flags (8 byte connection_id)
      {"Unable to read public flags.",
       {0x28}},
      // connection_id
      {"Unable to read ConnectionId.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"Unable to read packet number.",
       {0x78, 0x56, 0x34, 0x12}},
  };

  PacketFragments packet39 = {
      // public flags (8 byte connection_id)
      {"Unable to read public flags.",
       {0x28}},
      // connection_id
      {"Unable to read ConnectionId.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"Unable to read packet number.",
       {0x12, 0x34, 0x56, 0x78}},
  };
  // clang-format on

  if (framer_.transport_version() > QUIC_VERSION_43) {
    return;
  }

  PacketFragments& fragments =
      framer_.transport_version() == QUIC_VERSION_35 ? packet38 : packet39;

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));

  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(QUIC_MISSING_PAYLOAD, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(kConnectionId, visitor_.header_->destination_connection_id);
  EXPECT_FALSE(visitor_.header_->reset_flag);
  EXPECT_FALSE(visitor_.header_->version_flag);
  EXPECT_EQ(kPacketNumber, visitor_.header_->packet_number);

  CheckFramingBoundaries(fragments, QUIC_INVALID_PACKET_HEADER);
}

TEST_P(QuicFramerTest, LongPacketHeader) {
  // clang-format off
  PacketFragments packet = {
    // type (long header with packet type INITIAL)
    {"Unable to read public flags.",
     {0xFF}},
    // version tag
    {"Unable to read protocol version.",
     {QUIC_VERSION_BYTES}},
    // connection_id length
    {"Unable to read ConnectionId length.",
     {0x50}},
    // connection_id
    {"Unable to read Destination ConnectionId.",
     {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
    // packet number
    {"Unable to read packet number.",
     {0x12, 0x34, 0x56, 0x78}},
  };
  // clang-format on

  if (framer_.transport_version() <= QUIC_VERSION_43) {
    return;
  }

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));

  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(QUIC_MISSING_PAYLOAD, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(kConnectionId, visitor_.header_->destination_connection_id);
  EXPECT_FALSE(visitor_.header_->reset_flag);
  EXPECT_TRUE(visitor_.header_->version_flag);
  EXPECT_EQ(kPacketNumber, visitor_.header_->packet_number);

  CheckFramingBoundaries(packet, QUIC_INVALID_PACKET_HEADER);
}

TEST_P(QuicFramerTest, PacketHeaderWith0ByteConnectionId) {
  QuicFramerPeer::SetLastSerializedConnectionId(&framer_, kConnectionId);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);

  // clang-format off
  PacketFragments packet = {
      // public flags (0 byte connection_id)
      {"Unable to read public flags.",
       {0x20}},
      // connection_id
      // packet number
      {"Unable to read packet number.",
       {0x78, 0x56, 0x34, 0x12}}
  };

  PacketFragments packet39 = {
      // public flags (0 byte connection_id)
      {"Unable to read public flags.",
       {0x20}},
      // connection_id
      // packet number
      {"Unable to read packet number.",
       {0x12, 0x34, 0x56, 0x78}},
  };

  PacketFragments packet44 = {
        // type (short header, 4 byte packet number)
        {"Unable to read type.",
         {0x32}},
        // connection_id
        // packet number
        {"Unable to read packet number.",
         {0x12, 0x34, 0x56, 0x78}},
    };
  // clang-format on

  PacketFragments& fragments =
      framer_.transport_version() > QUIC_VERSION_43
          ? packet44
          : (framer_.transport_version() == QUIC_VERSION_35 ? packet
                                                            : packet39);
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(QUIC_MISSING_PAYLOAD, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(kConnectionId, visitor_.header_->destination_connection_id);
  EXPECT_FALSE(visitor_.header_->reset_flag);
  EXPECT_FALSE(visitor_.header_->version_flag);
  EXPECT_EQ(kPacketNumber, visitor_.header_->packet_number);

  CheckFramingBoundaries(fragments, QUIC_INVALID_PACKET_HEADER);
}

TEST_P(QuicFramerTest, PacketHeaderWithVersionFlag) {
  // clang-format off
  PacketFragments packet = {
      // public flags (0 byte connection_id)
      {"Unable to read public flags.",
       {0x29}},
      // connection_id
      {"Unable to read ConnectionId.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // version tag
      {"Unable to read protocol version.",
       {QUIC_VERSION_BYTES}},
      // packet number
      {"Unable to read packet number.",
       {0x78, 0x56, 0x34, 0x12}},
  };

  PacketFragments packet39 = {
      // public flags (0 byte connection_id)
      {"Unable to read public flags.",
       {0x29}},
      // connection_id
      {"Unable to read ConnectionId.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // version tag
      {"Unable to read protocol version.",
       {QUIC_VERSION_BYTES}},
      // packet number
      {"Unable to read packet number.",
       {0x12, 0x34, 0x56, 0x78}},
  };

  PacketFragments packet44 = {
      // type (long header with packet type ZERO_RTT_PROTECTED)
      {"Unable to read public flags.",
       {0xFC}},
      // version tag
      {"Unable to read protocol version.",
       {QUIC_VERSION_BYTES}},
      // connection_id length
      {"Unable to read ConnectionId length.",
       {0x50}},
      // connection_id
      {"Unable to read Destination ConnectionId.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"Unable to read packet number.",
       {0x12, 0x34, 0x56, 0x78}},
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.transport_version() > QUIC_VERSION_43
          ? packet44
          : (framer_.transport_version() == QUIC_VERSION_35 ? packet
                                                            : packet39);
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(QUIC_MISSING_PAYLOAD, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(kConnectionId, visitor_.header_->destination_connection_id);
  EXPECT_FALSE(visitor_.header_->reset_flag);
  EXPECT_TRUE(visitor_.header_->version_flag);
  EXPECT_EQ(GetParam(), visitor_.header_->version);
  EXPECT_EQ(kPacketNumber, visitor_.header_->packet_number);

  CheckFramingBoundaries(fragments, QUIC_INVALID_PACKET_HEADER);
}

TEST_P(QuicFramerTest, PacketHeaderWith4BytePacketNumber) {
  QuicFramerPeer::SetLargestPacketNumber(&framer_, kPacketNumber - 2);

  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id and 4 byte packet number)
      {"Unable to read public flags.",
       {0x28}},
      // connection_id
      {"Unable to read ConnectionId.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"Unable to read packet number.",
       {0x78, 0x56, 0x34, 0x12}},
  };

  PacketFragments packet39 = {
      // public flags (8 byte connection_id and 4 byte packet number)
      {"Unable to read public flags.",
       {0x28}},
      // connection_id
      {"Unable to read ConnectionId.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"Unable to read packet number.",
       {0x12, 0x34, 0x56, 0x78}},
  };

  PacketFragments packet44 = {
      // type (short header, 4 byte packet number)
      {"Unable to read public flags.",
       {0x32}},
      // connection_id
      {"Unable to read Destination ConnectionId.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"Unable to read packet number.",
       {0x12, 0x34, 0x56, 0x78}},
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.transport_version() > QUIC_VERSION_43
          ? packet44
          : (framer_.transport_version() == QUIC_VERSION_35 ? packet
                                                            : packet39);
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(QUIC_MISSING_PAYLOAD, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(kConnectionId, visitor_.header_->destination_connection_id);
  EXPECT_FALSE(visitor_.header_->reset_flag);
  EXPECT_FALSE(visitor_.header_->version_flag);
  EXPECT_EQ(kPacketNumber, visitor_.header_->packet_number);

  CheckFramingBoundaries(fragments, QUIC_INVALID_PACKET_HEADER);
}

TEST_P(QuicFramerTest, PacketHeaderWith2BytePacketNumber) {
  QuicFramerPeer::SetLargestPacketNumber(&framer_, kPacketNumber - 2);

  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id and 2 byte packet number)
      {"Unable to read public flags.",
       {0x18}},
      // connection_id
      {"Unable to read ConnectionId.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"Unable to read packet number.",
       {0x78, 0x56}},
  };

  PacketFragments packet39 = {
      // public flags (8 byte connection_id and 2 byte packet number)
      {"Unable to read public flags.",
       {0x18}},
      // connection_id
      {"Unable to read ConnectionId.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"Unable to read packet number.",
       {0x56, 0x78}},
  };

  PacketFragments packet44 = {
      // type (short header, 2 byte packet number)
      {"Unable to read public flags.",
       {0x31}},
      // connection_id
      {"Unable to read Destination ConnectionId.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"Unable to read packet number.",
       {0x56, 0x78}},
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.transport_version() > QUIC_VERSION_43
          ? packet44
          : (framer_.transport_version() == QUIC_VERSION_35 ? packet
                                                            : packet39);
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(QUIC_MISSING_PAYLOAD, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(kConnectionId, visitor_.header_->destination_connection_id);
  EXPECT_FALSE(visitor_.header_->reset_flag);
  EXPECT_FALSE(visitor_.header_->version_flag);
  EXPECT_EQ(PACKET_2BYTE_PACKET_NUMBER, visitor_.header_->packet_number_length);
  EXPECT_EQ(kPacketNumber, visitor_.header_->packet_number);

  CheckFramingBoundaries(fragments, QUIC_INVALID_PACKET_HEADER);
}

TEST_P(QuicFramerTest, PacketHeaderWith1BytePacketNumber) {
  QuicFramerPeer::SetLargestPacketNumber(&framer_, kPacketNumber - 2);

  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id and 1 byte packet number)
      {"Unable to read public flags.",
       {0x08}},
      // connection_id
      {"Unable to read ConnectionId.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"Unable to read packet number.",
       {0x78}},
  };

  PacketFragments packet44 = {
      // type (8 byte connection_id and 1 byte packet number)
      {"Unable to read public flags.",
       {0x30}},
      // connection_id
      {"Unable to read Destination ConnectionId.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"Unable to read packet number.",
       {0x78}},
  };

  // clang-format on

  PacketFragments& fragments =
      framer_.transport_version() > QUIC_VERSION_43 ? packet44 : packet;
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(QUIC_MISSING_PAYLOAD, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(kConnectionId, visitor_.header_->destination_connection_id);
  EXPECT_FALSE(visitor_.header_->reset_flag);
  EXPECT_FALSE(visitor_.header_->version_flag);
  EXPECT_EQ(PACKET_1BYTE_PACKET_NUMBER, visitor_.header_->packet_number_length);
  EXPECT_EQ(kPacketNumber, visitor_.header_->packet_number);

  CheckFramingBoundaries(fragments, QUIC_INVALID_PACKET_HEADER);
}

TEST_P(QuicFramerTest, PacketNumberDecreasesThenIncreases) {
  // Test the case when a packet is received from the past and future packet
  // numbers are still calculated relative to the largest received packet.
  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber - 2;

  QuicFrames frames = {QuicFrame(QuicPaddingFrame())};
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  QuicEncryptedPacket encrypted(data->data(), data->length(), false);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(kConnectionId, visitor_.header_->destination_connection_id);
  EXPECT_EQ(PACKET_4BYTE_PACKET_NUMBER, visitor_.header_->packet_number_length);
  EXPECT_EQ(kPacketNumber - 2, visitor_.header_->packet_number);

  // Receive a 1 byte packet number.
  header.packet_number = kPacketNumber;
  header.packet_number_length = PACKET_1BYTE_PACKET_NUMBER;
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  data = BuildDataPacket(header, frames);
  QuicEncryptedPacket encrypted1(data->data(), data->length(), false);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted1));
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(kConnectionId, visitor_.header_->destination_connection_id);
  EXPECT_EQ(PACKET_1BYTE_PACKET_NUMBER, visitor_.header_->packet_number_length);
  EXPECT_EQ(kPacketNumber, visitor_.header_->packet_number);

  // Process a 2 byte packet number 256 packets ago.
  header.packet_number = kPacketNumber - 256;
  header.packet_number_length = PACKET_2BYTE_PACKET_NUMBER;
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  data = BuildDataPacket(header, frames);
  QuicEncryptedPacket encrypted2(data->data(), data->length(), false);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted2));
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(kConnectionId, visitor_.header_->destination_connection_id);
  EXPECT_EQ(PACKET_2BYTE_PACKET_NUMBER, visitor_.header_->packet_number_length);
  EXPECT_EQ(kPacketNumber - 256, visitor_.header_->packet_number);

  // Process another 1 byte packet number and ensure it works.
  header.packet_number = kPacketNumber - 1;
  header.packet_number_length = PACKET_1BYTE_PACKET_NUMBER;
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  data = BuildDataPacket(header, frames);
  QuicEncryptedPacket encrypted3(data->data(), data->length(), false);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted3));
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(kConnectionId, visitor_.header_->destination_connection_id);
  EXPECT_EQ(PACKET_1BYTE_PACKET_NUMBER, visitor_.header_->packet_number_length);
  EXPECT_EQ(kPacketNumber - 1, visitor_.header_->packet_number);
}

TEST_P(QuicFramerTest, PacketWithDiversificationNonce) {
  // clang-format off
  unsigned char packet[] = {
    // public flags: includes nonce flag
    0x2C,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // nonce
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    // packet number
    0x78, 0x56, 0x34, 0x12,

    // frame type (padding)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet39[] = {
    // public flags: includes nonce flag
    0x2C,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // nonce
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (padding)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet44[] = {
    // type: Long header with packet type ZERO_RTT_PROTECTED
    0xFC,
    // version tag
    QUIC_VERSION_BYTES,
    // connection_id length
    0x05,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,
    // nonce
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,

    // frame type (padding)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };
  // clang-format on

  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  } else if (framer_.transport_version() != QUIC_VERSION_35) {
    p = packet39;
  }

  QuicEncryptedPacket encrypted(AsChars(p), p_size, false);
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  ASSERT_TRUE(visitor_.header_->nonce != nullptr);
  for (char i = 0; i < 32; ++i) {
    EXPECT_EQ(i, (*visitor_.header_->nonce)[static_cast<size_t>(i)]);
  }
  EXPECT_EQ(1u, visitor_.padding_frames_.size());
  EXPECT_EQ(5, visitor_.padding_frames_[0]->num_padding_bytes);
};

TEST_P(QuicFramerTest, LargePublicFlagWithMismatchedVersions) {
  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id, version flag and an unknown flag)
    0x29,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // version tag
    'Q', '0', '0', '0',
    // packet number
    0x78, 0x56, 0x34, 0x12,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet39[] = {
    // public flags (8 byte connection_id, version flag and an unknown flag)
    0x29,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // version tag
    'Q', '0', '0', '0',
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet44[] = {
    // type (long header with packet type ZERO_RTT_PROTECTED)
    0xFC,
    // version tag
    'Q', '0', '0', '0',
    // connection_id length
    0x50,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };
  // clang-format on

  QuicEncryptedPacket encrypted(
      AsChars(framer_.transport_version() > QUIC_VERSION_43
                  ? packet44
                  : (framer_.transport_version() == QUIC_VERSION_35
                         ? packet
                         : packet39)),
      framer_.transport_version() > QUIC_VERSION_43 ? QUIC_ARRAYSIZE(packet44)
                                                    : QUIC_ARRAYSIZE(packet),
      false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(0, visitor_.frame_count_);
  EXPECT_EQ(1, visitor_.version_mismatch_);
  EXPECT_EQ(1u, visitor_.padding_frames_.size());
  EXPECT_EQ(5, visitor_.padding_frames_[0]->num_padding_bytes);
};

TEST_P(QuicFramerTest, PaddingFrame) {
  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78, 0x56, 0x34, 0x12,

    // frame type (padding frame)
    0x00,
    // Ignored data (which in this case is a stream frame)
    // frame type (stream frame with fin)
    0xFF,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0x3A,
    // data length
    0x0c, 0x00,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };
  // clang-format on

  if (framer_.transport_version() != QUIC_VERSION_35) {
    return;
  }

  QuicEncryptedPacket encrypted(AsChars(packet), QUIC_ARRAYSIZE(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  ASSERT_EQ(0u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  EXPECT_EQ(1u, visitor_.padding_frames_.size());
  EXPECT_EQ(28, visitor_.padding_frames_[0]->num_padding_bytes);
  // A packet with no frames is not acceptable.
  CheckProcessingFails(
      packet,
      GetPacketHeaderSize(
          framer_.transport_version(), PACKET_8BYTE_CONNECTION_ID,
          PACKET_0BYTE_CONNECTION_ID, !kIncludeVersion,
          !kIncludeDiversificationNonce, PACKET_4BYTE_PACKET_NUMBER),
      "Packet has no frames.", QUIC_MISSING_PAYLOAD);
}

TEST_P(QuicFramerTest, NewPaddingFrame) {
  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78, 0x56, 0x34, 0x12,

    // paddings
    0x00, 0x00,
    // frame type (stream frame with fin)
    0xFF,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0x3A,
    // data length
    0x0c, 0x00,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
    // paddings
    0x00, 0x00,
  };

  unsigned char packet39[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // paddings
    0x00, 0x00,
    // frame type (stream frame with fin)
    0xFF,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // offset
    0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data length
    0x00, 0x0c,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
    // paddings
    0x00, 0x00,
  };

  unsigned char packet44[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // paddings
    0x00, 0x00,
    // frame type (stream frame with fin)
    0xFF,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // offset
    0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data length
    0x00, 0x0c,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
    // paddings
    0x00, 0x00,
  };

  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // paddings
    0x00, 0x00,
    // frame type - Stream with FIN, LEN, and OFFSET bits set.
    0x10 | 0x01 | 0x02 | 0x04,

    // stream id
    kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04,
    // offset
    kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data length
    kVarInt62OneByte + 0x0c,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
    // paddings
    0x00, 0x00,
  };
  // clang-format on

  if (framer_.transport_version() == QUIC_VERSION_35) {
    return;
  }
  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    p_size = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  } else if (framer_.transport_version() != QUIC_VERSION_35) {
    p = packet39;
  }

  QuicEncryptedPacket encrypted(AsChars(p), p_size, false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  EXPECT_EQ(2u, visitor_.padding_frames_.size());
  EXPECT_EQ(2, visitor_.padding_frames_[0]->num_padding_bytes);
  EXPECT_EQ(2, visitor_.padding_frames_[1]->num_padding_bytes);
  EXPECT_EQ(kStreamId, visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(kStreamOffset, visitor_.stream_frames_[0]->offset);
  CheckStreamFrameData("hello world!", visitor_.stream_frames_[0].get());
}

TEST_P(QuicFramerTest, StreamFrame) {
  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id)
      {"",
       {0x28}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x78, 0x56, 0x34, 0x12}},
      // frame type (stream frame with fin)
      {"",
       {0xFF}},
      // stream id
      {"Unable to read stream_id.",
       {0x04, 0x03, 0x02, 0x01}},
      // offset
      {"Unable to read offset.",
       {0x54, 0x76, 0x10, 0x32,
        0xDC, 0xFE, 0x98, 0x3A}},
      {"Unable to read frame data.",
       {
         // data length
         0x0c, 0x00,
         // data
         'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };

  PacketFragments packet39 = {
      // public flags (8 byte connection_id)
      {"",
       {0x28}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (stream frame with fin)
      {"",
       {0xFF}},
      // stream id
      {"Unable to read stream_id.",
       {0x01, 0x02, 0x03, 0x04}},
      // offset
      {"Unable to read offset.",
       {0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      {"Unable to read frame data.",
       {
         // data length
         0x00, 0x0c,
         // data
         'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };

  PacketFragments packet44 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (stream frame with fin)
      {"",
       {0xFF}},
      // stream id
      {"Unable to read stream_id.",
       {0x01, 0x02, 0x03, 0x04}},
      // offset
      {"Unable to read offset.",
       {0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      {"Unable to read frame data.",
       {
         // data length
         0x00, 0x0c,
         // data
         'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };

  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type - Stream with FIN, LEN, and OFFSET bits set.
      {"",
       { 0x10 | 0x01 | 0x02 | 0x04 }},
      // stream id
      {"Unable to read stream_id.",
       {kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04}},
      // offset
      {"Unable to read stream data offset.",
       {kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      // data length
      {"Unable to read stream data length.",
       {kVarInt62OneByte + 0x0c}},
      // data
      {"Unable to read frame data.",
       { 'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.transport_version() == QUIC_VERSION_99
          ? packet99
          : (framer_.transport_version() > QUIC_VERSION_43
                 ? packet44
                 : (framer_.transport_version() != QUIC_VERSION_35 ? packet39
                                                                   : packet));
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  EXPECT_EQ(kStreamId, visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(kStreamOffset, visitor_.stream_frames_[0]->offset);
  CheckStreamFrameData("hello world!", visitor_.stream_frames_[0].get());

  CheckFramingBoundaries(fragments, QUIC_INVALID_STREAM_DATA);
}

// Test an empty (no data) stream frame.
TEST_P(QuicFramerTest, EmptyStreamFrame) {
  // Only the IETF QUIC spec explicitly says that empty
  // stream frames are supported.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }
  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type - Stream with FIN, LEN, and OFFSET bits set.
      {"",
       { 0x10 | 0x01 | 0x02 | 0x04 }},
      // stream id
      {"Unable to read stream_id.",
       {kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04}},
      // offset
      {"Unable to read stream data offset.",
       {kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      // data length
      {"Unable to read stream data length.",
       {kVarInt62OneByte + 0x00}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  EXPECT_EQ(kStreamId, visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(kStreamOffset, visitor_.stream_frames_[0]->offset);
  EXPECT_EQ(visitor_.stream_frames_[0].get()->data_length, 0u);

  CheckFramingBoundaries(packet, QUIC_INVALID_STREAM_DATA);
}

TEST_P(QuicFramerTest, MissingDiversificationNonce) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  framer_.SetDecrypter(ENCRYPTION_NONE,
                       QuicMakeUnique<NullDecrypter>(Perspective::IS_CLIENT));
  decrypter_ = new test::TestDecrypter();
  framer_.SetAlternativeDecrypter(
      ENCRYPTION_INITIAL, std::unique_ptr<QuicDecrypter>(decrypter_), false);

  // clang-format off
  unsigned char packet[] = {
      // public flags (8 byte connection_id)
      0x28,
      // connection_id
      0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE,
      // packet number
      0x78, 0x56, 0x34, 0x12,

      // frame type (stream frame with fin)
      0xFF,
      // stream id
      0x04, 0x03, 0x02, 0x01,
      // offset
      0x54, 0x76, 0x10, 0x32,
      0xDC, 0xFE, 0x98, 0x3A,
      // data length
      0x0c, 0x00,
      // data
      'h',  'e',  'l',  'l',
      'o',  ' ',  'w',  'o',
      'r',  'l',  'd',  '!',
  };

  unsigned char packet39[] = {
        // public flags (8 byte connection_id)
        0x28,
        // connection_id
        0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE,
        // packet number
        0x12, 0x34, 0x56, 0x78,

        // frame type (stream frame with fin)
        0xFF,
        // stream id
        0x01, 0x02, 0x03, 0x04,
        // offset
        0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54,
        // data length
        0x00, 0x0c,
        // data
        'h',  'e',  'l',  'l',
        'o',  ' ',  'w',  'o',
        'r',  'l',  'd',  '!',
    };

  unsigned char packet44[] = {
        // type (long header with packet type ZERO_RTT_PROTECTED)
        0xFC,
        // version tag
        QUIC_VERSION_BYTES,
        // connection_id length
        0x50,
        // connection_id
        0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE,
        // packet number
        0x12, 0x34, 0x56, 0x78,

        // frame type - STREAM with LEN, FIN, and OFFSET bits set.
        0x10 | 0x01 | 0x02 | 0x04,
        // stream id
        0x01, 0x02, 0x03, 0x04,
        // offset
        0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54,
        // data length
        0x00, 0x0c,
        // data
        'h',  'e',  'l',  'l',
        'o',  ' ',  'w',  'o',
        'r',  'l',  'd',  '!',
    };
  // clang-format on

  unsigned char* p = packet;
  if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
  } else if (framer_.transport_version() != QUIC_VERSION_35) {
    p = packet39;
  }
  QuicEncryptedPacket encrypted(AsChars(p),
                                framer_.transport_version() > QUIC_VERSION_43
                                    ? QUIC_ARRAYSIZE(packet44)
                                    : QUIC_ARRAYSIZE(packet),
                                false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  if (framer_.transport_version() > QUIC_VERSION_43) {
    // Cannot read diversification nonce.
    EXPECT_EQ(QUIC_INVALID_PACKET_HEADER, framer_.error());
  } else {
    EXPECT_EQ(QUIC_DECRYPTION_FAILURE, framer_.error());
  }
}

TEST_P(QuicFramerTest, StreamFrame3ByteStreamId) {
  if (framer_.transport_version() > QUIC_VERSION_43) {
    // This test is nonsensical for IETF Quic.
    return;
  }
  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id)
      {"",
       {0x28}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x78, 0x56, 0x34, 0x12}},
      // frame type (stream frame with fin)
      {"",
       {0xFE}},
      // stream id
      {"Unable to read stream_id.",
       {0x04, 0x03, 0x02}},
      // offset
      {"Unable to read offset.",
       {0x54, 0x76, 0x10, 0x32,
        0xDC, 0xFE, 0x98, 0x3A}},
      {"Unable to read frame data.",
       {
         // data length
         0x0c, 0x00,
         // data
         'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };

  PacketFragments packet39 = {
      // public flags (8 byte connection_id)
      {"",
       {0x28}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (stream frame with fin)
      {"",
       {0xFE}},
      // stream id
      {"Unable to read stream_id.",
       {0x02, 0x03, 0x04}},
      // offset
      {"Unable to read offset.",
       {0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      {"Unable to read frame data.",
       {
         // data length
         0x00, 0x0c,
         // data
         'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.transport_version() != QUIC_VERSION_35 ? packet39 : packet;
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  // Stream ID should be the last 3 bytes of kStreamId.
  EXPECT_EQ(0x00FFFFFF & kStreamId, visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(kStreamOffset, visitor_.stream_frames_[0]->offset);
  CheckStreamFrameData("hello world!", visitor_.stream_frames_[0].get());

  CheckFramingBoundaries(fragments, QUIC_INVALID_STREAM_DATA);
}

TEST_P(QuicFramerTest, StreamFrame2ByteStreamId) {
  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id)
      {"",
       {0x28}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x78, 0x56, 0x34, 0x12}},
      // frame type (stream frame with fin)
      {"",
       {0xFD}},
      // stream id
      {"Unable to read stream_id.",
       {0x04, 0x03}},
      // offset
      {"Unable to read offset.",
       {0x54, 0x76, 0x10, 0x32,
        0xDC, 0xFE, 0x98, 0x3A}},
      {"Unable to read frame data.",
       {
         // data length
         0x0c, 0x00,
         // data
         'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };

  PacketFragments packet39 = {
      // public flags (8 byte connection_id)
      {"",
       {0x28}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (stream frame with fin)
      {"",
       {0xFD}},
      // stream id
      {"Unable to read stream_id.",
       {0x03, 0x04}},
      // offset
      {"Unable to read offset.",
       {0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      {"Unable to read frame data.",
       {
         // data length
         0x00, 0x0c,
         // data
         'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };

  PacketFragments packet44 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
       // frame type (stream frame with fin)
       {"",
        {0xFD}},
       // stream id
       {"Unable to read stream_id.",
        {0x03, 0x04}},
       // offset
       {"Unable to read offset.",
        {0x3A, 0x98, 0xFE, 0xDC,
         0x32, 0x10, 0x76, 0x54}},
       {"Unable to read frame data.",
        {
          // data length
          0x00, 0x0c,
          // data
          'h',  'e',  'l',  'l',
          'o',  ' ',  'w',  'o',
          'r',  'l',  'd',  '!'}},
  };

  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (stream frame with LEN, FIN, and OFFSET bits set)
      {"",
       {0x10 | 0x01 | 0x02 | 0x04}},
      // stream id
      {"Unable to read stream_id.",
       {kVarInt62TwoBytes + 0x03, 0x04}},
      // offset
      {"Unable to read stream data offset.",
       {kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      // data length
      {"Unable to read stream data length.",
       {kVarInt62OneByte + 0x0c}},
      // data
      {"Unable to read frame data.",
       { 'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.transport_version() == QUIC_VERSION_99
          ? packet99
          : (framer_.transport_version() > QUIC_VERSION_43
                 ? packet44
                 : (framer_.transport_version() != QUIC_VERSION_35 ? packet39
                                                                   : packet));
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  // Stream ID should be the last 2 bytes of kStreamId.
  EXPECT_EQ(0x0000FFFF & kStreamId, visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(kStreamOffset, visitor_.stream_frames_[0]->offset);
  CheckStreamFrameData("hello world!", visitor_.stream_frames_[0].get());

  CheckFramingBoundaries(fragments, QUIC_INVALID_STREAM_DATA);
}

TEST_P(QuicFramerTest, StreamFrame1ByteStreamId) {
  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id)
      {"",
       {0x28}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x78, 0x56, 0x34, 0x12}},
      // frame type (stream frame with fin)
      {"",
       {0xFC}},
      // stream id
      {"Unable to read stream_id.",
       {0x04}},
      // offset
      {"Unable to read offset.",
       {0x54, 0x76, 0x10, 0x32,
        0xDC, 0xFE, 0x98, 0x3A}},
      {"Unable to read frame data.",
       {
         // data length
         0x0c, 0x00,
         // data
         'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };

  PacketFragments packet39 = {
      // public flags (8 byte connection_id)
      {"",
       {0x28}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (stream frame with fin)
      {"",
       {0xFC}},
      // stream id
      {"Unable to read stream_id.",
       {0x04}},
      // offset
      {"Unable to read offset.",
       {0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      {"Unable to read frame data.",
       {
         // data length
         0x00, 0x0c,
         // data
         'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };

  PacketFragments packet44 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (stream frame with fin)
      {"",
       {0xFC}},
      // stream id
      {"Unable to read stream_id.",
       {0x04}},
      // offset
      {"Unable to read offset.",
       {0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      {"Unable to read frame data.",
       {
         // data length
         0x00, 0x0c,
         // data
         'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };

  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (stream frame with LEN, FIN, and OFFSET bits set)
      {"",
       {0x10 | 0x01 | 0x02 | 0x04}},
      // stream id
      {"Unable to read stream_id.",
       {kVarInt62OneByte + 0x04}},
      // offset
      {"Unable to read stream data offset.",
       {kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      // data length
      {"Unable to read stream data length.",
       {kVarInt62OneByte + 0x0c}},
      // data
      {"Unable to read frame data.",
       { 'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.transport_version() == QUIC_VERSION_99
          ? packet99
          : (framer_.transport_version() > QUIC_VERSION_43
                 ? packet44
                 : (framer_.transport_version() != QUIC_VERSION_35 ? packet39
                                                                   : packet));
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  // Stream ID should be the last 1 byte of kStreamId.
  EXPECT_EQ(0x000000FF & kStreamId, visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(kStreamOffset, visitor_.stream_frames_[0]->offset);
  CheckStreamFrameData("hello world!", visitor_.stream_frames_[0].get());

  CheckFramingBoundaries(fragments, QUIC_INVALID_STREAM_DATA);
}

TEST_P(QuicFramerTest, StreamFrameWithVersion) {
  // clang-format off
  PacketFragments packet = {
      // public flags (version, 8 byte connection_id)
      {"",
       {0x29}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // version tag
      {"",
       {QUIC_VERSION_BYTES}},
      // packet number
      {"",
       {0x78, 0x56, 0x34, 0x12}},
      // frame type (stream frame with fin)
      {"",
       {0xFE}},
      // stream id
      {"Unable to read stream_id.",
       {0x04, 0x03, 0x02}},
      // offset
      {"Unable to read offset.",
       {0x54, 0x76, 0x10, 0x32,
        0xDC, 0xFE, 0x98, 0x3A}},
      {"Unable to read frame data.",
       {
         // data length
         0x0c, 0x00,
         // data
         'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };

  PacketFragments packet39 = {
      // public flags (version, 8 byte connection_id)
      {"",
       {0x29}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // version tag
      {"",
       {QUIC_VERSION_BYTES}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (stream frame with fin)
      {"",
       {0xFE}},
      // stream id
      {"Unable to read stream_id.",
       {0x02, 0x03, 0x04}},
      // offset
      {"Unable to read offset.",
       {0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      {"Unable to read frame data.",
       {
         // data length
         0x00, 0x0c,
         // data
         'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };

  PacketFragments packet44 = {
      // public flags (long header with packet type ZERO_RTT_PROTECTED)
      {"",
       {0xFC}},
      // version tag
      {"",
       {QUIC_VERSION_BYTES}},
      // connection_id length
      {"",
       {0x50}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (stream frame with fin)
      {"",
       {0xFE}},
      // stream id
      {"Unable to read stream_id.",
       {0x02, 0x03, 0x04}},
      // offset
      {"Unable to read offset.",
       {0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      {"Unable to read frame data.",
       {
         // data length
         0x00, 0x0c,
         // data
         'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };

  PacketFragments packet99 = {
      // public flags (long header with packet type ZERO_RTT_PROTECTED)
      {"",
       {0xFC}},
      // version tag
      {"",
       {QUIC_VERSION_BYTES}},
      // connection_id length
      {"",
       {0x50}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (stream frame with FIN, LEN, and OFFSET bits set)
      {"",
       {0x10 | 0x01 | 0x02 | 0x04}},
      // stream id
      {"Unable to read stream_id.",
       {kVarInt62FourBytes + 0x00, 0x02, 0x03, 0x04}},
      // offset
      {"Unable to read stream data offset.",
       {kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      // data length
      {"Unable to read stream data length.",
       {kVarInt62OneByte + 0x0c}},
      // data
      {"Unable to read frame data.",
       { 'h',  'e',  'l',  'l',
         'o',  ' ',  'w',  'o',
         'r',  'l',  'd',  '!'}},
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.transport_version() == QUIC_VERSION_99
          ? packet99
          : (framer_.transport_version() > QUIC_VERSION_43
                 ? packet44
                 : (framer_.transport_version() != QUIC_VERSION_35 ? packet39
                                                                   : packet));
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  // Stream ID should be the last 3 bytes of kStreamId.
  EXPECT_EQ(0x00FFFFFF & kStreamId, visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(kStreamOffset, visitor_.stream_frames_[0]->offset);
  CheckStreamFrameData("hello world!", visitor_.stream_frames_[0].get());

  CheckFramingBoundaries(fragments, QUIC_INVALID_STREAM_DATA);
}

TEST_P(QuicFramerTest, RejectPacket) {
  visitor_.accept_packet_ = false;

  // clang-format off
  unsigned char packet[] = {
      // public flags (8 byte connection_id)
      0x28,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x78, 0x56, 0x34, 0x12,

      // frame type (stream frame with fin)
      0xFF,
      // stream id
      0x04, 0x03, 0x02, 0x01,
      // offset
      0x54, 0x76, 0x10, 0x32,
      0xDC, 0xFE, 0x98, 0x3A,
      // data length
      0x0c, 0x00,
      // data
      'h',  'e',  'l',  'l',
      'o',  ' ',  'w',  'o',
      'r',  'l',  'd',  '!',
  };

  unsigned char packet39[] = {
      // public flags (8 byte connection_id)
      0x28,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (stream frame with fin)
      0xFF,
      // stream id
      0x01, 0x02, 0x03, 0x04,
      // offset
      0x3A, 0x98, 0xFE, 0xDC,
      0x32, 0x10, 0x76, 0x54,
      // data length
      0x00, 0x0c,
      // data
      'h',  'e',  'l',  'l',
      'o',  ' ',  'w',  'o',
      'r',  'l',  'd',  '!',
  };

  unsigned char packet44[] = {
      // type (short header, 4 byte packet number)
      0x32,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (STREAM Frame with FIN, LEN, and OFFSET bits set)
      0x10 | 0x01 | 0x02 | 0x04,
      // stream id
      kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04,
      // offset
      kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
      0x32, 0x10, 0x76, 0x54,
      // data length
      kVarInt62OneByte + 0x0c,
      // data
      'h',  'e',  'l',  'l',
      'o',  ' ',  'w',  'o',
      'r',  'l',  'd',  '!',
  };
  // clang-format on

  unsigned char* p = packet;
  if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
  } else if (framer_.transport_version() != QUIC_VERSION_35) {
    p = packet39;
  }
  QuicEncryptedPacket encrypted(AsChars(p),
                                framer_.transport_version() > QUIC_VERSION_43
                                    ? QUIC_ARRAYSIZE(packet44)
                                    : QUIC_ARRAYSIZE(packet),
                                false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  ASSERT_EQ(0u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
}

TEST_P(QuicFramerTest, RejectPublicHeader) {
  visitor_.accept_public_header_ = false;

  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
  };

  unsigned char packet44[] = {
    // type (short header, 1 byte packet number)
    0x30,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x01,
  };
  // clang-format on

  QuicEncryptedPacket encrypted(
      framer_.transport_version() > QUIC_VERSION_43 ? AsChars(packet44)
                                                    : AsChars(packet),
      framer_.transport_version() > QUIC_VERSION_43 ? QUIC_ARRAYSIZE(packet44)
                                                    : QUIC_ARRAYSIZE(packet),
      false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(0u, visitor_.header_->packet_number);
}

TEST_P(QuicFramerTest, AckFrameOneAckBlock) {
  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id)
      {"",
       {0x2C}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x78, 0x56, 0x34, 0x12}},
      // frame type (ack frame)
      // (one ack block, 2 byte largest observed, 2 byte block length)
      {"",
       {0x45}},
      // largest acked
      {"Unable to read largest acked.",
       {0x34, 0x12}},
      // Zero delta time.
      {"Unable to read ack delay time.",
       {0x00, 0x00}},
      // first ack block length.
      {"Unable to read first ack block length.",
       {0x34, 0x12}},
      // num timestamps.
      {"Unable to read num received packets.",
       {0x00}}
  };

  PacketFragments packet39 = {
      // public flags (8 byte connection_id)
      {"",
       {0x2C}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (ack frame)
      // (one ack block, 2 byte largest observed, 2 byte block length)
      {"",
       {0x45}},
      // largest acked
      {"Unable to read largest acked.",
       {0x12, 0x34}},
      // Zero delta time.
      {"Unable to read ack delay time.",
       {0x00, 0x00}},
      // first ack block length.
      {"Unable to read first ack block length.",
       {0x12, 0x34}},
      // num timestamps.
      {"Unable to read num received packets.",
       {0x00}}
  };

  PacketFragments packet44 = {
      // type (short packet, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (ack frame)
      // (one ack block, 2 byte largest observed, 2 byte block length)
      {"",
       {0x45}},
      // largest acked
      {"Unable to read largest acked.",
       {0x12, 0x34}},
      // Zero delta time.
      {"Unable to read ack delay time.",
       {0x00, 0x00}},
      // first ack block length.
      {"Unable to read first ack block length.",
       {0x12, 0x34}},
      // num timestamps.
      {"Unable to read num received packets.",
       {0x00}}
  };

  PacketFragments packet99 = {
      // type (short packet, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
       // frame type (ack frame)
       // (one ack block, 2 byte largest observed, 2 byte block length)
       // IETF-Quic ignores the bit-fields in the ack type, all of
       // that information is encoded elsewhere in the frame.
       {"",
        {0x1a}},
       // largest acked
       {"Unable to read largest acked.",
        {kVarInt62TwoBytes  + 0x12, 0x34}},
       // Zero delta time.
       {"Unable to read ack delay time.",
        {kVarInt62OneByte + 0x00}},
      // Ack block count (0 -- no blocks after the first)
      {"Unable to read ack block count.",
       {kVarInt62OneByte + 0x00}},
       // first ack block length - 1.
       // IETF Quic defines the ack block's value as the "number of
       // packets that preceed the largest packet number in the block"
       // which for the 1st ack block is the largest acked field,
       // above. This means that if we are acking just packet 0x1234
       // then the 1st ack block will be 0.
       {"Unable to read first ack block length.",
        {kVarInt62TwoBytes + 0x12, 0x33}}
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.transport_version() == QUIC_VERSION_99
          ? packet99
          : (framer_.transport_version() > QUIC_VERSION_43
                 ? packet44
                 : (framer_.transport_version() != QUIC_VERSION_35 ? packet39
                                                                   : packet));
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  ASSERT_EQ(1u, visitor_.ack_frames_.size());
  const QuicAckFrame& frame = *visitor_.ack_frames_[0];
  EXPECT_EQ(kSmallLargestObserved, LargestAcked(frame));
  ASSERT_EQ(4660u, frame.packets.NumPacketsSlow());

  CheckFramingBoundaries(fragments, QUIC_INVALID_ACK_DATA);
}

// This test checks that the ack frame processor correctly identifies
// and handles the case where the first ack block is larger than the
// largest_acked packet.
TEST_P(QuicFramerTest, FirstAckFrameUnderflow) {
  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id)
      {"",
       {0x2C}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x78, 0x56, 0x34, 0x12}},
      // frame type (ack frame)
      // (one ack block, 2 byte largest observed, 2 byte block length)
      {"",
       {0x45}},
      // largest acked
      {"Unable to read largest acked.",
       {0x34, 0x12}},
      // Zero delta time.
      {"Unable to read ack delay time.",
       {0x00, 0x00}},
      // first ack block length.
      {"Unable to read first ack block length.",
       {0x88, 0x88}},
      // num timestamps.
      {"Underflow with first ack block length 34952 largest acked is 4660.",
       {0x00}}
  };

  PacketFragments packet39 = {
      // public flags (8 byte connection_id)
      {"",
       {0x2C}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (ack frame)
      // (one ack block, 2 byte largest observed, 2 byte block length)
      {"",
       {0x45}},
      // largest acked
      {"Unable to read largest acked.",
       {0x12, 0x34}},
      // Zero delta time.
      {"Unable to read ack delay time.",
       {0x00, 0x00}},
      // first ack block length.
      {"Unable to read first ack block length.",
       {0x88, 0x88}},
      // num timestamps.
      {"Underflow with first ack block length 34952 largest acked is 4660.",
       {0x00}}
  };

  PacketFragments packet44 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (ack frame)
      // (one ack block, 2 byte largest observed, 2 byte block length)
      {"",
       {0x45}},
      // largest acked
      {"Unable to read largest acked.",
       {0x12, 0x34}},
      // Zero delta time.
      {"Unable to read ack delay time.",
       {0x00, 0x00}},
      // first ack block length.
      {"Unable to read first ack block length.",
       {0x88, 0x88}},
      // num timestamps.
      {"Underflow with first ack block length 34952 largest acked is 4660.",
       {0x00}}
  };

  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
       // frame type (ack frame)
       {"",
        {0x1a}},
       // largest acked
       {"Unable to read largest acked.",
        {kVarInt62TwoBytes  + 0x12, 0x34}},
       // Zero delta time.
       {"Unable to read ack delay time.",
        {kVarInt62OneByte + 0x00}},
       // Ack block count (0 -- no blocks after the first)
       {"Unable to read ack block count.",
        {kVarInt62OneByte + 0x00}},
       // first ack block length.
       {"Unable to read first ack block length.",
        {kVarInt62TwoBytes + 0x28, 0x88}}
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.transport_version() == QUIC_VERSION_99
          ? packet99
          : (framer_.transport_version() > QUIC_VERSION_43
                 ? packet44
                 : (framer_.transport_version() != QUIC_VERSION_35 ? packet39
                                                                   : packet));
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  CheckFramingBoundaries(fragments, QUIC_INVALID_ACK_DATA);
}

// This test checks that the ack frame processor correctly identifies
// and handles the case where the third ack block's gap is larger than the
// available space in the ack range.
TEST_P(QuicFramerTest, ThirdAckBlockUnderflowGap) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // for now, only v99
    return;
  }
  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
       // frame type (ack frame)
       {"",
        {0x1a}},
       // largest acked
       {"Unable to read largest acked.",
        {kVarInt62OneByte  + 63}},
       // Zero delta time.
       {"Unable to read ack delay time.",
        {kVarInt62OneByte + 0x00}},
       // Ack block count (2 -- 2 blocks after the first)
       {"Unable to read ack block count.",
        {kVarInt62OneByte + 0x02}},
       // first ack block length.
       {"Unable to read first ack block length.",
        {kVarInt62OneByte + 13}},  // Ack 14 packets, range 50..63 (inclusive)

       {"Unable to read gap block value.",
        {kVarInt62OneByte + 9}},  // Gap 10 packets, 40..49 (inclusive)
       {"Unable to read ack block value.",
        {kVarInt62OneByte + 9}},  // Ack 10 packets, 30..39 (inclusive)
       {"Unable to read gap block value.",
        {kVarInt62OneByte + 29}},  // A gap of 30 packets (0..29 inclusive)
                                   // should be too big, leaving no room
                                   // for the ack.
       {"Underflow with gap block length 30 previous ack block start is 30.",
        {kVarInt62OneByte + 10}},  // Don't care
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(
      framer_.detailed_error(),
      "Underflow with gap block length 30 previous ack block start is 30.");
  CheckFramingBoundaries(packet99, QUIC_INVALID_ACK_DATA);
}

// This test checks that the ack frame processor correctly identifies
// and handles the case where the third ack block's length is larger than the
// available space in the ack range.
TEST_P(QuicFramerTest, ThirdAckBlockUnderflowAck) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // for now, only v99
    return;
  }
  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
       // frame type (ack frame)
       {"",
        {0x1a}},
       // largest acked
       {"Unable to read largest acked.",
        {kVarInt62OneByte  + 63}},
       // Zero delta time.
       {"Unable to read ack delay time.",
        {kVarInt62OneByte + 0x00}},
       // Ack block count (2 -- 2 blocks after the first)
       {"Unable to read ack block count.",
        {kVarInt62OneByte + 0x02}},
       // first ack block length.
       {"Unable to read first ack block length.",
        {kVarInt62OneByte + 13}},  // only 50 packet numbers "left"

       {"Unable to read gap block value.",
        {kVarInt62OneByte + 10}},  // Only 40 packet numbers left
       {"Unable to read ack block value.",
        {kVarInt62OneByte + 10}},  // only 30 packet numbers left.
       {"Unable to read gap block value.",
        {kVarInt62OneByte + 1}},  // Gap is OK, 29 packet numbers left
      {"Unable to read ack block value.",
        {kVarInt62OneByte + 30}},  // Use up all 30, should be an error
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(framer_.detailed_error(),
            "Underflow with ack block length 31 latest ack block end is 25.");
  CheckFramingBoundaries(packet99, QUIC_INVALID_ACK_DATA);
}

// Tests a variety of ack block wrap scenarios. For example, if the
// N-1th block causes packet 0 to be acked, then a gap would wrap
// around to 0x3fffffff ffffffff... Make sure we detect this
// condition.
TEST_P(QuicFramerTest, AckBlockUnderflowGapWrap) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // for now, only v99
    return;
  }
  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
       // frame type (ack frame)
       {"",
        {0x1a}},
       // largest acked
       {"Unable to read largest acked.",
        {kVarInt62OneByte  + 10}},
       // Zero delta time.
       {"Unable to read ack delay time.",
        {kVarInt62OneByte + 0x00}},
       // Ack block count (1 -- 1 blocks after the first)
       {"Unable to read ack block count.",
        {kVarInt62OneByte + 1}},
       // first ack block length.
       {"Unable to read first ack block length.",
        {kVarInt62OneByte + 9}},  // Ack packets 1..10 (inclusive)

       {"Unable to read gap block value.",
        {kVarInt62OneByte + 1}},  // Gap of 2 packets (-1...0), should wrap
       {"Underflow with gap block length 2 previous ack block start is 1.",
        {kVarInt62OneByte + 9}},  // irrelevant
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(framer_.detailed_error(),
            "Underflow with gap block length 2 previous ack block start is 1.");
  CheckFramingBoundaries(packet99, QUIC_INVALID_ACK_DATA);
}

// As AckBlockUnderflowGapWrap, but in this test, it's the ack
// component of the ack-block that causes the wrap, not the gap.
TEST_P(QuicFramerTest, AckBlockUnderflowAckWrap) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // for now, only v99
    return;
  }
  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
       // frame type (ack frame)
       {"",
        {0x1a}},
       // largest acked
       {"Unable to read largest acked.",
        {kVarInt62OneByte  + 10}},
       // Zero delta time.
       {"Unable to read ack delay time.",
        {kVarInt62OneByte + 0x00}},
       // Ack block count (1 -- 1 blocks after the first)
       {"Unable to read ack block count.",
        {kVarInt62OneByte + 1}},
       // first ack block length.
       {"Unable to read first ack block length.",
        {kVarInt62OneByte + 6}},  // Ack packets 4..10 (inclusive)

       {"Unable to read gap block value.",
        {kVarInt62OneByte + 1}},  // Gap of 2 packets (2..3)
       {"Unable to read ack block value.",
        {kVarInt62OneByte + 9}},  // Should wrap.
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(framer_.detailed_error(),
            "Underflow with ack block length 10 latest ack block end is 1.");
  CheckFramingBoundaries(packet99, QUIC_INVALID_ACK_DATA);
}

// An ack block that acks the entire range, 0...0x3fffffffffffffff
TEST_P(QuicFramerTest, AckBlockAcksEverything) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // for now, only v99
    return;
  }
  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
       // frame type (ack frame)
       {"",
        {0x1a}},
       // largest acked
       {"Unable to read largest acked.",
        {kVarInt62EightBytes  + 0x3f, 0xff, 0xff, 0xff,
         0xff, 0xff, 0xff, 0xff}},
       // Zero delta time.
       {"Unable to read ack delay time.",
        {kVarInt62OneByte + 0x00}},
       // Ack block count No additional blocks
       {"Unable to read ack block count.",
        {kVarInt62OneByte + 0}},
       // first ack block length.
       {"Unable to read first ack block length.",
        {kVarInt62EightBytes  + 0x3f, 0xff, 0xff, 0xff,
         0xff, 0xff, 0xff, 0xff}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(1u, visitor_.ack_frames_.size());
  const QuicAckFrame& frame = *visitor_.ack_frames_[0];
  EXPECT_EQ(1u, frame.packets.NumIntervals());
  EXPECT_EQ(kLargestIetfLargestObserved, LargestAcked(frame));
  // +1 because it's 0..Largest, inclusive.
  EXPECT_EQ(kLargestIetfLargestObserved + 1, frame.packets.NumPacketsSlow());
}

// This test looks for a malformed ack where
//  - There is a largest-acked value (that is, the frame is acking
//    something,
//  - But the length of the first ack block is 0 saying that no frames
//    are being acked with the largest-acked value or there are no
//    additional ack blocks.
//
TEST_P(QuicFramerTest, AckFrameFirstAckBlockLengthZero) {
  if (framer_.transport_version() == QUIC_VERSION_99) {
    // Not applicable to version 99 -- first ack block contains the
    // number of packets that preceed the largest_acked packet.
    // A value of 0 means no packets preceed --- that the block's
    // length is 1. Therefore the condition that this test checks can
    // not arise.
    return;
  }

  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id)
      {"",
       { 0x2C }},
      // connection_id
      {"",
       { 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10 }},
      // packet number
      {"",
       { 0x78, 0x56, 0x34, 0x12 }},

      // frame type (ack frame)
      // (more than one ack block, 2 byte largest observed, 2 byte block length)
      {"",
       { 0x65 }},
      // largest acked
      {"Unable to read largest acked.",
       { 0x34, 0x12 }},
      // Zero delta time.
      {"Unable to read ack delay time.",
       { 0x00, 0x00 }},
      // num ack blocks ranges.
      {"Unable to read num of ack blocks.",
       { 0x01 }},
      // first ack block length.
      {"Unable to read first ack block length.",
       { 0x00, 0x00 }},
      // gap to next block.
      { "First block length is zero but ACK is not empty. "
        "largest acked is 4660, num ack blocks is 1.",
        { 0x01 }},
      // ack block length.
      { "First block length is zero but ACK is not empty. "
        "largest acked is 4660, num ack blocks is 1.",
        { 0xaf, 0x0e }},
      // Number of timestamps.
      { "First block length is zero but ACK is not empty. "
        "largest acked is 4660, num ack blocks is 1.",
        { 0x00 }},
  };

  PacketFragments packet39 = {
      // public flags (8 byte connection_id)
      {"",
       { 0x2C }},
      // connection_id
      {"",
       { 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10 }},
      // packet number
      {"",
       { 0x12, 0x34, 0x56, 0x78 }},

      // frame type (ack frame)
      // (more than one ack block, 2 byte largest observed, 2 byte block length)
      {"",
       { 0x65 }},
      // largest acked
      {"Unable to read largest acked.",
       { 0x12, 0x34 }},
      // Zero delta time.
      {"Unable to read ack delay time.",
       { 0x00, 0x00 }},
      // num ack blocks ranges.
      {"Unable to read num of ack blocks.",
       { 0x01 }},
      // first ack block length.
      {"Unable to read first ack block length.",
       { 0x00, 0x00 }},
      // gap to next block.
      { "First block length is zero but ACK is not empty. "
        "largest acked is 4660, num ack blocks is 1.",
        { 0x01 }},
      // ack block length.
      { "First block length is zero but ACK is not empty. "
        "largest acked is 4660, num ack blocks is 1.",
        { 0x0e, 0xaf }},
      // Number of timestamps.
      { "First block length is zero but ACK is not empty. "
        "largest acked is 4660, num ack blocks is 1.",
        { 0x00 }},
  };

  PacketFragments packet44 = {
      // type (short header, 4 byte packet number)
      {"",
       { 0x32 }},
      // connection_id
      {"",
       { 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10 }},
      // packet number
      {"",
       { 0x12, 0x34, 0x56, 0x78 }},

      // frame type (ack frame)
      // (more than one ack block, 2 byte largest observed, 2 byte block length)
      {"",
       { 0x65 }},
      // largest acked
      {"Unable to read largest acked.",
       { 0x12, 0x34 }},
      // Zero delta time.
      {"Unable to read ack delay time.",
       { 0x00, 0x00 }},
      // num ack blocks ranges.
      {"Unable to read num of ack blocks.",
       { 0x01 }},
      // first ack block length.
      {"Unable to read first ack block length.",
       { 0x00, 0x00 }},
      // gap to next block.
      { "First block length is zero but ACK is not empty. "
        "largest acked is 4660, num ack blocks is 1.",
        { 0x01 }},
      // ack block length.
      { "First block length is zero but ACK is not empty. "
        "largest acked is 4660, num ack blocks is 1.",
        { 0x0e, 0xaf }},
      // Number of timestamps.
      { "First block length is zero but ACK is not empty. "
        "largest acked is 4660, num ack blocks is 1.",
        { 0x00 }},
  };

  // clang-format on
  PacketFragments& fragments =
      framer_.transport_version() > QUIC_VERSION_43
          ? packet44
          : (framer_.transport_version() != QUIC_VERSION_35 ? packet39
                                                            : packet);

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));

  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));
  EXPECT_EQ(QUIC_INVALID_ACK_DATA, framer_.error());

  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  ASSERT_EQ(1u, visitor_.ack_frames_.size());

  CheckFramingBoundaries(fragments, QUIC_INVALID_ACK_DATA);
}

TEST_P(QuicFramerTest, AckFrameOneAckBlockMaxLength) {
  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id)
      {"",
       {0x2C}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x78, 0x56, 0x34, 0x12}},
      // frame type (ack frame)
      // (one ack block, 4 byte largest observed, 2 byte block length)
      {"",
       {0x49}},
      // largest acked
      {"Unable to read largest acked.",
       {0x78, 0x56, 0x34, 0x12}},
      // Zero delta time.
      {"Unable to read ack delay time.",
       {0x00, 0x00}},
      // first ack block length.
      {"Unable to read first ack block length.",
       {0x34, 0x12}},
      // num timestamps.
      {"Unable to read num received packets.",
       {0x00}}
  };

  PacketFragments packet39 = {
      // public flags (8 byte connection_id)
      {"",
       {0x2C}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (ack frame)
      // (one ack block, 4 byte largest observed, 2 byte block length)
      {"",
       {0x49}},
      // largest acked
      {"Unable to read largest acked.",
       {0x12, 0x34, 0x56, 0x78}},
      // Zero delta time.
      {"Unable to read ack delay time.",
       {0x00, 0x00}},
      // first ack block length.
      {"Unable to read first ack block length.",
       {0x12, 0x34}},
      // num timestamps.
      {"Unable to read num received packets.",
       {0x00}}
  };

  PacketFragments packet44 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x56, 0x78, 0x9A, 0xBC}},
      // frame type (ack frame)
      // (one ack block, 4 byte largest observed, 2 byte block length)
      {"",
       {0x49}},
      // largest acked
      {"Unable to read largest acked.",
       {0x12, 0x34, 0x56, 0x78}},
      // Zero delta time.
      {"Unable to read ack delay time.",
       {0x00, 0x00}},
      // first ack block length.
      {"Unable to read first ack block length.",
       {0x12, 0x34}},
      // num timestamps.
      {"Unable to read num received packets.",
       {0x00}}
  };

  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x56, 0x78, 0x9A, 0xBC}},
       // frame type (ack frame)
       {"",
        {0x1a}},
       // largest acked
       {"Unable to read largest acked.",
        {kVarInt62FourBytes  + 0x12, 0x34, 0x56, 0x78}},
       // Zero delta time.
       {"Unable to read ack delay time.",
        {kVarInt62OneByte + 0x00}},
       // Number of ack blocks after first
       {"Unable to read ack block count.",
        {kVarInt62OneByte + 0x00}},
       // first ack block length.
       {"Unable to read first ack block length.",
        {kVarInt62TwoBytes  + 0x12, 0x33}}
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.transport_version() == QUIC_VERSION_99
          ? packet99
          : (framer_.transport_version() > QUIC_VERSION_43
                 ? packet44
                 : (framer_.transport_version() != QUIC_VERSION_35 ? packet39
                                                                   : packet));
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  ASSERT_EQ(1u, visitor_.ack_frames_.size());
  const QuicAckFrame& frame = *visitor_.ack_frames_[0];
  EXPECT_EQ(kPacketNumber, LargestAcked(frame));
  ASSERT_EQ(4660u, frame.packets.NumPacketsSlow());

  CheckFramingBoundaries(fragments, QUIC_INVALID_ACK_DATA);
}

// Tests ability to handle multiple ackblocks after the first ack
// block. Non-version-99 tests include multiple timestamps as well.
TEST_P(QuicFramerTest, AckFrameTwoTimeStampsMultipleAckBlocks) {
  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id)
      {"",
       { 0x2C }},
      // connection_id
      {"",
       { 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10 }},
      // packet number
      {"",
       { 0x78, 0x56, 0x34, 0x12 }},

      // frame type (ack frame)
      // (more than one ack block, 2 byte largest observed, 2 byte block length)
      {"",
       { 0x65 }},
      // largest acked
      {"Unable to read largest acked.",
       { 0x34, 0x12 }},
      // Zero delta time.
      {"Unable to read ack delay time.",
       { 0x00, 0x00 }},
      // num ack blocks ranges.
      {"Unable to read num of ack blocks.",
       { 0x04 }},
      // first ack block length.
      {"Unable to read first ack block length.",
       { 0x01, 0x00 }},
      // gap to next block.
      { "Unable to read gap to next ack block.",
        { 0x01 }},
      // ack block length.
      { "Unable to ack block length.",
        { 0xaf, 0x0e }},
      // gap to next block.
      { "Unable to read gap to next ack block.",
        { 0xff }},
      // ack block length.
      { "Unable to ack block length.",
        { 0x00, 0x00 }},
      // gap to next block.
      { "Unable to read gap to next ack block.",
        { 0x91 }},
      // ack block length.
      { "Unable to ack block length.",
        { 0xea, 0x01 }},
      // gap to next block.
      { "Unable to read gap to next ack block.",
        { 0x05 }},
      // ack block length.
      { "Unable to ack block length.",
        { 0x04, 0x00 }},
      // Number of timestamps.
      { "Unable to read num received packets.",
        { 0x02 }},
      // Delta from largest observed.
      { "Unable to read sequence delta in received packets.",
        { 0x01 }},
      // Delta time.
      { "Unable to read time delta in received packets.",
        { 0x10, 0x32, 0x54, 0x76 }},
      // Delta from largest observed.
      { "Unable to read sequence delta in received packets.",
        { 0x02 }},
      // Delta time.
      { "Unable to read incremental time delta in received packets.",
        { 0x10, 0x32 }},
  };

  PacketFragments packet39 = {
      // public flags (8 byte connection_id)
      {"",
       { 0x2C }},
      // connection_id
      {"",
       { 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10 }},
      // packet number
      {"",
       { 0x12, 0x34, 0x56, 0x78 }},

      // frame type (ack frame)
      // (more than one ack block, 2 byte largest observed, 2 byte block length)
      {"",
       { 0x65 }},
      // largest acked
      {"Unable to read largest acked.",
       { 0x12, 0x34 }},
      // Zero delta time.
      {"Unable to read ack delay time.",
       { 0x00, 0x00 }},
      // num ack blocks ranges.
      {"Unable to read num of ack blocks.",
       { 0x04 }},
      // first ack block length.
      {"Unable to read first ack block length.",
       { 0x00, 0x01 }},
      // gap to next block.
      { "Unable to read gap to next ack block.",
        { 0x01 }},
      // ack block length.
      { "Unable to ack block length.",
        { 0x0e, 0xaf }},
      // gap to next block.
      { "Unable to read gap to next ack block.",
        { 0xff }},
      // ack block length.
      { "Unable to ack block length.",
        { 0x00, 0x00 }},
      // gap to next block.
      { "Unable to read gap to next ack block.",
        { 0x91 }},
      // ack block length.
      { "Unable to ack block length.",
        { 0x01, 0xea }},
      // gap to next block.
      { "Unable to read gap to next ack block.",
        { 0x05 }},
      // ack block length.
      { "Unable to ack block length.",
        { 0x00, 0x04 }},
      // Number of timestamps.
      { "Unable to read num received packets.",
        { 0x02 }},
      // Delta from largest observed.
      { "Unable to read sequence delta in received packets.",
        { 0x01 }},
      // Delta time.
      { "Unable to read time delta in received packets.",
        { 0x76, 0x54, 0x32, 0x10 }},
      // Delta from largest observed.
      { "Unable to read sequence delta in received packets.",
        { 0x02 }},
      // Delta time.
      { "Unable to read incremental time delta in received packets.",
        { 0x32, 0x10 }},
  };

  PacketFragments packet44 = {
      // type (short header, 4 byte packet number)
      {"",
       { 0x32 }},
      // connection_id
      {"",
       { 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10 }},
      // packet number
      {"",
       { 0x12, 0x34, 0x56, 0x78 }},

      // frame type (ack frame)
      // (more than one ack block, 2 byte largest observed, 2 byte block length)
      {"",
       { 0x65 }},
      // largest acked
      {"Unable to read largest acked.",
       { 0x12, 0x34 }},
      // Zero delta time.
      {"Unable to read ack delay time.",
       { 0x00, 0x00 }},
      // num ack blocks ranges.
      {"Unable to read num of ack blocks.",
       { 0x04 }},
      // first ack block length.
      {"Unable to read first ack block length.",
       { 0x00, 0x01 }},
      // gap to next block.
      { "Unable to read gap to next ack block.",
        { 0x01 }},
      // ack block length.
      { "Unable to ack block length.",
        { 0x0e, 0xaf }},
      // gap to next block.
      { "Unable to read gap to next ack block.",
        { 0xff }},
      // ack block length.
      { "Unable to ack block length.",
        { 0x00, 0x00 }},
      // gap to next block.
      { "Unable to read gap to next ack block.",
        { 0x91 }},
      // ack block length.
      { "Unable to ack block length.",
        { 0x01, 0xea }},
      // gap to next block.
      { "Unable to read gap to next ack block.",
        { 0x05 }},
      // ack block length.
      { "Unable to ack block length.",
        { 0x00, 0x04 }},
      // Number of timestamps.
      { "Unable to read num received packets.",
        { 0x02 }},
      // Delta from largest observed.
      { "Unable to read sequence delta in received packets.",
        { 0x01 }},
      // Delta time.
      { "Unable to read time delta in received packets.",
        { 0x76, 0x54, 0x32, 0x10 }},
      // Delta from largest observed.
      { "Unable to read sequence delta in received packets.",
        { 0x02 }},
      // Delta time.
      { "Unable to read incremental time delta in received packets.",
        { 0x32, 0x10 }},
  };

  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       { 0x32 }},
      // connection_id
      {"",
       { 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10 }},
      // packet number
      {"",
       { 0x12, 0x34, 0x56, 0x78 }},

      // frame type (ack frame)
      {"",
       { 0x1a }},
       // largest acked
       {"Unable to read largest acked.",
        { kVarInt62TwoBytes + 0x12, 0x34 }},   // = 4660
       // Zero delta time.
       {"Unable to read ack delay time.",
        { kVarInt62OneByte + 0x00 }},
       // number of additional ack blocks
       {"Unable to read ack block count.",
        { kVarInt62OneByte + 0x03 }},
       // first ack block length.
       {"Unable to read first ack block length.",
        { kVarInt62OneByte + 0x00 }},  // 1st block length = 1

       // Additional ACK Block #1
       // gap to next block.
       { "Unable to read gap block value.",
         { kVarInt62OneByte + 0x00 }},   // gap of 1 packet
       // ack block length.
       { "Unable to read ack block value.",
         { kVarInt62TwoBytes + 0x0e, 0xae }},   // 3759

       // pre-version-99 test includes an ack block of 0 length. this
       // can not happen in version 99. ergo the second block is not
       // present in the v99 test and the gap length of the next block
       // is the sum of the two gaps in the pre-version-99 tests.
       // Additional ACK Block #2
       // gap to next block.
       { "Unable to read gap block value.",
         { kVarInt62TwoBytes + 0x01, 0x8f }},  // Gap is 400 (0x190) pkts
       // ack block length.
       { "Unable to read ack block value.",
         { kVarInt62TwoBytes + 0x01, 0xe9 }},  // block is 389 (x1ea) pkts

       // Additional ACK Block #3
       // gap to next block.
       { "Unable to read gap block value.",
         { kVarInt62OneByte + 0x04 }},   // Gap is 5 packets.
       // ack block length.
       { "Unable to read ack block value.",
         { kVarInt62OneByte + 0x03 }},   // block is 3 packets.
  };

  // clang-format on
  PacketFragments& fragments =
      framer_.transport_version() == QUIC_VERSION_99
          ? packet99
          : (framer_.transport_version() > QUIC_VERSION_43
                 ? packet44
                 : (framer_.transport_version() != QUIC_VERSION_35 ? packet39
                                                                   : packet));

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));

  framer_.set_process_timestamps(true);
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  ASSERT_EQ(1u, visitor_.ack_frames_.size());
  const QuicAckFrame& frame = *visitor_.ack_frames_[0];
  EXPECT_EQ(kSmallLargestObserved, LargestAcked(frame));
  ASSERT_EQ(4254u, frame.packets.NumPacketsSlow());
  EXPECT_EQ(4u, frame.packets.NumIntervals());
  if (framer_.transport_version() == QUIC_VERSION_99) {
    EXPECT_EQ(0u, frame.received_packet_times.size());
  } else {
    EXPECT_EQ(2u, frame.received_packet_times.size());
  }
  CheckFramingBoundaries(fragments, QUIC_INVALID_ACK_DATA);
}

TEST_P(QuicFramerTest, NewStopWaitingFrame) {
  if (version_.transport_version == QUIC_VERSION_99) {
    return;
  }
  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id)
      {"",
       {0x2C}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x78, 0x56, 0x34, 0x12}},
      // frame type (stop waiting frame)
      {"",
       {0x06}},
      // least packet number awaiting an ack, delta from packet number.
      {"Unable to read least unacked delta.",
        {0x08, 0x00, 0x00, 0x00}}
  };

  PacketFragments packet39 = {
      // public flags (8 byte connection_id)
      {"",
       {0x2C}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (stop waiting frame)
      {"",
       {0x06}},
      // least packet number awaiting an ack, delta from packet number.
      {"Unable to read least unacked delta.",
        {0x00, 0x00, 0x00, 0x08}}
  };

  PacketFragments packet44 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (stop waiting frame)
      {"",
       {0x06}},
      // least packet number awaiting an ack, delta from packet number.
      {"Unable to read least unacked delta.",
        {0x00, 0x00, 0x00, 0x08}}
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.transport_version() > QUIC_VERSION_43
          ? packet44
          : (framer_.transport_version() != QUIC_VERSION_35 ? packet39
                                                            : packet);
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  ASSERT_EQ(1u, visitor_.stop_waiting_frames_.size());
  const QuicStopWaitingFrame& frame = *visitor_.stop_waiting_frames_[0];
  EXPECT_EQ(kLeastUnacked, frame.least_unacked);

  CheckFramingBoundaries(fragments, QUIC_INVALID_STOP_WAITING_DATA);
}

TEST_P(QuicFramerTest, InvalidNewStopWaitingFrame) {
  if (version_.transport_version == QUIC_VERSION_99) {
    return;
  }
  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x2C,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78, 0x56, 0x34, 0x12,
    // frame type (stop waiting frame)
    0x06,
    // least packet number awaiting an ack, delta from packet number.
    0xA8, 0x9A, 0x78, 0x56,
    0x34, 0x13,
  };

  unsigned char packet39[] = {
    // public flags (8 byte connection_id)
    0x2C,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,
    // frame type (stop waiting frame)
    0x06,
    // least packet number awaiting an ack, delta from packet number.
    0x13, 0x34, 0x56, 0x78,
    0x9A, 0xA8,
  };

  unsigned char packet44[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,
    // frame type (stop waiting frame)
    0x06,
    // least packet number awaiting an ack, delta from packet number.
    0x57, 0x78, 0x9A, 0xA8,
  };
  // clang-format on

  QuicEncryptedPacket encrypted(
      AsChars(framer_.transport_version() > QUIC_VERSION_43
                  ? packet44
                  : (framer_.transport_version() == QUIC_VERSION_35
                         ? packet
                         : packet39)),
      framer_.transport_version() > QUIC_VERSION_43 ? QUIC_ARRAYSIZE(packet44)
                                                    : QUIC_ARRAYSIZE(packet),
      false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_INVALID_STOP_WAITING_DATA, framer_.error());
  EXPECT_EQ("Invalid unacked delta.", framer_.detailed_error());
}

TEST_P(QuicFramerTest, RstStreamFrame) {
  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id)
      {"",
       {0x28}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x78, 0x56, 0x34, 0x12}},
      // frame type (rst stream frame)
      {"",
       {0x01}},
      // stream id
      {"Unable to read stream_id.",
       {0x04, 0x03, 0x02, 0x01}},
      // sent byte offset
      {"Unable to read rst stream sent byte offset.",
       {0x54, 0x76, 0x10, 0x32,
        0xDC, 0xFE, 0x98, 0x3A}},
      // error code
      {"Unable to read rst stream error code.",
       {0x01, 0x00, 0x00, 0x00}}
  };

  PacketFragments packet39 = {
      // public flags (8 byte connection_id)
      {"",
       {0x28}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (rst stream frame)
      {"",
       {0x01}},
      // stream id
      {"Unable to read stream_id.",
       {0x01, 0x02, 0x03, 0x04}},
      // sent byte offset
      {"Unable to read rst stream sent byte offset.",
       {0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      // error code
      {"Unable to read rst stream error code.",
       {0x00, 0x00, 0x00, 0x01}}
  };

  PacketFragments packet44 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (rst stream frame)
      {"",
       {0x01}},
      // stream id
      {"Unable to read stream_id.",
       {0x01, 0x02, 0x03, 0x04}},
      // sent byte offset
      {"Unable to read rst stream sent byte offset.",
       {0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
      // error code
      {"Unable to read rst stream error code.",
       {0x00, 0x00, 0x00, 0x01}}
  };

  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (rst stream frame)
      {"",
       {0x01}},
      // stream id
      {"Unable to read rst stream stream id.",
       {kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04}},
      // application error code
      {"Unable to read rst stream error code.",
       {0x00, 0x01}},   // Not varint62 encoded
      // Final Offset
      {"Unable to read rst stream sent byte offset.",
       {kVarInt62EightBytes + 0x3a, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54}}
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.transport_version() == QUIC_VERSION_99
          ? packet99
          : (framer_.transport_version() > QUIC_VERSION_43
                 ? packet44
                 : (framer_.transport_version() != QUIC_VERSION_35 ? packet39
                                                                   : packet));
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(kStreamId, visitor_.rst_stream_frame_.stream_id);
  EXPECT_EQ(0x01, visitor_.rst_stream_frame_.error_code);
  EXPECT_EQ(kStreamOffset, visitor_.rst_stream_frame_.byte_offset);
  CheckFramingBoundaries(fragments, QUIC_INVALID_RST_STREAM_DATA);
}

TEST_P(QuicFramerTest, ConnectionCloseFrame) {
  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id)
      {"",
       {0x28}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x78, 0x56, 0x34, 0x12}},
      // frame type (connection close frame)
      {"",
       {0x02}},
      // error code
      {"Unable to read connection close error code.",
       {0x11, 0x00, 0x00, 0x00}},
      {"Unable to read connection close error details.",
       {
         // error details length
         0x0d, 0x00,
         // error details
         'b',  'e',  'c',  'a',
         'u',  's',  'e',  ' ',
         'I',  ' ',  'c',  'a',
         'n'}
      }
  };

  PacketFragments packet39 = {
      // public flags (8 byte connection_id)
      {"",
       {0x28}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (connection close frame)
      {"",
       {0x02}},
      // error code
      {"Unable to read connection close error code.",
       {0x00, 0x00, 0x00, 0x11}},
      {"Unable to read connection close error details.",
       {
         // error details length
         0x0, 0x0d,
         // error details
         'b',  'e',  'c',  'a',
         'u',  's',  'e',  ' ',
         'I',  ' ',  'c',  'a',
         'n'}
      }
  };

  PacketFragments packet44 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (connection close frame)
      {"",
       {0x02}},
      // error code
      {"Unable to read connection close error code.",
       {0x00, 0x00, 0x00, 0x11}},
      {"Unable to read connection close error details.",
       {
         // error details length
         0x0, 0x0d,
         // error details
         'b',  'e',  'c',  'a',
         'u',  's',  'e',  ' ',
         'I',  ' ',  'c',  'a',
         'n'}
      }
  };

  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (connection close frame)
      {"",
       {0x02}},
      // error code
      {"Unable to read connection close error code.",
       {0x00, 0x11}},
      {"Unable to read connection close frame type.",
       {kVarInt62TwoBytes + 0x12, 0x34 }},
      {"Unable to read connection close error details.",
       {
         // error details length
         kVarInt62OneByte + 0x0d,
         // error details
         'b',  'e',  'c',  'a',
         'u',  's',  'e',  ' ',
         'I',  ' ',  'c',  'a',
         'n'}
      }
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.transport_version() == QUIC_VERSION_99
          ? packet99
          : (framer_.transport_version() > QUIC_VERSION_43
                 ? packet44
                 : (framer_.transport_version() != QUIC_VERSION_35 ? packet39
                                                                   : packet));
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());

  EXPECT_EQ(0x11, visitor_.connection_close_frame_.error_code);
  EXPECT_EQ("because I can", visitor_.connection_close_frame_.error_details);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    EXPECT_EQ(0x1234u, visitor_.connection_close_frame_.frame_type);
  }

  ASSERT_EQ(0u, visitor_.ack_frames_.size());

  CheckFramingBoundaries(fragments, QUIC_INVALID_CONNECTION_CLOSE_DATA);
}

TEST_P(QuicFramerTest, ApplicationCloseFrame) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // This frame does not exist in versions other than 99.
    return;
  }

  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (application close frame)
      {"",
       {0x03}},
      // error code
      {"Unable to read application close error code.",
       {0x00, 0x11}},
      {"Unable to read application close error details.",
       {
         // error details length
         kVarInt62OneByte + 0x0d,
         // error details
         'b',  'e',  'c',  'a',
         'u',  's',  'e',  ' ',
         'I',  ' ',  'c',  'a',
         'n'}
      }
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());

  EXPECT_EQ(0x11, visitor_.application_close_frame_.error_code);
  EXPECT_EQ("because I can", visitor_.application_close_frame_.error_details);

  ASSERT_EQ(0u, visitor_.ack_frames_.size());

  CheckFramingBoundaries(packet99, QUIC_INVALID_APPLICATION_CLOSE_DATA);
}

TEST_P(QuicFramerTest, GoAwayFrame) {
  if (framer_.transport_version() == QUIC_VERSION_99) {
    // This frame is not supported in version 99.
    return;
  }
  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id)
      {"",
       {0x28}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x78, 0x56, 0x34, 0x12}},
      // frame type (go away frame)
      {"",
       {0x03}},
      // error code
      {"Unable to read go away error code.",
       {0x09, 0x00, 0x00, 0x00}},
      // stream id
      {"Unable to read last good stream id.",
       {0x04, 0x03, 0x02, 0x01}},
      {"Unable to read goaway reason.",
       {
         // error details length
         0x0d, 0x00,
         // error details
         'b',  'e',  'c',  'a',
         'u',  's',  'e',  ' ',
         'I',  ' ',  'c',  'a',
         'n'}
      }
  };

  PacketFragments packet39 = {
      // public flags (8 byte connection_id)
      {"",
       {0x28}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (go away frame)
      {"",
       {0x03}},
      // error code
      {"Unable to read go away error code.",
       {0x00, 0x00, 0x00, 0x09}},
      // stream id
      {"Unable to read last good stream id.",
       {0x01, 0x02, 0x03, 0x04}},
      // stream id
      {"Unable to read goaway reason.",
       {
         // error details length
         0x0, 0x0d,
         // error details
         'b',  'e',  'c',  'a',
         'u',  's',  'e',  ' ',
         'I',  ' ',  'c',  'a',
         'n'}
      }
  };

  PacketFragments packet44 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (go away frame)
      {"",
       {0x03}},
      // error code
      {"Unable to read go away error code.",
       {0x00, 0x00, 0x00, 0x09}},
      // stream id
      {"Unable to read last good stream id.",
       {0x01, 0x02, 0x03, 0x04}},
      // stream id
      {"Unable to read goaway reason.",
       {
         // error details length
         0x0, 0x0d,
         // error details
         'b',  'e',  'c',  'a',
         'u',  's',  'e',  ' ',
         'I',  ' ',  'c',  'a',
         'n'}
      }
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.transport_version() > QUIC_VERSION_43
          ? packet44
          : (framer_.transport_version() != QUIC_VERSION_35 ? packet39
                                                            : packet);
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(kStreamId, visitor_.goaway_frame_.last_good_stream_id);
  EXPECT_EQ(0x9, visitor_.goaway_frame_.error_code);
  EXPECT_EQ("because I can", visitor_.goaway_frame_.reason_phrase);

  CheckFramingBoundaries(fragments, QUIC_INVALID_GOAWAY_DATA);
}

TEST_P(QuicFramerTest, WindowUpdateFrame) {
  if (framer_.transport_version() == QUIC_VERSION_99) {
    // This frame is not in version 99, see MaxDataFrame and MaxStreamDataFrame
    // for Version 99 equivalents.
    return;
  }
  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id)
      {"",
       {0x28}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x78, 0x56, 0x34, 0x12}},
      // frame type (window update frame)
      {"",
       {0x04}},
      // stream id
      {"Unable to read stream_id.",
       {0x04, 0x03, 0x02, 0x01}},
      // byte offset
      {"Unable to read window byte_offset.",
       {0x54, 0x76, 0x10, 0x32,
        0xDC, 0xFE, 0x98, 0x3A}},
  };

  PacketFragments packet39 = {
      // public flags (8 byte connection_id)
      {"",
       {0x28}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (window update frame)
      {"",
       {0x04}},
      // stream id
      {"Unable to read stream_id.",
       {0x01, 0x02, 0x03, 0x04}},
      // byte offset
      {"Unable to read window byte_offset.",
       {0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
  };

  PacketFragments packet44 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (window update frame)
      {"",
       {0x04}},
      // stream id
      {"Unable to read stream_id.",
       {0x01, 0x02, 0x03, 0x04}},
      // byte offset
      {"Unable to read window byte_offset.",
       {0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
  };

  // clang-format on

  PacketFragments& fragments =
      framer_.transport_version() > QUIC_VERSION_43
          ? packet44
          : (framer_.transport_version() != QUIC_VERSION_35 ? packet39
                                                            : packet);
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(kStreamId, visitor_.window_update_frame_.stream_id);
  EXPECT_EQ(kStreamOffset, visitor_.window_update_frame_.byte_offset);

  CheckFramingBoundaries(fragments, QUIC_INVALID_WINDOW_UPDATE_DATA);
}

TEST_P(QuicFramerTest, MaxDataFrame) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // This frame is available only in version 99.
    return;
  }
  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (IETF max data frame)
      {"",
       {0x04}},
      // byte offset
      {"Can not read MAX_DATA byte-offset",
       {kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(0u, visitor_.window_update_frame_.stream_id);
  EXPECT_EQ(kStreamOffset, visitor_.window_update_frame_.byte_offset);

  CheckFramingBoundaries(packet99, QUIC_INVALID_MAX_DATA_FRAME_DATA);
}

TEST_P(QuicFramerTest, MaxStreamDataFrame) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // This frame available only in version 99.
    return;
  }
  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (ietf max stream data frame)
      {"",
       {0x05}},
      // stream id
      {"Can not read MAX_STREAM_DATA stream id",
       {kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04}},
      // byte offset
      {"Can not read MAX_STREAM_DATA byte-count",
       {kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
        0x32, 0x10, 0x76, 0x54}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(kStreamId, visitor_.window_update_frame_.stream_id);
  EXPECT_EQ(kStreamOffset, visitor_.window_update_frame_.byte_offset);

  CheckFramingBoundaries(packet99, QUIC_INVALID_MAX_STREAM_DATA_FRAME_DATA);
}

TEST_P(QuicFramerTest, BlockedFrame) {
  // clang-format off
  PacketFragments packet = {
      // public flags (8 byte connection_id)
      {"",
       {0x28}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x78, 0x56, 0x34, 0x12}},
      // frame type (blocked frame)
      {"",
       {0x05}},
      // stream id
      {"Unable to read stream_id.",
       {0x04, 0x03, 0x02, 0x01}},
  };

  PacketFragments packet39 = {
      // public flags (8 byte connection_id)
      {"",
       {0x28}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (blocked frame)
      {"",
       {0x05}},
      // stream id
      {"Unable to read stream_id.",
       {0x01, 0x02, 0x03, 0x04}},
  };

  PacketFragments packet44 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (blocked frame)
      {"",
       {0x05}},
      // stream id
      {"Unable to read stream_id.",
       {0x01, 0x02, 0x03, 0x04}},
  };

  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (IETF stream blocked frame)
      {"",
       {0x09}},
      // stream id
      {"Can not read stream blocked stream id.",
       {kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04}},
      // Offset
      {"Can not read stream blocked offset.",
       {kVarInt62EightBytes + 0x3a, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54}},
  };
  // clang-format on

  PacketFragments& fragments =
      framer_.transport_version() == QUIC_VERSION_99
          ? packet99
          : (framer_.transport_version() > QUIC_VERSION_43
                 ? packet44
                 : (framer_.transport_version() != QUIC_VERSION_35 ? packet39
                                                                   : packet));
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  if (framer_.transport_version() == QUIC_VERSION_99) {
    EXPECT_EQ(kStreamOffset, visitor_.blocked_frame_.offset);
  } else {
    EXPECT_EQ(0u, visitor_.blocked_frame_.offset);
  }
  EXPECT_EQ(kStreamId, visitor_.blocked_frame_.stream_id);

  if (framer_.transport_version() == QUIC_VERSION_99) {
    CheckFramingBoundaries(fragments, QUIC_INVALID_STREAM_BLOCKED_DATA);
  } else {
    CheckFramingBoundaries(fragments, QUIC_INVALID_BLOCKED_DATA);
  }
}

TEST_P(QuicFramerTest, PingFrame) {
  // clang-format off
  unsigned char packet[] = {
     // public flags (8 byte connection_id)
     0x28,
     // connection_id
     0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
     // packet number
     0x78, 0x56, 0x34, 0x12,

     // frame type (ping frame)
     0x07,
    };

  unsigned char packet39[] = {
     // public flags (8 byte connection_id)
     0x28,
     // connection_id
     0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
     // packet number
     0x12, 0x34, 0x56, 0x78,

     // frame type (ping frame)
     0x07,
    };

  unsigned char packet44[] = {
     // type (short header, 4 byte packet number)
     0x32,
     // connection_id
     0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
     // packet number
     0x12, 0x34, 0x56, 0x78,

     // frame type
     0x07,
    };
  // clang-format on

  QuicEncryptedPacket encrypted(
      AsChars(framer_.transport_version() > QUIC_VERSION_43
                  ? packet44
                  : (framer_.transport_version() == QUIC_VERSION_35
                         ? packet
                         : packet39)),
      framer_.transport_version() > QUIC_VERSION_43 ? QUIC_ARRAYSIZE(packet44)
                                                    : QUIC_ARRAYSIZE(packet),
      false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(1u, visitor_.ping_frames_.size());

  // No need to check the PING frame boundaries because it has no payload.
}

TEST_P(QuicFramerTest, MessageFrame) {
  if (framer_.transport_version() <= QUIC_VERSION_44) {
    return;
  }
  // clang-format off
  PacketFragments packet45 = {
       // type (short header, 4 byte packet number)
       {"",
        {0x32}},
       // connection_id
       {"",
        {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
       // packet number
       {"",
        {0x12, 0x34, 0x56, 0x78}},
       // message frame type.
       {"",
        { 0x21 }},
       // message length
       {"Unable to read message length",
        {0x07}},
       // message data
       {"Unable to read message data",
        {'m', 'e', 's', 's', 'a', 'g', 'e'}},
        // message frame no length.
        {"",
         { 0x20 }},
        // message data
        {{},
         {'m', 'e', 's', 's', 'a', 'g', 'e', '2'}},
   };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet45));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  ASSERT_EQ(2u, visitor_.message_frames_.size());
  EXPECT_EQ(7u, visitor_.message_frames_[0]->message_data.length());
  EXPECT_EQ(8u, visitor_.message_frames_[1]->message_data.length());

  CheckFramingBoundaries(packet45, QUIC_INVALID_MESSAGE_DATA);
}

TEST_P(QuicFramerTest, PublicResetPacketV33) {
  // clang-format off
  PacketFragments packet = {
      // public flags (public reset, 8 byte connection_id)
      {"",
       {0x0A}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      {"Unable to read reset message.",
       {
         // message tag (kPRST)
         'P', 'R', 'S', 'T',
         // num_entries (2) + padding
         0x02, 0x00, 0x00, 0x00,
         // tag kRNON
         'R', 'N', 'O', 'N',
         // end offset 8
         0x08, 0x00, 0x00, 0x00,
         // tag kRSEQ
         'R', 'S', 'E', 'Q',
         // end offset 16
         0x10, 0x00, 0x00, 0x00,
         // nonce proof
         0x89, 0x67, 0x45, 0x23,
         0x01, 0xEF, 0xCD, 0xAB,
         // rejected packet number
         0xBC, 0x9A, 0x78, 0x56,
         0x34, 0x12, 0x00, 0x00,
       }
      }
  };
  // clang-format on
  if (framer_.transport_version() > QUIC_VERSION_43) {
    return;
  }

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  ASSERT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.public_reset_packet_.get());
  EXPECT_EQ(kConnectionId, visitor_.public_reset_packet_->connection_id);
  EXPECT_EQ(kNonceProof, visitor_.public_reset_packet_->nonce_proof);
  EXPECT_EQ(
      IpAddressFamily::IP_UNSPEC,
      visitor_.public_reset_packet_->client_address.host().address_family());

  CheckFramingBoundaries(packet, QUIC_INVALID_PUBLIC_RST_PACKET);
}

TEST_P(QuicFramerTest, PublicResetPacket) {
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);

  // clang-format off
  PacketFragments packet = {
      // public flags (public reset, 8 byte connection_id)
      {"",
       {0x0E}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      {"Unable to read reset message.",
       {
         // message tag (kPRST)
         'P', 'R', 'S', 'T',
         // num_entries (2) + padding
         0x02, 0x00, 0x00, 0x00,
         // tag kRNON
         'R', 'N', 'O', 'N',
         // end offset 8
         0x08, 0x00, 0x00, 0x00,
         // tag kRSEQ
         'R', 'S', 'E', 'Q',
         // end offset 16
         0x10, 0x00, 0x00, 0x00,
         // nonce proof
         0x89, 0x67, 0x45, 0x23,
         0x01, 0xEF, 0xCD, 0xAB,
         // rejected packet number
         0xBC, 0x9A, 0x78, 0x56,
         0x34, 0x12, 0x00, 0x00,
       }
      }
  };
  // clang-format on

  if (framer_.transport_version() > QUIC_VERSION_43) {
    return;
  }

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  ASSERT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.public_reset_packet_.get());
  EXPECT_EQ(kConnectionId, visitor_.public_reset_packet_->connection_id);
  EXPECT_EQ(kNonceProof, visitor_.public_reset_packet_->nonce_proof);
  EXPECT_EQ(
      IpAddressFamily::IP_UNSPEC,
      visitor_.public_reset_packet_->client_address.host().address_family());

  CheckFramingBoundaries(packet, QUIC_INVALID_PUBLIC_RST_PACKET);
}

TEST_P(QuicFramerTest, PublicResetPacketWithTrailingJunk) {
  // clang-format off
  unsigned char packet[] = {
    // public flags (public reset, 8 byte connection_id)
    0x0A,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // message tag (kPRST)
    'P', 'R', 'S', 'T',
    // num_entries (2) + padding
    0x02, 0x00, 0x00, 0x00,
    // tag kRNON
    'R', 'N', 'O', 'N',
    // end offset 8
    0x08, 0x00, 0x00, 0x00,
    // tag kRSEQ
    'R', 'S', 'E', 'Q',
    // end offset 16
    0x10, 0x00, 0x00, 0x00,
    // nonce proof
    0x89, 0x67, 0x45, 0x23,
    0x01, 0xEF, 0xCD, 0xAB,
    // rejected packet number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12, 0x00, 0x00,
    // trailing junk
    'j', 'u', 'n', 'k',
  };
  // clang-format on
  if (framer_.transport_version() > QUIC_VERSION_43) {
    return;
  }

  QuicEncryptedPacket encrypted(AsChars(packet), QUIC_ARRAYSIZE(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  ASSERT_EQ(QUIC_INVALID_PUBLIC_RST_PACKET, framer_.error());
  EXPECT_EQ("Unable to read reset message.", framer_.detailed_error());
}

TEST_P(QuicFramerTest, PublicResetPacketWithClientAddress) {
  // clang-format off
  PacketFragments packet = {
      // public flags (public reset, 8 byte connection_id)
      {"",
       {0x0A}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      {"Unable to read reset message.",
       {
         // message tag (kPRST)
         'P', 'R', 'S', 'T',
         // num_entries (2) + padding
         0x03, 0x00, 0x00, 0x00,
         // tag kRNON
         'R', 'N', 'O', 'N',
         // end offset 8
         0x08, 0x00, 0x00, 0x00,
         // tag kRSEQ
         'R', 'S', 'E', 'Q',
         // end offset 16
         0x10, 0x00, 0x00, 0x00,
         // tag kCADR
         'C', 'A', 'D', 'R',
         // end offset 24
         0x18, 0x00, 0x00, 0x00,
         // nonce proof
         0x89, 0x67, 0x45, 0x23,
         0x01, 0xEF, 0xCD, 0xAB,
         // rejected packet number
         0xBC, 0x9A, 0x78, 0x56,
         0x34, 0x12, 0x00, 0x00,
         // client address: 4.31.198.44:443
         0x02, 0x00,
         0x04, 0x1F, 0xC6, 0x2C,
         0xBB, 0x01,
       }
      }
  };
  // clang-format on
  if (framer_.transport_version() > QUIC_VERSION_43) {
    return;
  }

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  ASSERT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.public_reset_packet_.get());
  EXPECT_EQ(kConnectionId, visitor_.public_reset_packet_->connection_id);
  EXPECT_EQ(kNonceProof, visitor_.public_reset_packet_->nonce_proof);
  EXPECT_EQ("4.31.198.44",
            visitor_.public_reset_packet_->client_address.host().ToString());
  EXPECT_EQ(443, visitor_.public_reset_packet_->client_address.port());

  CheckFramingBoundaries(packet, QUIC_INVALID_PUBLIC_RST_PACKET);
}

TEST_P(QuicFramerTest, IetfStatelessResetPacket) {
  // clang-format off
  unsigned char packet[] = {
      // type (short packet, 1 byte packet number)
      0x10,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // Random bytes
      0x01, 0x11, 0x02, 0x22, 0x03, 0x33, 0x04, 0x44,
      0x01, 0x11, 0x02, 0x22, 0x03, 0x33, 0x04, 0x44,
      0x01, 0x11, 0x02, 0x22, 0x03, 0x33, 0x04, 0x44,
      0x01, 0x11, 0x02, 0x22, 0x03, 0x33, 0x04, 0x44,
      // stateless reset token
      0xB5, 0x69, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  // clang-format on
  if (framer_.transport_version() <= QUIC_VERSION_43) {
    return;
  }
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  framer_.SetDecrypter(ENCRYPTION_NONE,
                       QuicMakeUnique<NullDecrypter>(Perspective::IS_CLIENT));
  decrypter_ = new test::TestDecrypter();
  framer_.SetAlternativeDecrypter(
      ENCRYPTION_INITIAL, std::unique_ptr<QuicDecrypter>(decrypter_), false);
  // This packet cannot be decrypted because diversification nonce is missing.
  QuicEncryptedPacket encrypted(AsChars(packet), QUIC_ARRAYSIZE(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  ASSERT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.stateless_reset_packet_.get());
  EXPECT_EQ(kTestStatelessResetToken,
            visitor_.stateless_reset_packet_->stateless_reset_token);
}

TEST_P(QuicFramerTest, IetfStatelessResetPacketInvalidStatelessResetToken) {
  // clang-format off
  unsigned char packet[] = {
      // type (short packet, 1 byte packet number)
      0x10,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // stateless reset token
      0xB6, 0x69, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  // clang-format on
  if (framer_.transport_version() <= QUIC_VERSION_43) {
    return;
  }
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  framer_.SetDecrypter(ENCRYPTION_NONE,
                       QuicMakeUnique<NullDecrypter>(Perspective::IS_CLIENT));
  decrypter_ = new test::TestDecrypter();
  framer_.SetAlternativeDecrypter(
      ENCRYPTION_INITIAL, std::unique_ptr<QuicDecrypter>(decrypter_), false);
  // This packet cannot be decrypted because diversification nonce is missing.
  QuicEncryptedPacket encrypted(AsChars(packet), QUIC_ARRAYSIZE(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_DECRYPTION_FAILURE, framer_.error());
  ASSERT_FALSE(visitor_.stateless_reset_packet_);
}

TEST_P(QuicFramerTest, VersionNegotiationPacket) {
  // clang-format off
  PacketFragments packet = {
      // public flags (version, 8 byte connection_id)
      {"",
       {0x29}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // version tag
      {"Unable to read supported version in negotiation.",
       {QUIC_VERSION_BYTES,
        'Q', '2', '.', '0'}},
  };

  PacketFragments packet44 = {
      // type (long header)
      {"",
       {0x8F}},
             // version tag
      {"",
       {0x00, 0x00, 0x00, 0x00}},
      {"",
       {0x05}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // Supported versions
      {"Unable to read supported version in negotiation.",
       {QUIC_VERSION_BYTES,
        'Q', '2', '.', '0'}},
  };
  // clang-format on

  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);

  PacketFragments& fragments =
      framer_.transport_version() > QUIC_VERSION_43 ? packet44 : packet;
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(fragments));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  ASSERT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.version_negotiation_packet_.get());
  EXPECT_EQ(2u, visitor_.version_negotiation_packet_->versions.size());
  EXPECT_EQ(GetParam(), visitor_.version_negotiation_packet_->versions[0]);

  // Remove the last version from the packet so that every truncated
  // version of the packet is invalid, otherwise checking boundaries
  // is annoyingly complicated.
  for (size_t i = 0; i < 4; ++i) {
    fragments.back().fragment.pop_back();
  }
  CheckFramingBoundaries(fragments, QUIC_INVALID_VERSION_NEGOTIATION_PACKET);
}

TEST_P(QuicFramerTest, OldVersionNegotiationPacket) {
  // clang-format off
  PacketFragments packet = {
      // public flags (version, 8 byte connection_id)
      {"",
       {0x2D}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // version tag
      {"Unable to read supported version in negotiation.",
       {QUIC_VERSION_BYTES,
        'Q', '2', '.', '0'}},
  };
  // clang-format on

  if (framer_.transport_version() > QUIC_VERSION_43) {
    return;
  }

  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));
  ASSERT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.version_negotiation_packet_.get());
  EXPECT_EQ(2u, visitor_.version_negotiation_packet_->versions.size());
  EXPECT_EQ(GetParam(), visitor_.version_negotiation_packet_->versions[0]);

  // Remove the last version from the packet so that every truncated
  // version of the packet is invalid, otherwise checking boundaries
  // is annoyingly complicated.
  for (size_t i = 0; i < 4; ++i) {
    packet.back().fragment.pop_back();
  }
  CheckFramingBoundaries(packet, QUIC_INVALID_VERSION_NEGOTIATION_PACKET);
}

TEST_P(QuicFramerTest, BuildPaddingFramePacket) {
  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicFrames frames = {QuicFrame(QuicPaddingFrame())};

  // clang-format off
  unsigned char packet[kMaxPacketSize] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78, 0x56, 0x34, 0x12,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet39[kMaxPacketSize] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet44[kMaxPacketSize] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };
  // clang-format on

  unsigned char* p = packet;
  if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
  } else if (framer_.transport_version() != QUIC_VERSION_35) {
    p = packet39;
  }

  uint64_t header_size = GetPacketHeaderSize(
      framer_.transport_version(), PACKET_8BYTE_CONNECTION_ID,
      PACKET_0BYTE_CONNECTION_ID, !kIncludeVersion,
      !kIncludeDiversificationNonce, PACKET_4BYTE_PACKET_NUMBER);
  memset(p + header_size + 1, 0x00, kMaxPacketSize - header_size - 1);

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(p),
      framer_.transport_version() > QUIC_VERSION_43 ? QUIC_ARRAYSIZE(packet44)
                                                    : QUIC_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, BuildStreamFramePacketWithNewPaddingFrame) {
  if (framer_.transport_version() == QUIC_VERSION_35) {
    return;
  }
  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;
  QuicStreamFrame stream_frame(kStreamId, true, kStreamOffset,
                               QuicStringPiece("hello world!"));
  QuicPaddingFrame padding_frame(2);
  QuicFrames frames = {QuicFrame(padding_frame), QuicFrame(stream_frame),
                       QuicFrame(padding_frame)};

  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78, 0x56, 0x34, 0x12,

    // paddings
    0x00, 0x00,
    // frame type (stream frame with fin)
    0xFF,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0x3A,
    // data length
    0x0c, 0x00,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
    // paddings
    0x00, 0x00,
  };

  unsigned char packet39[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // paddings
    0x00, 0x00,
    // frame type (stream frame with fin)
    0xFF,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // offset
    0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data length
    0x00, 0x0c,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
    // paddings
    0x00, 0x00,
  };

  unsigned char packet44[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // paddings
    0x00, 0x00,
    // frame type (stream frame with fin)
    0xFF,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // offset
    0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data length
    0x00, 0x0c,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
    // paddings
    0x00, 0x00,
  };

  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // paddings
    0x00, 0x00,
    // frame type (stream frame with FIN, LEN, and OFFSET bits set)
    0x10 | 0x01 | 0x02 | 0x04,
    // stream id
    kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04,
    // offset
    kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data length
    kVarInt62OneByte + 0x0c,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
    // paddings
    0x00, 0x00,
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    p_size = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  } else if (framer_.transport_version() != QUIC_VERSION_35) {
    p = packet39;
  }
  QuicEncryptedPacket encrypted(AsChars(p), p_size, false);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, Build4ByteSequenceNumberPaddingFramePacket) {
  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number_length = PACKET_4BYTE_PACKET_NUMBER;
  header.packet_number = kPacketNumber;

  QuicFrames frames = {QuicFrame(QuicPaddingFrame())};

  // clang-format off
  unsigned char packet[kMaxPacketSize] = {
    // public flags (8 byte connection_id and 4 byte packet number)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78, 0x56, 0x34, 0x12,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet39[kMaxPacketSize] = {
    // public flags (8 byte connection_id and 4 byte packet number)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet44[kMaxPacketSize] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };
  // clang-format on

  unsigned char* p = packet;
  if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
  } else if (framer_.transport_version() != QUIC_VERSION_35) {
    p = packet39;
  }

  uint64_t header_size = GetPacketHeaderSize(
      framer_.transport_version(), PACKET_8BYTE_CONNECTION_ID,
      PACKET_0BYTE_CONNECTION_ID, !kIncludeVersion,
      !kIncludeDiversificationNonce, PACKET_4BYTE_PACKET_NUMBER);
  memset(p + header_size + 1, 0x00, kMaxPacketSize - header_size - 1);

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(p),
      framer_.transport_version() > QUIC_VERSION_43 ? QUIC_ARRAYSIZE(packet44)
                                                    : QUIC_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, Build2ByteSequenceNumberPaddingFramePacket) {
  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number_length = PACKET_2BYTE_PACKET_NUMBER;
  header.packet_number = kPacketNumber;

  QuicFrames frames = {QuicFrame(QuicPaddingFrame())};

  // clang-format off
  unsigned char packet[kMaxPacketSize] = {
    // public flags (8 byte connection_id and 2 byte packet number)
    0x18,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78, 0x56,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet39[kMaxPacketSize] = {
    // public flags (8 byte connection_id and 2 byte packet number)
    0x18,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x56, 0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet44[kMaxPacketSize] = {
    // type (short header, 2 byte packet number)
    0x31,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x56, 0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };
  // clang-format on

  unsigned char* p = packet;
  if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
  } else if (framer_.transport_version() != QUIC_VERSION_35) {
    p = packet39;
  }

  uint64_t header_size = GetPacketHeaderSize(
      framer_.transport_version(), PACKET_8BYTE_CONNECTION_ID,
      PACKET_0BYTE_CONNECTION_ID, !kIncludeVersion,
      !kIncludeDiversificationNonce, PACKET_2BYTE_PACKET_NUMBER);
  memset(p + header_size + 1, 0x00, kMaxPacketSize - header_size - 1);

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(p),
      framer_.transport_version() > QUIC_VERSION_43 ? QUIC_ARRAYSIZE(packet44)
                                                    : QUIC_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, Build1ByteSequenceNumberPaddingFramePacket) {
  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number_length = PACKET_1BYTE_PACKET_NUMBER;
  header.packet_number = kPacketNumber;

  QuicFrames frames = {QuicFrame(QuicPaddingFrame())};

  // clang-format off
  unsigned char packet[kMaxPacketSize] = {
    // public flags (8 byte connection_id and 1 byte packet number)
    0x08,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet44[kMaxPacketSize] = {
    // type (short header, 1 byte packet number)
    0x30,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };
  // clang-format on

  unsigned char* p = packet;
  if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
  }

  uint64_t header_size = GetPacketHeaderSize(
      framer_.transport_version(), PACKET_8BYTE_CONNECTION_ID,
      PACKET_0BYTE_CONNECTION_ID, !kIncludeVersion,
      !kIncludeDiversificationNonce, PACKET_1BYTE_PACKET_NUMBER);
  memset(p + header_size + 1, 0x00, kMaxPacketSize - header_size - 1);

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(p),
      framer_.transport_version() > QUIC_VERSION_43 ? QUIC_ARRAYSIZE(packet44)
                                                    : QUIC_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, BuildStreamFramePacket) {
  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicStreamFrame stream_frame(kStreamId, true, kStreamOffset,
                               QuicStringPiece("hello world!"));

  QuicFrames frames = {QuicFrame(stream_frame)};

  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78, 0x56, 0x34, 0x12,

    // frame type (stream frame with fin and no length)
    0xDF,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0x3A,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  unsigned char packet39[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (stream frame with fin and no length)
    0xDF,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // offset
    0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  unsigned char packet44[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (stream frame with fin and no length)
    0xDF,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // offset
    0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (stream frame with FIN and OFFSET, no length)
    0x10 | 0x01 | 0x04,
    // stream id
    kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04,
    // offset
    kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    p_size = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  } else if (framer_.transport_version() != QUIC_VERSION_35) {
    p = packet39;
  }
  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildStreamFramePacketWithVersionFlag) {
  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = true;
  if (framer_.transport_version() > QUIC_VERSION_43) {
    header.long_packet_type = ZERO_RTT_PROTECTED;
  }
  header.packet_number = kPacketNumber;

  QuicStreamFrame stream_frame(kStreamId, true, kStreamOffset,
                               QuicStringPiece("hello world!"));
  QuicFrames frames = {QuicFrame(stream_frame)};

  // clang-format off
  unsigned char packet[] = {
      // public flags (version, 8 byte connection_id)
      0x2D,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // version tag
      QUIC_VERSION_BYTES,
      // packet number
      0x78, 0x56, 0x34, 0x12,

      // frame type (stream frame with fin and no length)
      0xDF,
      // stream id
      0x04, 0x03, 0x02, 0x01,
      // offset
      0x54, 0x76, 0x10, 0x32, 0xDC, 0xFE, 0x98, 0x3A,
      // data
      'h',  'e',  'l',  'l',  'o',  ' ',  'w',  'o',  'r', 'l', 'd', '!',
  };

  unsigned char packet39[] = {
      // public flags (version, 8 byte connection_id)
      0x2D,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // version tag
      QUIC_VERSION_BYTES,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (stream frame with fin and no length)
      0xDF,
      // stream id
      0x01, 0x02, 0x03, 0x04,
      // offset
      0x3A, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54,
      // data
      'h',  'e',  'l',  'l',  'o',  ' ',  'w',  'o',  'r', 'l', 'd', '!',
  };

  unsigned char packet44[] = {
      // type (long header with packet type ZERO_RTT_PROTECTED)
      0xFC,
      // version tag
      QUIC_VERSION_BYTES,
      // connection_id length
      0x50,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (stream frame with fin and no length)
      0xDF,
      // stream id
      0x01, 0x02, 0x03, 0x04,
      // offset
      0x3A, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54,
      // data
      'h',  'e',  'l',  'l',  'o',  ' ',  'w',  'o',  'r', 'l', 'd', '!',
  };

  unsigned char packet99[] = {
      // type (long header with packet type ZERO_RTT_PROTECTED)
      0xFC,
      // version tag
      QUIC_VERSION_BYTES,
      // connection_id length
      0x50,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (stream frame with fin and offset, no length)
      0x10 | 0x01 | 0x04,
      // stream id
      kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04,
      // offset
      kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54,
      // data
      'h',  'e',  'l',  'l',  'o',  ' ',  'w',  'o',  'r', 'l', 'd', '!',
  };
  // clang-format on

  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    p_size = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  } else if (framer_.transport_version() != QUIC_VERSION_35) {
    p = packet39;
  }
  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildVersionNegotiationPacket) {
  // clang-format off
  unsigned char packet[] = {
      // public flags (version, 8 byte connection_id)
      0x0D,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // version tag
      QUIC_VERSION_BYTES,
  };
  unsigned char packet44[] = {
      // type (long header)
      0x80,
      // version tag
      0x00, 0x00, 0x00, 0x00,
      // connection_id length
      0x05,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // version tag
      QUIC_VERSION_BYTES,
  };
  // clang-format on
  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  }

  QuicConnectionId connection_id = kConnectionId;
  std::unique_ptr<QuicEncryptedPacket> data(
      framer_.BuildVersionNegotiationPacket(
          connection_id, framer_.transport_version() > QUIC_VERSION_43,
          SupportedVersions(GetParam())));
  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildAckFramePacketOneAckBlock) {
  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  // Use kSmallLargestObserved to make this test finished in a short time.
  QuicAckFrame ack_frame = InitAckFrame(kSmallLargestObserved);
  ack_frame.ack_delay_time = QuicTime::Delta::Zero();

  QuicFrames frames = {QuicFrame(&ack_frame)};

  // clang-format off
  unsigned char packet[] = {
      // public flags (8 byte connection_id)
      0x28,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x78, 0x56, 0x34, 0x12,

      // frame type (ack frame)
      // (no ack blocks, 2 byte largest observed, 2 byte block length)
      0x45,
      // largest acked
      0x34, 0x12,
      // Zero delta time.
      0x00, 0x00,
      // first ack block length.
      0x34, 0x12,
      // num timestamps.
      0x00,
  };

  unsigned char packet39[] = {
      // public flags (8 byte connection_id)
      0x28,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (ack frame)
      // (no ack blocks, 2 byte largest observed, 2 byte block length)
      0x45,
      // largest acked
      0x12, 0x34,
      // Zero delta time.
      0x00, 0x00,
      // first ack block length.
      0x12, 0x34,
      // num timestamps.
      0x00,
  };

  unsigned char packet44[] = {
      // type (short header, 4 byte packet number)
      0x32,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (ack frame)
      // (no ack blocks, 2 byte largest observed, 2 byte block length)
      0x45,
      // largest acked
      0x12, 0x34,
      // Zero delta time.
      0x00, 0x00,
      // first ack block length.
      0x12, 0x34,
      // num timestamps.
      0x00,
  };

  unsigned char packet99[] = {
      // type (short header, 4 byte packet number)
      0x32,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (ack frame)
      0x1a,
      // largest acked
      kVarInt62TwoBytes + 0x12, 0x34,
      // Zero delta time.
      kVarInt62OneByte + 0x00,
      // Number of additional ack blocks.
      kVarInt62OneByte + 0x00,
      // first ack block length.
      kVarInt62TwoBytes + 0x12, 0x33,
  };
  // clang-format on
  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    p_size = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  } else if (framer_.transport_version() != QUIC_VERSION_35) {
    p = packet39;
  }

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);
  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildAckFramePacketOneAckBlockMaxLength) {
  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicAckFrame ack_frame = InitAckFrame(kPacketNumber);
  ack_frame.ack_delay_time = QuicTime::Delta::Zero();

  QuicFrames frames = {QuicFrame(&ack_frame)};

  // clang-format off
  unsigned char packet[] = {
      // public flags (8 byte connection_id)
      0x28,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x78, 0x56, 0x34, 0x12,

      // frame type (ack frame)
      // (no ack blocks, 4 byte largest observed, 4 byte block length)
      0x4A,
      // largest acked
      0x78, 0x56, 0x34, 0x12,
      // Zero delta time.
      0x00, 0x00,
      // first ack block length.
      0x78, 0x56, 0x34, 0x12,
      // num timestamps.
      0x00,
  };

  unsigned char packet39[] = {
      // public flags (8 byte connection_id)
      0x28,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (ack frame)
      // (no ack blocks, 4 byte largest observed, 4 byte block length)
      0x4A,
      // largest acked
      0x12, 0x34, 0x56, 0x78,
      // Zero delta time.
      0x00, 0x00,
      // first ack block length.
      0x12, 0x34, 0x56, 0x78,
      // num timestamps.
      0x00,
  };

  unsigned char packet44[] = {
      // type (short header, 4 byte packet number)
      0x32,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (ack frame)
      // (no ack blocks, 4 byte largest observed, 4 byte block length)
      0x4A,
      // largest acked
      0x12, 0x34, 0x56, 0x78,
      // Zero delta time.
      0x00, 0x00,
      // first ack block length.
      0x12, 0x34, 0x56, 0x78,
      // num timestamps.
      0x00,
  };


  unsigned char packet99[] = {
      // type (short header, 4 byte packet number)
      0x32,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (ack frame)
      0x1a,
      // largest acked
      kVarInt62FourBytes + 0x12, 0x34, 0x56, 0x78,
      // Zero delta time.
      kVarInt62OneByte + 0x00,
      // Nr. of additional ack blocks
      kVarInt62OneByte + 0x00,
      // first ack block length.
      kVarInt62FourBytes + 0x12, 0x34, 0x56, 0x77,
  };
  // clang-format on
  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    p_size = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  } else if (framer_.transport_version() != QUIC_VERSION_35) {
    p = packet39;
  }

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);
  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildAckFramePacketMultipleAckBlocks) {
  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  // Use kSmallLargestObserved to make this test finished in a short time.
  QuicAckFrame ack_frame =
      InitAckFrame({{1, 5},
                    {10, 500},
                    {900, kSmallMissingPacket},
                    {kSmallMissingPacket + 1, kSmallLargestObserved + 1}});
  ack_frame.ack_delay_time = QuicTime::Delta::Zero();

  QuicFrames frames = {QuicFrame(&ack_frame)};

  // clang-format off
  unsigned char packet[] = {
      // public flags (8 byte connection_id)
      0x28,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x78, 0x56, 0x34, 0x12,

      // frame type (ack frame)
      // (has ack blocks, 2 byte largest observed, 2 byte block length)
      0x65,
      // largest acked
      0x34, 0x12,
      // Zero delta time.
      0x00, 0x00,
      // num ack blocks ranges.
      0x04,
      // first ack block length.
      0x01, 0x00,
      // gap to next block.
      0x01,
      // ack block length.
      0xaf, 0x0e,
      // gap to next block.
      0xff,
      // ack block length.
      0x00, 0x00,
      // gap to next block.
      0x91,
      // ack block length.
      0xea, 0x01,
      // gap to next block.
      0x05,
      // ack block length.
      0x04, 0x00,
      // num timestamps.
      0x00,
  };

  unsigned char packet39[] = {
      // public flags (8 byte connection_id)
      0x28,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (ack frame)
      // (has ack blocks, 2 byte largest observed, 2 byte block length)
      0x65,
      // largest acked
      0x12, 0x34,
      // Zero delta time.
      0x00, 0x00,
      // num ack blocks ranges.
      0x04,
      // first ack block length.
      0x00, 0x01,
      // gap to next block.
      0x01,
      // ack block length.
      0x0e, 0xaf,
      // gap to next block.
      0xff,
      // ack block length.
      0x00, 0x00,
      // gap to next block.
      0x91,
      // ack block length.
      0x01, 0xea,
      // gap to next block.
      0x05,
      // ack block length.
      0x00, 0x04,
      // num timestamps.
      0x00,
  };

  unsigned char packet44[] = {
      // type (short header, 4 byte packet number)
      0x32,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (ack frame)
      // (has ack blocks, 2 byte largest observed, 2 byte block length)
      0x65,
      // largest acked
      0x12, 0x34,
      // Zero delta time.
      0x00, 0x00,
      // num ack blocks ranges.
      0x04,
      // first ack block length.
      0x00, 0x01,
      // gap to next block.
      0x01,
      // ack block length.
      0x0e, 0xaf,
      // gap to next block.
      0xff,
      // ack block length.
      0x00, 0x00,
      // gap to next block.
      0x91,
      // ack block length.
      0x01, 0xea,
      // gap to next block.
      0x05,
      // ack block length.
      0x00, 0x04,
      // num timestamps.
      0x00,
  };

  unsigned char packet99[] = {
      // type (short header, 4 byte packet number)
      0x32,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,

      // frame type (ack frame)
      0x1a,
      // largest acked
      kVarInt62TwoBytes + 0x12, 0x34,
      // Zero delta time.
      kVarInt62OneByte + 0x00,
      // num additional ack blocks.
      kVarInt62OneByte + 0x03,
      // first ack block length.
      kVarInt62OneByte + 0x00,

      // gap to next block.
      kVarInt62OneByte + 0x00,
      // ack block length.
      kVarInt62TwoBytes + 0x0e, 0xae,

      // gap to next block.
      kVarInt62TwoBytes + 0x01, 0x8f,
      // ack block length.
      kVarInt62TwoBytes + 0x01, 0xe9,

      // gap to next block.
      kVarInt62OneByte + 0x04,
      // ack block length.
      kVarInt62OneByte + 0x03,
  };
  // clang-format on
  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    p_size = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  } else if (framer_.transport_version() != QUIC_VERSION_35) {
    p = packet39;
  }

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildAckFramePacketMaxAckBlocks) {
  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  // Use kSmallLargestObservedto make this test finished in a short time.
  QuicAckFrame ack_frame;
  ack_frame.largest_acked = kSmallLargestObserved;
  ack_frame.ack_delay_time = QuicTime::Delta::Zero();
  // 300 ack blocks.
  for (size_t i = 2; i < 2 * 300; i += 2) {
    ack_frame.packets.Add(i);
  }
  ack_frame.packets.AddRange(600, kSmallLargestObserved + 1);

  QuicFrames frames = {QuicFrame(&ack_frame)};

  // clang-format off
  unsigned char packet[] = {
      // public flags (8 byte connection_id)
      0x28,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x78, 0x56, 0x34, 0x12,
      // frame type (ack frame)
      // (has ack blocks, 2 byte largest observed, 2 byte block length)
      0x65,
      // largest acked
      0x34, 0x12,
      // Zero delta time.
      0x00, 0x00,
      // num ack blocks ranges.
      0xff,
      // first ack block length.
      0xdd, 0x0f,
      // 255 = 4 * 63 + 3
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,

      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,

      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,

      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,

      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,

      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,

      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
      // num timestamps.
      0x00,
  };

  unsigned char packet39[] = {
      // public flags (8 byte connection_id)
      0x28,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,
      // frame type (ack frame)
      // (has ack blocks, 2 byte largest observed, 2 byte block length)
      0x65,
      // largest acked
      0x12, 0x34,
      // Zero delta time.
      0x00, 0x00,
      // num ack blocks ranges.
      0xff,
      // first ack block length.
      0x0f, 0xdd,
      // 255 = 4 * 63 + 3
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      // num timestamps.
      0x00,
  };

  unsigned char packet44[] = {
      // type (short header, 4 byte packet number)
      0x32,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,
      // frame type (ack frame)
      // (has ack blocks, 2 byte largest observed, 2 byte block length)
      0x65,
      // largest acked
      0x12, 0x34,
      // Zero delta time.
      0x00, 0x00,
      // num ack blocks ranges.
      0xff,
      // first ack block length.
      0x0f, 0xdd,
      // 255 = 4 * 63 + 3
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,

      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
      // num timestamps.
      0x00,
  };

  unsigned char packet99[] = {
      // type (short header, 4 byte packet number)
      0x32,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // packet number
      0x12, 0x34, 0x56, 0x78,
      // frame type (ack frame)
      0x1a,
      // largest acked
      kVarInt62TwoBytes + 0x12, 0x34,
      // Zero delta time.
      kVarInt62OneByte + 0x00,
      // num ack blocks ranges.
      kVarInt62TwoBytes + 0x01, 0x2b,
      // first ack block length.
      kVarInt62TwoBytes + 0x0f, 0xdc,
      // 255 added blocks of gap_size == 1, ack_size == 1
#define V99AddedBLOCK kVarInt62OneByte + 0x00, kVarInt62OneByte + 0x00
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,

      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,

      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,
      V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK, V99AddedBLOCK,

#undef V99AddedBLOCK
  };
  // clang-format on
  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    p_size = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  } else if (framer_.transport_version() != QUIC_VERSION_35) {
    p = packet39;
  }

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildNewStopWaitingPacket) {
  if (version_.transport_version > QUIC_VERSION_43) {
    return;
  }
  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicStopWaitingFrame stop_waiting_frame;
  stop_waiting_frame.least_unacked = kLeastUnacked;

  QuicFrames frames = {QuicFrame(&stop_waiting_frame)};

  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78, 0x56, 0x34, 0x12,

    // frame type (stop waiting frame)
    0x06,
    // least packet number awaiting an ack, delta from packet number.
    0x08, 0x00, 0x00, 0x00,
  };

  unsigned char packet39[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (stop waiting frame)
    0x06,
    // least packet number awaiting an ack, delta from packet number.
    0x00, 0x00, 0x00, 0x08,
  };

  // clang-format on

  unsigned char* p = packet;
  if (framer_.transport_version() != QUIC_VERSION_35) {
    p = packet39;
  }

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(p),
      framer_.transport_version() != QUIC_VERSION_35 ? QUIC_ARRAYSIZE(packet39)
                                                     : QUIC_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, BuildRstFramePacketQuic) {
  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicRstStreamFrame rst_frame;
  rst_frame.stream_id = kStreamId;
  if (framer_.transport_version() == QUIC_VERSION_99) {
    rst_frame.ietf_error_code = 0x01;
  } else {
    rst_frame.error_code = static_cast<QuicRstStreamErrorCode>(0x05060708);
  }
  rst_frame.byte_offset = 0x0807060504030201;

  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78, 0x56, 0x34, 0x12,

    // frame type (rst stream frame)
    0x01,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // sent byte offset
    0x01, 0x02, 0x03, 0x04,
    0x05, 0x06, 0x07, 0x08,
    // error code
    0x08, 0x07, 0x06, 0x05,
  };

  unsigned char packet39[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (rst stream frame)
    0x01,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // sent byte offset
    0x08, 0x07, 0x06, 0x05,
    0x04, 0x03, 0x02, 0x01,
    // error code
    0x05, 0x06, 0x07, 0x08,
  };

  unsigned char packet44[] = {
    // type (short packet, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (rst stream frame)
    0x01,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // sent byte offset
    0x08, 0x07, 0x06, 0x05,
    0x04, 0x03, 0x02, 0x01,
    // error code
    0x05, 0x06, 0x07, 0x08,
  };

  unsigned char packet99[] = {
    // type (short packet, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (rst stream frame)
    0x01,
    // stream id
    kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04,
    // error code (not VarInt32 encoded)
    0x00, 0x01,
    // sent byte offset
    kVarInt62EightBytes + 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01
  };
  // clang-format on

  QuicFrames frames = {QuicFrame(&rst_frame)};

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    p_size = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  } else if (framer_.transport_version() != QUIC_VERSION_35) {
    p = packet39;
  }
  QuicEncryptedPacket encrypted(AsChars(p), p_size, false);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildCloseFramePacket) {
  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicConnectionCloseFrame close_frame;
  if (framer_.transport_version() == QUIC_VERSION_99) {
    close_frame.ietf_error_code =
        static_cast<QuicIetfTransportErrorCodes>(0x11);
    close_frame.frame_type = 0x05;
  } else {
    close_frame.error_code = static_cast<QuicErrorCode>(0x05060708);
  }
  close_frame.error_details = "because I can";

  QuicFrames frames = {QuicFrame(&close_frame)};

  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78, 0x56, 0x34, 0x12,

    // frame type (connection close frame)
    0x02,
    // error code
    0x08, 0x07, 0x06, 0x05,
    // error details length
    0x0d, 0x00,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',
  };

  unsigned char packet39[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (connection close frame)
    0x02,
    // error code
    0x05, 0x06, 0x07, 0x08,
    // error details length
    0x00, 0x0d,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',
  };

  unsigned char packet44[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (connection close frame)
    0x02,
    // error code
    0x05, 0x06, 0x07, 0x08,
    // error details length
    0x00, 0x0d,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',
  };

  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (connection close frame)
    0x02,
    // error code
    0x00, 0x11,
    // Frame type within the CONNECTION_CLOSE frame
    kVarInt62OneByte + 0x05,
    // error details length
    kVarInt62OneByte + 0x0d,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',
  };
  // clang-format on

  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    p_size = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  } else if (framer_.transport_version() != QUIC_VERSION_35) {
    p = packet39;
  }

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildTruncatedCloseFramePacket) {
  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicConnectionCloseFrame close_frame;
  if (framer_.transport_version() == QUIC_VERSION_99) {
    close_frame.ietf_error_code = PROTOCOL_VIOLATION;  // value is 0x0a
    EXPECT_EQ(0u, close_frame.frame_type);
  } else {
    close_frame.error_code = static_cast<QuicErrorCode>(0x05060708);
  }
  close_frame.error_details = QuicString(2048, 'A');
  QuicFrames frames = {QuicFrame(&close_frame)};

  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78, 0x56, 0x34, 0x12,

    // frame type (connection close frame)
    0x02,
    // error code
    0x08, 0x07, 0x06, 0x05,
    // error details length
    0x00, 0x01,
    // error details (truncated to 256 bytes)
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
  };

  unsigned char packet39[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (connection close frame)
    0x02,
    // error code
    0x05, 0x06, 0x07, 0x08,
    // error details length
    0x01, 0x00,
    // error details (truncated to 256 bytes)
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
  };

  unsigned char packet44[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (connection close frame)
    0x02,
    // error code
    0x05, 0x06, 0x07, 0x08,
    // error details length
    0x01, 0x00,
    // error details (truncated to 256 bytes)
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
  };

  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (connection close frame)
    0x02,
    // error code
    0x00, 0x0a,
    // Frame type within the CONNECTION_CLOSE frame
    kVarInt62OneByte + 0x00,
    // error details length
    kVarInt62TwoBytes + 0x01, 0x00,
    // error details (truncated to 256 bytes)
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
  };
  // clang-format on

  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    p_size = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  } else if (framer_.transport_version() != QUIC_VERSION_35) {
    p = packet39;
  }

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildApplicationCloseFramePacket) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // Versions other than 99 do not have ApplicationClose
    return;
  }
  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicApplicationCloseFrame app_close_frame;
  app_close_frame.error_code = static_cast<QuicErrorCode>(0x11);
  app_close_frame.error_details = "because I can";

  QuicFrames frames = {QuicFrame(&app_close_frame)};

  // clang-format off

  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (application close frame)
    0x03,
    // error code
    0x00, 0x11,
    // error details length
    kVarInt62OneByte + 0x0d,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet99),
                                      QUIC_ARRAYSIZE(packet99));
}

TEST_P(QuicFramerTest, BuildTruncatedApplicationCloseFramePacket) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // Versions other than 99 do not have this frame.
    return;
  }
  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicApplicationCloseFrame app_close_frame;
  app_close_frame.error_code = static_cast<QuicErrorCode>(0x11);
  app_close_frame.error_details = QuicString(2048, 'A');

  QuicFrames frames = {QuicFrame(&app_close_frame)};

  // clang-format off
  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type
    0x03,
    // error code
    0x00, 0x11,
    // error details length
    kVarInt62TwoBytes + 0x01, 0x00,
    // error details (truncated to 256 bytes)
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet99),
                                      QUIC_ARRAYSIZE(packet99));
}

TEST_P(QuicFramerTest, BuildGoAwayPacket) {
  if (framer_.transport_version() == QUIC_VERSION_99) {
    // This frame type is not supported in version 99.
    return;
  }
  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicGoAwayFrame goaway_frame;
  goaway_frame.error_code = static_cast<QuicErrorCode>(0x05060708);
  goaway_frame.last_good_stream_id = kStreamId;
  goaway_frame.reason_phrase = "because I can";

  QuicFrames frames = {QuicFrame(&goaway_frame)};

  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78, 0x56, 0x34, 0x12,

    // frame type (go away frame)
    0x03,
    // error code
    0x08, 0x07, 0x06, 0x05,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // error details length
    0x0d, 0x00,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',
  };

  unsigned char packet39[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (go away frame)
    0x03,
    // error code
    0x05, 0x06, 0x07, 0x08,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // error details length
    0x00, 0x0d,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',
  };

  unsigned char packet44[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (go away frame)
    0x03,
    // error code
    0x05, 0x06, 0x07, 0x08,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // error details length
    0x00, 0x0d,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',
  };

  // clang-format on

  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  } else if (framer_.transport_version() != QUIC_VERSION_35) {
    p = packet39;
  }

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildTruncatedGoAwayPacket) {
  if (framer_.transport_version() == QUIC_VERSION_99) {
    // This frame type is not supported in version 99.
    return;
  }
  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicGoAwayFrame goaway_frame;
  goaway_frame.error_code = static_cast<QuicErrorCode>(0x05060708);
  goaway_frame.last_good_stream_id = kStreamId;
  goaway_frame.reason_phrase = QuicString(2048, 'A');

  QuicFrames frames = {QuicFrame(&goaway_frame)};

  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78, 0x56, 0x34, 0x12,

    // frame type (go away frame)
    0x03,
    // error code
    0x08, 0x07, 0x06, 0x05,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // error details length
    0x00, 0x01,
    // error details (truncated to 256 bytes)
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
  };

  unsigned char packet39[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (go away frame)
    0x03,
    // error code
    0x05, 0x06, 0x07, 0x08,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // error details length
    0x01, 0x00,
    // error details (truncated to 256 bytes)
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
  };

  unsigned char packet44[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (go away frame)
    0x03,
    // error code
    0x05, 0x06, 0x07, 0x08,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // error details length
    0x01, 0x00,
    // error details (truncated to 256 bytes)
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
    'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
  };
  // clang-format on

  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  } else if (framer_.transport_version() != QUIC_VERSION_35) {
    p = packet39;
  }

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildWindowUpdatePacket) {
  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicWindowUpdateFrame window_update_frame;
  window_update_frame.stream_id = kStreamId;
  window_update_frame.byte_offset = 0x1122334455667788;

  QuicFrames frames = {QuicFrame(&window_update_frame)};

  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78, 0x56, 0x34, 0x12,

    // frame type (window update frame)
    0x04,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // byte offset
    0x88, 0x77, 0x66, 0x55,
    0x44, 0x33, 0x22, 0x11,
  };

  unsigned char packet39[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (window update frame)
    0x04,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // byte offset
    0x11, 0x22, 0x33, 0x44,
    0x55, 0x66, 0x77, 0x88,
  };

  unsigned char packet44[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (window update frame)
    0x04,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // byte offset
    0x11, 0x22, 0x33, 0x44,
    0x55, 0x66, 0x77, 0x88,
  };

  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (max stream data frame)
    0x05,
    // stream id
    kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04,
    // byte offset
    kVarInt62EightBytes + 0x11, 0x22, 0x33, 0x44,
    0x55, 0x66, 0x77, 0x88,
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    p_size = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  } else if (framer_.transport_version() != QUIC_VERSION_35) {
    p = packet39;
  }

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildMaxStreamDataPacket) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // This frame is available only in this version.
    return;
  }
  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicWindowUpdateFrame window_update_frame;
  window_update_frame.stream_id = kStreamId;
  window_update_frame.byte_offset = 0x1122334455667788;

  QuicFrames frames = {QuicFrame(&window_update_frame)};

  // clang-format off
  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (max stream data frame)
    0x05,
    // stream id
    kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04,
    // byte offset
    kVarInt62EightBytes + 0x11, 0x22, 0x33, 0x44,
    0x55, 0x66, 0x77, 0x88,
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet99),
                                      QUIC_ARRAYSIZE(packet99));
}

TEST_P(QuicFramerTest, BuildMaxDataPacket) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // This frame is available only in this version.
    return;
  }
  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicWindowUpdateFrame window_update_frame;
  window_update_frame.stream_id = 0;
  window_update_frame.byte_offset = 0x1122334455667788;

  QuicFrames frames = {QuicFrame(&window_update_frame)};

  // clang-format off
  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (max data frame)
    0x04,
    // byte offset
    kVarInt62EightBytes + 0x11, 0x22, 0x33, 0x44,
    0x55, 0x66, 0x77, 0x88,
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet99),
                                      QUIC_ARRAYSIZE(packet99));
}

TEST_P(QuicFramerTest, BuildBlockedPacket) {
  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicBlockedFrame blocked_frame;
  blocked_frame.stream_id = kStreamId;
  blocked_frame.offset = kStreamOffset;

  QuicFrames frames = {QuicFrame(&blocked_frame)};

  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78, 0x56, 0x34, 0x12,

    // frame type (blocked frame)
    0x05,
    // stream id
    0x04, 0x03, 0x02, 0x01,
  };

  unsigned char packet39[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (blocked frame)
    0x05,
    // stream id
    0x01, 0x02, 0x03, 0x04,
  };

  unsigned char packet44[] = {
    // type (short packet, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (blocked frame)
    0x05,
    // stream id
    0x01, 0x02, 0x03, 0x04,
  };

  unsigned char packet99[] = {
    // type (short packet, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type
    0x09,
    // stream id
    kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04,
    // Offset
    kVarInt62EightBytes + 0x3a, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    p_size = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  } else if (framer_.transport_version() != QUIC_VERSION_35) {
    p = packet39;
  }

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(p), p_size);
}

TEST_P(QuicFramerTest, BuildPingPacket) {
  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicFrames frames = {QuicFrame(QuicPingFrame())};

  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78, 0x56, 0x34, 0x12,

    // frame type (ping frame)
    0x07,
  };

  unsigned char packet39[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (ping frame)
    0x07,
  };

  unsigned char packet44[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type
    0x07,
  };
  // clang-format on

  unsigned char* p = packet;
  if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
  } else if (framer_.transport_version() != QUIC_VERSION_35) {
    p = packet39;
  }

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(p),
      framer_.transport_version() > QUIC_VERSION_43 ? QUIC_ARRAYSIZE(packet44)
                                                    : QUIC_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, BuildMessagePacket) {
  if (framer_.transport_version() <= QUIC_VERSION_44) {
    return;
  }
  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicMessageFrame frame(1, "message");
  QuicMessageFrame frame2(2, "message2");
  QuicFrames frames = {QuicFrame(&frame), QuicFrame(&frame2)};

  // clang-format off
  unsigned char packet45[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (message frame)
    0x21,
    // Length
    0x07,
    // Message Data
    'm', 'e', 's', 's', 'a', 'g', 'e',
    // frame type (message frame no length)
    0x20,
    // Message Data
    'm', 'e', 's', 's', 'a', 'g', 'e', '2'
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet45),
                                      QUIC_ARRAYSIZE(packet45));
}

// Test that the connectivity probing packet is serialized correctly as a
// padded PING packet.
TEST_P(QuicFramerTest, BuildConnectivityProbingPacket) {
  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78, 0x56, 0x34, 0x12,

    // frame type (ping frame)
    0x07,
    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet39[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (ping frame)
    0x07,
    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  unsigned char packet44[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type
    0x07,
    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };
  // clang-format on

  unsigned char* p = packet;
  size_t packet_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    packet_size = QUIC_ARRAYSIZE(packet44);
  } else if (framer_.transport_version() != QUIC_VERSION_35) {
    p = packet39;
    packet_size = QUIC_ARRAYSIZE(packet39);
  }

  std::unique_ptr<char[]> buffer(new char[kMaxPacketSize]);

  size_t length =
      framer_.BuildConnectivityProbingPacket(header, buffer.get(), packet_size);

  EXPECT_NE(0u, length);
  QuicPacket data(buffer.release(), length, true,
                  header.destination_connection_id_length,
                  header.source_connection_id_length, header.version_flag,
                  header.nonce != nullptr, header.packet_number_length);

  test::CompareCharArraysWithHexError("constructed packet", data.data(),
                                      data.length(), AsChars(p), packet_size);
}

// Test that the path challenge connectivity probing packet is serialized
// correctly as a padded PATH CHALLENGE packet.
TEST_P(QuicFramerTest, BuildPaddedPathChallengePacket) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;
  QuicPathFrameBuffer payload;

  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // Path Challenge Frame type
    0x0e,
    // 8 "random" bytes, MockRandom makes lots of r's
    'r', 'r', 'r', 'r', 'r', 'r', 'r', 'r',
    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };
  // clang-format on

  std::unique_ptr<char[]> buffer(new char[kMaxPacketSize]);
  MockRandom randomizer;

  size_t length = framer_.BuildPaddedPathChallengePacket(
      header, buffer.get(), QUIC_ARRAYSIZE(packet), &payload, &randomizer);
  EXPECT_EQ(length, QUIC_ARRAYSIZE(packet));

  // Payload has the random bytes that were generated. Copy them into packet,
  // above, before checking that the generated packet is correct.
  EXPECT_EQ(kQuicPathFrameBufferSize, payload.size());

  QuicPacket data(buffer.release(), length, true,
                  header.destination_connection_id_length,
                  header.source_connection_id_length, header.version_flag,
                  header.nonce != nullptr, header.packet_number_length);

  test::CompareCharArraysWithHexError("constructed packet", data.data(),
                                      data.length(), AsChars(packet),
                                      QUIC_ARRAYSIZE(packet));
}

// Several tests that the path response connectivity probing packet is
// serialized correctly as either a padded and unpadded PATH RESPONSE
// packet. Also generates packets with 1 and 3 PATH_RESPONSES in them to
// exercised the single- and multiple- payload cases.
TEST_P(QuicFramerTest, BuildPathResponsePacket1ResponseUnpadded) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;
  QuicPathFrameBuffer payload0 = {
      {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}};

  // Build 1 PATH RESPONSE, not padded
  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // Path Challenge Frame type
    0x0f,
    // 8 "random" bytes
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
  };
  // clang-format on
  std::unique_ptr<char[]> buffer(new char[kMaxPacketSize]);
  QuicDeque<QuicPathFrameBuffer> payloads;
  payloads.push_back(payload0);
  size_t length = framer_.BuildPathResponsePacket(
      header, buffer.get(), QUIC_ARRAYSIZE(packet), payloads,
      /*is_padded=*/false);
  EXPECT_EQ(length, QUIC_ARRAYSIZE(packet));
  QuicPacket data(buffer.release(), length, true,
                  header.destination_connection_id_length,
                  header.source_connection_id_length, header.version_flag,
                  header.nonce != nullptr, header.packet_number_length);

  test::CompareCharArraysWithHexError("constructed packet", data.data(),
                                      data.length(), AsChars(packet),
                                      QUIC_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, BuildPathResponsePacket1ResponsePadded) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;
  QuicPathFrameBuffer payload0 = {
      {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}};

  // Build 1 PATH RESPONSE, padded
  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // Path Challenge Frame type
    0x0f,
    // 8 "random" bytes
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    // Padding type and pad
    0x00, 0x00, 0x00, 0x00, 0x00
  };
  // clang-format on
  std::unique_ptr<char[]> buffer(new char[kMaxPacketSize]);
  QuicDeque<QuicPathFrameBuffer> payloads;
  payloads.push_back(payload0);
  size_t length = framer_.BuildPathResponsePacket(
      header, buffer.get(), QUIC_ARRAYSIZE(packet), payloads,
      /*is_padded=*/true);
  EXPECT_EQ(length, QUIC_ARRAYSIZE(packet));
  QuicPacket data(buffer.release(), length, true,
                  header.destination_connection_id_length,
                  header.source_connection_id_length, header.version_flag,
                  header.nonce != nullptr, header.packet_number_length);

  test::CompareCharArraysWithHexError("constructed packet", data.data(),
                                      data.length(), AsChars(packet),
                                      QUIC_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, BuildPathResponsePacket3ResponsesUnpadded) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;
  QuicPathFrameBuffer payload0 = {
      {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}};
  QuicPathFrameBuffer payload1 = {
      {0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18}};
  QuicPathFrameBuffer payload2 = {
      {0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28}};

  // Build one packet with 3 PATH RESPONSES, no padding
  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // 3 path challenge frames (type byte and payload)
    0x0f, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x0f, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
    0x0f, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
  };
  // clang-format on

  std::unique_ptr<char[]> buffer(new char[kMaxPacketSize]);
  QuicDeque<QuicPathFrameBuffer> payloads;
  payloads.push_back(payload0);
  payloads.push_back(payload1);
  payloads.push_back(payload2);
  size_t length = framer_.BuildPathResponsePacket(
      header, buffer.get(), QUIC_ARRAYSIZE(packet), payloads,
      /*is_padded=*/false);
  EXPECT_EQ(length, QUIC_ARRAYSIZE(packet));
  QuicPacket data(buffer.release(), length, true,
                  header.destination_connection_id_length,
                  header.source_connection_id_length, header.version_flag,
                  header.nonce != nullptr, header.packet_number_length);

  test::CompareCharArraysWithHexError("constructed packet", data.data(),
                                      data.length(), AsChars(packet),
                                      QUIC_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, BuildPathResponsePacket3ResponsesPadded) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;
  QuicPathFrameBuffer payload0 = {
      {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}};
  QuicPathFrameBuffer payload1 = {
      {0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18}};
  QuicPathFrameBuffer payload2 = {
      {0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28}};

  // Build one packet with 3 PATH RESPONSES, with padding
  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // 3 path challenge frames (type byte and payload)
    0x0f, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x0f, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
    0x0f, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    // Padding
    0x00, 0x00, 0x00, 0x00, 0x00
  };
  // clang-format on

  std::unique_ptr<char[]> buffer(new char[kMaxPacketSize]);
  QuicDeque<QuicPathFrameBuffer> payloads;
  payloads.push_back(payload0);
  payloads.push_back(payload1);
  payloads.push_back(payload2);
  size_t length = framer_.BuildPathResponsePacket(
      header, buffer.get(), QUIC_ARRAYSIZE(packet), payloads,
      /*is_padded=*/true);
  EXPECT_EQ(length, QUIC_ARRAYSIZE(packet));
  QuicPacket data(buffer.release(), length, true,
                  header.destination_connection_id_length,
                  header.source_connection_id_length, header.version_flag,
                  header.nonce != nullptr, header.packet_number_length);

  test::CompareCharArraysWithHexError("constructed packet", data.data(),
                                      data.length(), AsChars(packet),
                                      QUIC_ARRAYSIZE(packet));
}

// Test that the MTU discovery packet is serialized correctly as a PING packet.
TEST_P(QuicFramerTest, BuildMtuDiscoveryPacket) {
  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicFrames frames = {QuicFrame(QuicMtuDiscoveryFrame())};

  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78, 0x56, 0x34, 0x12,

    // frame type (ping frame)
    0x07,
  };

  unsigned char packet39[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (ping frame)
    0x07,
  };

  unsigned char packet44[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type
    0x07,
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  unsigned char* p = packet;
  if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
  } else if (framer_.transport_version() != QUIC_VERSION_35) {
    p = packet39;
  }

  test::CompareCharArraysWithHexError(
      "constructed packet", data->data(), data->length(), AsChars(p),
      framer_.transport_version() > QUIC_VERSION_43 ? QUIC_ARRAYSIZE(packet44)
                                                    : QUIC_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, BuildPublicResetPacket) {
  QuicPublicResetPacket reset_packet;
  reset_packet.connection_id = kConnectionId;
  reset_packet.nonce_proof = kNonceProof;

  // clang-format off
  unsigned char packet[] = {
    // public flags (public reset, 8 byte ConnectionId)
    0x0E,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // message tag (kPRST)
    'P', 'R', 'S', 'T',
    // num_entries (1) + padding
    0x01, 0x00, 0x00, 0x00,
    // tag kRNON
    'R', 'N', 'O', 'N',
    // end offset 8
    0x08, 0x00, 0x00, 0x00,
    // nonce proof
    0x89, 0x67, 0x45, 0x23,
    0x01, 0xEF, 0xCD, 0xAB,
  };
  // clang-format on

  if (framer_.transport_version() > QUIC_VERSION_43) {
    return;
  }

  std::unique_ptr<QuicEncryptedPacket> data(
      framer_.BuildPublicResetPacket(reset_packet));
  ASSERT_TRUE(data != nullptr);
  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet),
                                      QUIC_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, BuildPublicResetPacketWithClientAddress) {
  QuicPublicResetPacket reset_packet;
  reset_packet.connection_id = kConnectionId;
  reset_packet.nonce_proof = kNonceProof;
  reset_packet.client_address =
      QuicSocketAddress(QuicIpAddress::Loopback4(), 0x1234);

  // clang-format off
  unsigned char packet[] = {
      // public flags (public reset, 8 byte ConnectionId)
      0x0E,
      // connection_id
      0xFE, 0xDC, 0xBA, 0x98,
      0x76, 0x54, 0x32, 0x10,
      // message tag (kPRST)
      'P', 'R', 'S', 'T',
      // num_entries (2) + padding
      0x02, 0x00, 0x00, 0x00,
      // tag kRNON
      'R', 'N', 'O', 'N',
      // end offset 8
      0x08, 0x00, 0x00, 0x00,
      // tag kCADR
      'C', 'A', 'D', 'R',
      // end offset 16
      0x10, 0x00, 0x00, 0x00,
      // nonce proof
      0x89, 0x67, 0x45, 0x23,
      0x01, 0xEF, 0xCD, 0xAB,
      // client address
      0x02, 0x00,
      0x7F, 0x00, 0x00, 0x01,
      0x34, 0x12,
  };
  // clang-format on

  if (framer_.transport_version() > QUIC_VERSION_43) {
    return;
  }

  std::unique_ptr<QuicEncryptedPacket> data(
      framer_.BuildPublicResetPacket(reset_packet));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet),
                                      QUIC_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, BuildIetfStatelessResetPacket) {
  // clang-format off
  unsigned char packet[] = {
    // type (short header, 1 byte packet number)
    0x30,
    // random packet number
    0xFE,
    // stateless reset token
    0xB5, 0x69, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  // clang-format on
  std::unique_ptr<QuicEncryptedPacket> data(
      framer_.BuildIetfStatelessResetPacket(kConnectionId,
                                            kTestStatelessResetToken));
  ASSERT_TRUE(data != nullptr);
  // Skip packet number byte which is random in stateless reset packet.
  test::CompareCharArraysWithHexError("constructed packet", data->data(), 1,
                                      AsChars(packet), 1);
  const size_t random_bytes_length =
      data->length() - kPacketHeaderTypeSize - sizeof(kTestStatelessResetToken);
  if (GetQuicReloadableFlag(quic_more_random_bytes_in_stateless_reset)) {
    EXPECT_EQ(kMinRandomBytesLengthInStatelessReset, random_bytes_length);
  } else {
    EXPECT_EQ(1u, random_bytes_length);
  }
  // Verify stateless reset token is correct.
  test::CompareCharArraysWithHexError(
      "constructed packet",
      data->data() + data->length() - sizeof(kTestStatelessResetToken),
      sizeof(kTestStatelessResetToken),
      AsChars(packet) + QUIC_ARRAYSIZE(packet) -
          sizeof(kTestStatelessResetToken),
      sizeof(kTestStatelessResetToken));
}

TEST_P(QuicFramerTest, EncryptPacket) {
  QuicPacketNumber packet_number = kPacketNumber;
  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78, 0x56, 0x34, 0x12,

    // redundancy
    'a',  'b',  'c',  'd',
    'e',  'f',  'g',  'h',
    'i',  'j',  'k',  'l',
    'm',  'n',  'o',  'p',
  };

  unsigned char packet39[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // redundancy
    'a',  'b',  'c',  'd',
    'e',  'f',  'g',  'h',
    'i',  'j',  'k',  'l',
    'm',  'n',  'o',  'p',
  };

  unsigned char packet44[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // redundancy
    'a',  'b',  'c',  'd',
    'e',  'f',  'g',  'h',
    'i',  'j',  'k',  'l',
    'm',  'n',  'o',  'p',
  };
  // clang-format on

  unsigned char* p = packet;
  if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
  } else if (framer_.transport_version() != QUIC_VERSION_35) {
    p = packet39;
  }

  std::unique_ptr<QuicPacket> raw(new QuicPacket(
      AsChars(p), QUIC_ARRAYSIZE(packet), false, PACKET_8BYTE_CONNECTION_ID,
      PACKET_0BYTE_CONNECTION_ID, !kIncludeVersion,
      !kIncludeDiversificationNonce, PACKET_4BYTE_PACKET_NUMBER));
  char buffer[kMaxPacketSize];
  size_t encrypted_length = framer_.EncryptPayload(
      ENCRYPTION_NONE, packet_number, *raw, buffer, kMaxPacketSize);

  ASSERT_NE(0u, encrypted_length);
  EXPECT_TRUE(CheckEncryption(packet_number, raw.get()));
}

TEST_P(QuicFramerTest, EncryptPacketWithVersionFlag) {
  QuicPacketNumber packet_number = kPacketNumber;
  // clang-format off
  unsigned char packet[] = {
    // public flags (version, 8 byte connection_id)
    0x29,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // version tag
    'Q', '.', '1', '0',
    // packet number
    0x78, 0x56, 0x34, 0x12,

    // redundancy
    'a',  'b',  'c',  'd',
    'e',  'f',  'g',  'h',
    'i',  'j',  'k',  'l',
    'm',  'n',  'o',  'p',
  };

  unsigned char packet39[] = {
    // public flags (version, 8 byte connection_id)
    0x29,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // version tag
    'Q', '.', '1', '0',
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // redundancy
    'a',  'b',  'c',  'd',
    'e',  'f',  'g',  'h',
    'i',  'j',  'k',  'l',
    'm',  'n',  'o',  'p',
  };

  unsigned char packet44[] = {
    // type (long header with packet type ZERO_RTT_PROTECTED)
    0xFC,
    // version tag
    'Q', '.', '1', '0',
    // connection_id length
    0x50,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // redundancy
    'a',  'b',  'c',  'd',
    'e',  'f',  'g',  'h',
    'i',  'j',  'k',  'l',
    'm',  'n',  'o',  'p',
  };
  // clang-format on

  unsigned char* p = packet;
  if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
  } else if (framer_.transport_version() != QUIC_VERSION_35) {
    p = packet39;
  }

  std::unique_ptr<QuicPacket> raw(new QuicPacket(
      AsChars(p),
      framer_.transport_version() > QUIC_VERSION_43 ? QUIC_ARRAYSIZE(packet44)
                                                    : QUIC_ARRAYSIZE(packet),
      false, PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID,
      kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_4BYTE_PACKET_NUMBER));
  char buffer[kMaxPacketSize];
  size_t encrypted_length = framer_.EncryptPayload(
      ENCRYPTION_NONE, packet_number, *raw, buffer, kMaxPacketSize);

  ASSERT_NE(0u, encrypted_length);
  EXPECT_TRUE(CheckEncryption(packet_number, raw.get()));
}

TEST_P(QuicFramerTest, AckTruncationLargePacket) {
  if (framer_.transport_version() == QUIC_VERSION_99) {
    // This test is not applicable to this version; the range count is
    // effectively unlimited
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicAckFrame ack_frame;
  // Create a packet with just the ack.
  ack_frame = MakeAckFrameWithAckBlocks(300, 0u);
  QuicFrames frames = {QuicFrame(&ack_frame)};

  // Build an ack packet with truncation due to limit in number of nack ranges.
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  std::unique_ptr<QuicPacket> raw_ack_packet(BuildDataPacket(header, frames));
  ASSERT_TRUE(raw_ack_packet != nullptr);
  char buffer[kMaxPacketSize];
  size_t encrypted_length =
      framer_.EncryptPayload(ENCRYPTION_NONE, header.packet_number,
                             *raw_ack_packet, buffer, kMaxPacketSize);
  ASSERT_NE(0u, encrypted_length);
  // Now make sure we can turn our ack packet back into an ack frame.
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  ASSERT_TRUE(framer_.ProcessPacket(
      QuicEncryptedPacket(buffer, encrypted_length, false)));
  ASSERT_EQ(1u, visitor_.ack_frames_.size());
  QuicAckFrame& processed_ack_frame = *visitor_.ack_frames_[0];
  EXPECT_EQ(600u, LargestAcked(processed_ack_frame));
  ASSERT_EQ(256u, processed_ack_frame.packets.NumPacketsSlow());
  EXPECT_EQ(90u, processed_ack_frame.packets.Min());
  EXPECT_EQ(600u, processed_ack_frame.packets.Max());
}

TEST_P(QuicFramerTest, AckTruncationSmallPacket) {
  if (framer_.transport_version() == QUIC_VERSION_99) {
    // This test is not applicable to this version; the range count is
    // effectively unlimited
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  // Create a packet with just the ack.
  QuicAckFrame ack_frame;
  ack_frame = MakeAckFrameWithAckBlocks(300, 0u);
  QuicFrames frames = {QuicFrame(&ack_frame)};

  // Build an ack packet with truncation due to limit in number of nack ranges.
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  std::unique_ptr<QuicPacket> raw_ack_packet(
      BuildDataPacket(header, frames, 500));
  ASSERT_TRUE(raw_ack_packet != nullptr);
  char buffer[kMaxPacketSize];
  size_t encrypted_length =
      framer_.EncryptPayload(ENCRYPTION_NONE, header.packet_number,
                             *raw_ack_packet, buffer, kMaxPacketSize);
  ASSERT_NE(0u, encrypted_length);
  // Now make sure we can turn our ack packet back into an ack frame.
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  ASSERT_TRUE(framer_.ProcessPacket(
      QuicEncryptedPacket(buffer, encrypted_length, false)));
  ASSERT_EQ(1u, visitor_.ack_frames_.size());
  QuicAckFrame& processed_ack_frame = *visitor_.ack_frames_[0];
  EXPECT_EQ(600u, LargestAcked(processed_ack_frame));
  ASSERT_EQ(240u, processed_ack_frame.packets.NumPacketsSlow());
  EXPECT_EQ(122u, processed_ack_frame.packets.Min());
  EXPECT_EQ(600u, processed_ack_frame.packets.Max());
}

TEST_P(QuicFramerTest, CleanTruncation) {
  if (framer_.transport_version() == QUIC_VERSION_99) {
    // This test is not applicable to this version; the range count is
    // effectively unlimited
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicAckFrame ack_frame = InitAckFrame(201);

  // Create a packet with just the ack.
  QuicFrames frames = {QuicFrame(&ack_frame)};
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  std::unique_ptr<QuicPacket> raw_ack_packet(BuildDataPacket(header, frames));
  ASSERT_TRUE(raw_ack_packet != nullptr);

  char buffer[kMaxPacketSize];
  size_t encrypted_length =
      framer_.EncryptPayload(ENCRYPTION_NONE, header.packet_number,
                             *raw_ack_packet, buffer, kMaxPacketSize);
  ASSERT_NE(0u, encrypted_length);

  // Now make sure we can turn our ack packet back into an ack frame.
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_SERVER);
  ASSERT_TRUE(framer_.ProcessPacket(
      QuicEncryptedPacket(buffer, encrypted_length, false)));

  // Test for clean truncation of the ack by comparing the length of the
  // original packets to the re-serialized packets.
  frames.clear();
  frames.push_back(QuicFrame(visitor_.ack_frames_[0].get()));

  size_t original_raw_length = raw_ack_packet->length();
  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  raw_ack_packet = BuildDataPacket(header, frames);
  ASSERT_TRUE(raw_ack_packet != nullptr);
  EXPECT_EQ(original_raw_length, raw_ack_packet->length());
  ASSERT_TRUE(raw_ack_packet != nullptr);
}

TEST_P(QuicFramerTest, StopPacketProcessing) {
  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78, 0x56, 0x34, 0x12,

    // frame type (stream frame with fin)
    0xFF,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0x3A,
    // data length
    0x0c, 0x00,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',

    // frame type (ack frame)
    0x40,
    // least packet number awaiting an ack
    0xA0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // largest observed packet number
    0xBF, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // num missing packets
    0x01,
    // missing packet
    0xBE, 0x9A, 0x78, 0x56,
    0x34, 0x12,
  };

  unsigned char packet39[] = {
    // public flags (8 byte connection_id)
    0x28,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (stream frame with fin)
    0xFF,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // offset
    0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data length
    0x00, 0x0c,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',

    // frame type (ack frame)
    0x40,
    // least packet number awaiting an ack
    0x12, 0x34, 0x56, 0x78,
    0x9A, 0xA0,
    // largest observed packet number
    0x12, 0x34, 0x56, 0x78,
    0x9A, 0xBF,
    // num missing packets
    0x01,
    // missing packet
    0x12, 0x34, 0x56, 0x78,
    0x9A, 0xBE,
  };

  unsigned char packet44[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (stream frame with fin)
    0xFF,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // offset
    0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data length
    0x00, 0x0c,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',

    // frame type (ack frame)
    0x40,
    // least packet number awaiting an ack
    0x12, 0x34, 0x56, 0x78,
    0x9A, 0xA0,
    // largest observed packet number
    0x12, 0x34, 0x56, 0x78,
    0x9A, 0xBF,
    // num missing packets
    0x01,
    // missing packet
    0x12, 0x34, 0x56, 0x78,
    0x9A, 0xBE,
  };

  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (stream frame with fin, length, and offset bits set)
    0x10 | 0x01 | 0x02 | 0x04,
    // stream id
    kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04,
    // offset
    kVarInt62EightBytes + 0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data length
    kVarInt62TwoBytes + 0x00, 0x0c,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',

    // frame type (ack frame)
    0x0d,
    // largest observed packet number
    kVarInt62FourBytes + 0x12, 0x34, 0x56, 0x78,
    // Delta time
    kVarInt62OneByte + 0x00,
    // Ack Block count
    kVarInt62OneByte + 0x01,
    // First block size (one packet)
    kVarInt62OneByte + 0x00,

    // Next gap size & ack. Missing all preceding packets
    kVarInt62FourBytes + 0x12, 0x34, 0x56, 0x77,
    kVarInt62OneByte + 0x00,
  };
  // clang-format on

  MockFramerVisitor visitor;
  framer_.set_visitor(&visitor);
  EXPECT_CALL(visitor, OnPacket());
  EXPECT_CALL(visitor, OnPacketHeader(_));
  EXPECT_CALL(visitor, OnStreamFrame(_)).WillOnce(Return(false));
  EXPECT_CALL(visitor, OnPacketComplete());
  EXPECT_CALL(visitor, OnUnauthenticatedPublicHeader(_)).WillOnce(Return(true));
  EXPECT_CALL(visitor, OnUnauthenticatedHeader(_)).WillOnce(Return(true));
  EXPECT_CALL(visitor, OnDecryptedPacket(_));

  unsigned char* p = packet;
  size_t p_size = QUIC_ARRAYSIZE(packet);
  if (framer_.transport_version() == QUIC_VERSION_99) {
    p = packet99;
    p_size = QUIC_ARRAYSIZE(packet99);
  } else if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
    p_size = QUIC_ARRAYSIZE(packet44);
  } else if (framer_.transport_version() != QUIC_VERSION_35) {
    p = packet39;
  }
  QuicEncryptedPacket encrypted(AsChars(p), p_size, false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
}

static char kTestString[] = "At least 20 characters.";
static QuicStreamId kTestQuicStreamId = 1;
static bool ExpectedStreamFrame(const QuicStreamFrame& frame) {
  return (frame.stream_id == kTestQuicStreamId ||
          frame.stream_id == QuicUtils::GetCryptoStreamId(QUIC_VERSION_99)) &&
         !frame.fin && frame.offset == 0 &&
         QuicString(frame.data_buffer, frame.data_length) == kTestString;
  // FIN is hard-coded false in ConstructEncryptedPacket.
  // Offset 0 is hard-coded in ConstructEncryptedPacket.
}

// Verify that the packet returned by ConstructEncryptedPacket() can be properly
// parsed by the framer.
TEST_P(QuicFramerTest, ConstructEncryptedPacket) {
  // Since we are using ConstructEncryptedPacket, we have to set the framer's
  // crypto to be Null.
  framer_.SetDecrypter(ENCRYPTION_NONE,
                       QuicMakeUnique<NullDecrypter>(framer_.perspective()));
  framer_.SetEncrypter(ENCRYPTION_NONE,
                       QuicMakeUnique<NullEncrypter>(framer_.perspective()));
  ParsedQuicVersionVector versions;
  versions.push_back(framer_.version());
  std::unique_ptr<QuicEncryptedPacket> packet(ConstructEncryptedPacket(
      42, 0, false, false, kTestQuicStreamId, kTestString,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID,
      PACKET_4BYTE_PACKET_NUMBER, &versions));

  MockFramerVisitor visitor;
  framer_.set_visitor(&visitor);
  EXPECT_CALL(visitor, OnPacket()).Times(1);
  EXPECT_CALL(visitor, OnUnauthenticatedPublicHeader(_))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(visitor, OnUnauthenticatedHeader(_))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(visitor, OnPacketHeader(_)).Times(1).WillOnce(Return(true));
  EXPECT_CALL(visitor, OnDecryptedPacket(_)).Times(1);
  EXPECT_CALL(visitor, OnError(_)).Times(0);
  EXPECT_CALL(visitor, OnStreamFrame(_)).Times(0);
  EXPECT_CALL(visitor, OnStreamFrame(Truly(ExpectedStreamFrame))).Times(1);
  EXPECT_CALL(visitor, OnPacketComplete()).Times(1);

  EXPECT_TRUE(framer_.ProcessPacket(*packet));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
}

// Verify that the packet returned by ConstructMisFramedEncryptedPacket()
// does cause the framer to return an error.
TEST_P(QuicFramerTest, ConstructMisFramedEncryptedPacket) {
  // Since we are using ConstructEncryptedPacket, we have to set the framer's
  // crypto to be Null.
  framer_.SetDecrypter(ENCRYPTION_NONE,
                       QuicMakeUnique<NullDecrypter>(framer_.perspective()));
  framer_.SetEncrypter(ENCRYPTION_NONE,
                       QuicMakeUnique<NullEncrypter>(framer_.perspective()));
  ParsedQuicVersionVector versions;
  versions.push_back(framer_.version());
  std::unique_ptr<QuicEncryptedPacket> packet(ConstructMisFramedEncryptedPacket(
      42, 0, false, false, kTestQuicStreamId, kTestString,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID,
      PACKET_4BYTE_PACKET_NUMBER, &versions, Perspective::IS_CLIENT));

  MockFramerVisitor visitor;
  framer_.set_visitor(&visitor);
  EXPECT_CALL(visitor, OnPacket()).Times(1);
  EXPECT_CALL(visitor, OnUnauthenticatedPublicHeader(_))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(visitor, OnUnauthenticatedHeader(_))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(visitor, OnPacketHeader(_)).Times(1);
  EXPECT_CALL(visitor, OnDecryptedPacket(_)).Times(1);
  EXPECT_CALL(visitor, OnError(_)).Times(1);
  EXPECT_CALL(visitor, OnStreamFrame(_)).Times(0);
  EXPECT_CALL(visitor, OnPacketComplete()).Times(0);

  EXPECT_FALSE(framer_.ProcessPacket(*packet));
  EXPECT_EQ(QUIC_INVALID_FRAME_DATA, framer_.error());
}

// Tests for fuzzing with Dr. Fuzz
// Xref http://www.chromium.org/developers/testing/dr-fuzz for more details.
#ifdef __cplusplus
extern "C" {
#endif

// target function to be fuzzed by Dr. Fuzz
void QuicFramerFuzzFunc(unsigned char* data,
                        size_t size,
                        const ParsedQuicVersion& version) {
  QuicFramer framer(AllSupportedVersions(), QuicTime::Zero(),
                    Perspective::IS_SERVER);
  ASSERT_EQ(GetQuicFlag(FLAGS_quic_supports_tls_handshake), true);
  const char* const packet_bytes = reinterpret_cast<const char*>(data);

  // Test the CryptoFramer.
  QuicStringPiece crypto_input(packet_bytes, size);
  std::unique_ptr<CryptoHandshakeMessage> handshake_message(
      CryptoFramer::ParseMessage(crypto_input));

  // Test the regular QuicFramer with the same input.
  NoOpFramerVisitor visitor;
  framer.set_visitor(&visitor);
  framer.set_version(version);
  QuicEncryptedPacket packet(packet_bytes, size);
  framer.ProcessPacket(packet);
}

#ifdef __cplusplus
}
#endif

TEST_P(QuicFramerTest, FramerFuzzTest) {
  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x2C,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x78, 0x56, 0x34, 0x12,
    // private flags
    0x00,

    // frame type (stream frame with fin)
    0xFF,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0x3A,
    // data length
    0x0c, 0x00,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  unsigned char packet39[] = {
    // public flags (8 byte connection_id)
    0x2C,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,
    // private flags
    0x00,

    // frame type (stream frame with fin)
    0xFF,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // offset
    0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data length
    0x00, 0x0c,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  unsigned char packet44[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (stream frame with fin, length, and offset bits set)
    0x10 | 0x01 | 0x02 | 0x04,
    // stream id
    0x01, 0x02, 0x03, 0x04,
    // offset
    0x3A, 0x98, 0xFE, 0xDC,
    0x32, 0x10, 0x76, 0x54,
    // data length
    0x00, 0x0c,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };
  // clang-format on

  unsigned char* p = packet;
  if (framer_.transport_version() > QUIC_VERSION_43) {
    p = packet44;
  } else if (framer_.transport_version() != QUIC_VERSION_35) {
    p = packet39;
  }
  QuicFramerFuzzFunc(p,
                     framer_.transport_version() > QUIC_VERSION_43
                         ? QUIC_ARRAYSIZE(packet44)
                         : QUIC_ARRAYSIZE(packet),
                     framer_.version());
}

TEST_P(QuicFramerTest, StartsWithChlo) {
  SimpleDataProducer producer;
  framer_.set_data_producer(&producer);
  QuicStringPiece data("CHLOCHLO");
  struct iovec iovec;
  iovec.iov_base = const_cast<char*>(data.data());
  iovec.iov_len = data.length();
  producer.SaveStreamData(
      QuicUtils::GetCryptoStreamId(framer_.transport_version()), &iovec, 1, 0,
      0, data.length());
  for (size_t offset = 0; offset < 5; ++offset) {
    if (offset == 0 || offset == 4) {
      EXPECT_TRUE(framer_.StartsWithChlo(
          QuicUtils::GetCryptoStreamId(framer_.transport_version()), offset));
    } else {
      EXPECT_FALSE(framer_.StartsWithChlo(
          QuicUtils::GetCryptoStreamId(framer_.transport_version()), offset));
    }
  }
}

TEST_P(QuicFramerTest, IetfBlockedFrame) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (blocked)
      {"",
       {0x08}},
      // blocked offset
      {"Can not read blocked offset.",
       {kVarInt62EightBytes + 0x3a, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(kStreamOffset, visitor_.blocked_frame_.offset);

  CheckFramingBoundaries(packet99, QUIC_INVALID_BLOCKED_DATA);
}

TEST_P(QuicFramerTest, BuildIetfBlockedPacket) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicBlockedFrame frame;
  frame.stream_id = 0;
  frame.offset = kStreamOffset;
  QuicFrames frames = {QuicFrame(&frame)};

  // clang-format off
  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (blocked)
    0x08,
    // Offset
    kVarInt62EightBytes + 0x3a, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet99),
                                      QUIC_ARRAYSIZE(packet99));
}

TEST_P(QuicFramerTest, IetfStreamBlockedFrame) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (blocked)
      {"",
       {0x09}},
      // blocked offset
      {"Can not read stream blocked stream id.",
       {kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04}},
      {"Can not read stream blocked offset.",
       {kVarInt62EightBytes + 0x3a, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(kStreamId, visitor_.blocked_frame_.stream_id);
  EXPECT_EQ(kStreamOffset, visitor_.blocked_frame_.offset);

  CheckFramingBoundaries(packet99, QUIC_INVALID_STREAM_BLOCKED_DATA);
}

TEST_P(QuicFramerTest, BuildIetfStreamBlockedPacket) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicBlockedFrame frame;
  frame.stream_id = kStreamId;
  frame.offset = kStreamOffset;
  QuicFrames frames = {QuicFrame(&frame)};

  // clang-format off
  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (blocked)
    0x09,
    // Stream ID
    kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04,
    // Offset
    kVarInt62EightBytes + 0x3a, 0x98, 0xFE, 0xDC, 0x32, 0x10, 0x76, 0x54
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet99),
                                      QUIC_ARRAYSIZE(packet99));
}

TEST_P(QuicFramerTest, MaxStreamIdFrame) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (max stream id)
      {"",
       {0x06}},
      // max. stream id
      {"Can not read MAX_STREAM_ID stream id.",
       {kVarInt62OneByte + 0x01}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(0x1u, visitor_.max_stream_id_frame_.max_stream_id);

  CheckFramingBoundaries(packet99, QUIC_MAX_STREAM_ID_DATA);
}

TEST_P(QuicFramerTest, BuildMaxStreamIdPacket) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicMaxStreamIdFrame frame;
  frame.max_stream_id = kTestQuicStreamId;
  QuicFrames frames = {QuicFrame(frame)};

  // clang-format off
  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (max stream id frame)
    0x06,
    // Max stream id
    kVarInt62OneByte + 0x01
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet99),
                                      QUIC_ARRAYSIZE(packet99));
}

TEST_P(QuicFramerTest, StreamIdBlockedFrame) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (stream id blocked)
      {"",
       {0x0a}},
      // stream id
      {"Can not read STREAM_ID_BLOCKED stream id.",
       {kVarInt62OneByte + 0x01}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(0x1u, visitor_.stream_id_blocked_frame_.stream_id);

  CheckFramingBoundaries(packet99, QUIC_STREAM_ID_BLOCKED_DATA);
}

TEST_P(QuicFramerTest, BuildStreamIdBlockedPacket) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicStreamIdBlockedFrame frame;
  frame.stream_id = kTestQuicStreamId;
  QuicFrames frames = {QuicFrame(frame)};

  // clang-format off
  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (stream id blocked frame)
    0x0a,
    // Max stream id
    kVarInt62OneByte + 0x01
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet99),
                                      QUIC_ARRAYSIZE(packet99));
}

TEST_P(QuicFramerTest, NewConnectionIdFrame) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // This frame is only for version 99.
    return;
  }
  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (new connection id frame)
      {"",
       {0x0b}},
      // error code
      {"Unable to read new connection ID frame sequence number.",
       {kVarInt62OneByte + 0x11}},
      {"Unable to read new connection ID frame connection id length.",
       {0x08}},
      {"Unable to read new connection ID frame connection id.",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x11}},
      {"Can not read new connection ID frame reset token.",
       {0xb5, 0x69, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());

  EXPECT_EQ(kConnectionId + 1, visitor_.new_connection_id_.connection_id);
  EXPECT_EQ(0x11u, visitor_.new_connection_id_.sequence_number);
  EXPECT_EQ(kTestStatelessResetToken,
            visitor_.new_connection_id_.stateless_reset_token);

  ASSERT_EQ(0u, visitor_.ack_frames_.size());

  CheckFramingBoundaries(packet99, QUIC_INVALID_NEW_CONNECTION_ID_DATA);
}

TEST_P(QuicFramerTest, BuildNewConnectionIdFramePacket) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // This frame is only for version 99.
    return;
  }
  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicNewConnectionIdFrame frame;
  frame.sequence_number = 0x11;
  // Use this value to force a 4-byte encoded variable length connection ID
  // in the frame.
  frame.connection_id = kConnectionId + 1;
  frame.stateless_reset_token = kTestStatelessResetToken;

  QuicFrames frames = {QuicFrame(&frame)};

  // clang-format off
  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (new connection id frame)
    0x0b,
    // sequence number
    kVarInt62OneByte + 0x11,
    // new connection id length
    0x08,
    // new connection id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x11,
    // stateless reset token
    0xb5, 0x69, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet99),
                                      QUIC_ARRAYSIZE(packet99));
}

TEST_P(QuicFramerTest, NewTokenFrame) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // This frame is only for version 99.
    return;
  }
  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (new token frame)
      {"",
       {0x19}},
      // Length
      {"Unable to read new token length.",
       {kVarInt62OneByte + 0x08}},
      {"Unable to read new token data.",
       {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07}}
  };
  // clang-format on
  uint8_t expected_token_value[] = {0x00, 0x01, 0x02, 0x03,
                                    0x04, 0x05, 0x06, 0x07};

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());

  EXPECT_EQ(sizeof(expected_token_value), visitor_.new_token_.token.length());
  EXPECT_EQ(0, memcmp(expected_token_value, visitor_.new_token_.token.data(),
                      sizeof(expected_token_value)));

  CheckFramingBoundaries(packet, QUIC_INVALID_NEW_TOKEN);
}

TEST_P(QuicFramerTest, BuildNewTokenFramePacket) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // This frame is only for version 99.
    return;
  }
  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  uint8_t expected_token_value[] = {0x00, 0x01, 0x02, 0x03,
                                    0x04, 0x05, 0x06, 0x07};

  QuicNewTokenFrame frame(0, QuicString((const char*)(expected_token_value),
                                        sizeof(expected_token_value)));

  QuicFrames frames = {QuicFrame(&frame)};

  // clang-format off
  unsigned char packet[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (new token frame)
    0x19,
    // Length and token
    kVarInt62OneByte + 0x08,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet),
                                      QUIC_ARRAYSIZE(packet));
}

TEST_P(QuicFramerTest, IetfStopSendingFrame) {
  // This test is only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (stop sending)
      {"",
       {0x0c}},
      // stream id
      {"Unable to read stop sending stream id.",
       {kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04}},
      {"Unable to read stop sending application error code.",
       {0x76, 0x54}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(kStreamId, visitor_.stop_sending_frame_.stream_id);
  EXPECT_EQ(0x7654, visitor_.stop_sending_frame_.application_error_code);

  CheckFramingBoundaries(packet99, QUIC_INVALID_STOP_SENDING_FRAME_DATA);
}

TEST_P(QuicFramerTest, BuildIetfStopSendingPacket) {
  // This test is only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicStopSendingFrame frame;
  frame.stream_id = kStreamId;
  frame.application_error_code = 0xffff;
  QuicFrames frames = {QuicFrame(&frame)};

  // clang-format off
  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (stop sending)
    0x0c,
    // Stream ID
    kVarInt62FourBytes + 0x01, 0x02, 0x03, 0x04,
    // Application error code
    0xff, 0xff
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet99),
                                      QUIC_ARRAYSIZE(packet99));
}

TEST_P(QuicFramerTest, IetfPathChallengeFrame) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (path challenge)
      {"",
       {0x0e}},
      // data
      {"Can not read path challenge data.",
       {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(QuicPathFrameBuffer({{0, 1, 2, 3, 4, 5, 6, 7}}),
            visitor_.path_challenge_frame_.data_buffer);

  CheckFramingBoundaries(packet99, QUIC_INVALID_PATH_CHALLENGE_DATA);
}

TEST_P(QuicFramerTest, BuildIetfPathChallengePacket) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicPathChallengeFrame frame;
  frame.data_buffer = QuicPathFrameBuffer({{0, 1, 2, 3, 4, 5, 6, 7}});
  QuicFrames frames = {QuicFrame(&frame)};

  // clang-format off
  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (path challenge)
    0x0e,
    // Data
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet99),
                                      QUIC_ARRAYSIZE(packet99));
}

TEST_P(QuicFramerTest, IetfPathResponseFrame) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (path response)
      {"",
       {0x0f}},
      // data
      {"Can not read path response data.",
       {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07}},
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(QuicPathFrameBuffer({{0, 1, 2, 3, 4, 5, 6, 7}}),
            visitor_.path_response_frame_.data_buffer);

  CheckFramingBoundaries(packet99, QUIC_INVALID_PATH_RESPONSE_DATA);
}

TEST_P(QuicFramerTest, BuildIetfPathResponsePacket) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicPathResponseFrame frame;
  frame.data_buffer = QuicPathFrameBuffer({{0, 1, 2, 3, 4, 5, 6, 7}});
  QuicFrames frames = {QuicFrame(&frame)};

  // clang-format off
  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (path response)
    0x0f,
    // Data
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet99),
                                      QUIC_ARRAYSIZE(packet99));
}

TEST_P(QuicFramerTest, GetRetransmittableControlFrameSize) {
  QuicRstStreamFrame rst_stream(1, 3, QUIC_STREAM_CANCELLED, 1024);
  EXPECT_EQ(QuicFramer::GetRstStreamFrameSize(framer_.transport_version(),
                                              rst_stream),
            QuicFramer::GetRetransmittableControlFrameSize(
                framer_.transport_version(), QuicFrame(&rst_stream)));

  QuicString error_detail(2048, 'e');
  QuicConnectionCloseFrame connection_close(QUIC_NETWORK_IDLE_TIMEOUT,
                                            error_detail);
  EXPECT_EQ(QuicFramer::GetMinConnectionCloseFrameSize(
                framer_.transport_version(), connection_close) +
                256,
            QuicFramer::GetRetransmittableControlFrameSize(
                framer_.transport_version(), QuicFrame(&connection_close)));

  QuicGoAwayFrame goaway(2, QUIC_PEER_GOING_AWAY, 3, error_detail);
  EXPECT_EQ(QuicFramer::GetMinGoAwayFrameSize() + 256,
            QuicFramer::GetRetransmittableControlFrameSize(
                framer_.transport_version(), QuicFrame(&goaway)));

  QuicWindowUpdateFrame window_update(3, 3, 1024);
  EXPECT_EQ(QuicFramer::GetWindowUpdateFrameSize(framer_.transport_version(),
                                                 window_update),
            QuicFramer::GetRetransmittableControlFrameSize(
                framer_.transport_version(), QuicFrame(&window_update)));

  QuicBlockedFrame blocked(4, 3, 1024);
  EXPECT_EQ(
      QuicFramer::GetBlockedFrameSize(framer_.transport_version(), blocked),
      QuicFramer::GetRetransmittableControlFrameSize(
          framer_.transport_version(), QuicFrame(&blocked)));

  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }
  QuicApplicationCloseFrame application_close;
  EXPECT_EQ(QuicFramer::GetMinApplicationCloseFrameSize(
                framer_.transport_version(), application_close),
            QuicFramer::GetRetransmittableControlFrameSize(
                framer_.transport_version(), QuicFrame(&application_close)));

  QuicNewConnectionIdFrame new_connection_id(5, 42, 1, 101111);
  EXPECT_EQ(QuicFramer::GetNewConnectionIdFrameSize(new_connection_id),
            QuicFramer::GetRetransmittableControlFrameSize(
                framer_.transport_version(), QuicFrame(&new_connection_id)));

  QuicMaxStreamIdFrame max_stream_id(6, 3);
  EXPECT_EQ(QuicFramer::GetMaxStreamIdFrameSize(framer_.transport_version(),
                                                max_stream_id),
            QuicFramer::GetRetransmittableControlFrameSize(
                framer_.transport_version(), QuicFrame(max_stream_id)));

  QuicStreamIdBlockedFrame stream_id_blocked(7, 3);
  EXPECT_EQ(QuicFramer::GetStreamIdBlockedFrameSize(framer_.transport_version(),
                                                    stream_id_blocked),
            QuicFramer::GetRetransmittableControlFrameSize(
                framer_.transport_version(), QuicFrame(stream_id_blocked)));

  QuicPathFrameBuffer buffer = {
      {0x80, 0x91, 0xa2, 0xb3, 0xc4, 0xd5, 0xe5, 0xf7}};
  QuicPathResponseFrame path_response_frame(8, buffer);
  EXPECT_EQ(QuicFramer::GetPathResponseFrameSize(path_response_frame),
            QuicFramer::GetRetransmittableControlFrameSize(
                framer_.transport_version(), QuicFrame(&path_response_frame)));

  QuicPathChallengeFrame path_challenge_frame(9, buffer);
  EXPECT_EQ(QuicFramer::GetPathChallengeFrameSize(path_challenge_frame),
            QuicFramer::GetRetransmittableControlFrameSize(
                framer_.transport_version(), QuicFrame(&path_challenge_frame)));

  QuicStopSendingFrame stop_sending_frame(10, 3, 20);
  EXPECT_EQ(QuicFramer::GetStopSendingFrameSize(stop_sending_frame),
            QuicFramer::GetRetransmittableControlFrameSize(
                framer_.transport_version(), QuicFrame(&stop_sending_frame)));
}

// A set of tests to ensure that bad frame-type encodings
// are properly detected and handled.
// First, four tests to see that unknown frame types generate
// a QUIC_INVALID_FRAME_DATA error with detailed information
// "Illegal frame type." This regardless of the encoding of the type
// (1/2/4/8 bytes).
// This only for version 99.
TEST_P(QuicFramerTest, IetfFrameTypeEncodingErrorUnknown1Byte) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }
  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (unknown value, single-byte encoding)
      {"",
       {0x38}}
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));

  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_INVALID_FRAME_DATA, framer_.error());
  EXPECT_EQ("Illegal frame type.", framer_.detailed_error());
}

TEST_P(QuicFramerTest, IetfFrameTypeEncodingErrorUnknown2Bytes) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (unknown value, two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x01, 0x38}}
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));

  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_INVALID_FRAME_DATA, framer_.error());
  EXPECT_EQ("Illegal frame type.", framer_.detailed_error());
}

TEST_P(QuicFramerTest, IetfFrameTypeEncodingErrorUnknown4Bytes) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (unknown value, four-byte encoding)
      {"",
       {kVarInt62FourBytes + 0x01, 0x00, 0x00, 0x38}}
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));

  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_INVALID_FRAME_DATA, framer_.error());
  EXPECT_EQ("Illegal frame type.", framer_.detailed_error());
}

TEST_P(QuicFramerTest, IetfFrameTypeEncodingErrorUnknown8Bytes) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }
  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (unknown value, eight-byte encoding)
      {"",
       {kVarInt62EightBytes + 0x01, 0x00, 0x00, 0x01, 0x02, 0x34, 0x56, 0x38}}
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));

  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_INVALID_FRAME_DATA, framer_.error());
  EXPECT_EQ("Illegal frame type.", framer_.detailed_error());
}

// Three tests to check that known frame types that are not minimally
// encoded generate IETF_QUIC_PROTOCOL_VIOLATION errors with detailed
// information "Frame type not minimally encoded."
// Look at the frame-type encoded in 2, 4, and 8 bytes.
TEST_P(QuicFramerTest, IetfFrameTypeEncodingErrorKnown2Bytes) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (Blocked, two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x08}}
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));

  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(IETF_QUIC_PROTOCOL_VIOLATION, framer_.error());
  EXPECT_EQ("Frame type not minimally encoded.", framer_.detailed_error());
}

TEST_P(QuicFramerTest, IetfFrameTypeEncodingErrorKnown4Bytes) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (Blocked, four-byte encoding)
      {"",
       {kVarInt62FourBytes + 0x00, 0x00, 0x00, 0x08}}
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));

  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(IETF_QUIC_PROTOCOL_VIOLATION, framer_.error());
  EXPECT_EQ("Frame type not minimally encoded.", framer_.detailed_error());
}

TEST_P(QuicFramerTest, IetfFrameTypeEncodingErrorKnown8Bytes) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }
  // clang-format off
  PacketFragments packet = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (Blocked, eight-byte encoding)
      {"",
       {kVarInt62EightBytes + 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08}}
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet));

  EXPECT_FALSE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(IETF_QUIC_PROTOCOL_VIOLATION, framer_.error());
  EXPECT_EQ("Frame type not minimally encoded.", framer_.detailed_error());
}

// Tests to check that all known OETF frame types that are not minimally
// encoded generate IETF_QUIC_PROTOCOL_VIOLATION errors with detailed
// information "Frame type not minimally encoded."
// Just look at 2-byte encoding.
TEST_P(QuicFramerTest, IetfFrameTypeEncodingErrorKnown2BytesAllTypes) {
  // This test only for version 99.
  if (framer_.transport_version() != QUIC_VERSION_99) {
    return;
  }

  // clang-format off
  PacketFragments packets[] = {
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x00}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x01}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x02}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x03}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x04}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x05}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x06}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x07}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x08}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x09}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x0a}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x0b}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x0c}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x0d}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x0e}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x0f}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x10}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x11}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x12}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x13}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x14}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x15}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x16}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x17}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x18}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x20}}
    },
    {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x9A, 0xBC}},
      // frame type (two-byte encoding)
      {"",
       {kVarInt62TwoBytes + 0x00, 0x21}}
    },
  };
  // clang-format on

  for (PacketFragments& packet : packets) {
    std::unique_ptr<QuicEncryptedPacket> encrypted(
        AssemblePacketFromFragments(packet));

    EXPECT_FALSE(framer_.ProcessPacket(*encrypted));

    EXPECT_EQ(IETF_QUIC_PROTOCOL_VIOLATION, framer_.error());
    EXPECT_EQ("Frame type not minimally encoded.", framer_.detailed_error());
  }
}

TEST_P(QuicFramerTest, RetireConnectionIdFrame) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // This frame is only for version 99.
    return;
  }
  // clang-format off
  PacketFragments packet99 = {
      // type (short header, 4 byte packet number)
      {"",
       {0x32}},
      // connection_id
      {"",
       {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}},
      // packet number
      {"",
       {0x12, 0x34, 0x56, 0x78}},
      // frame type (retire connection id frame)
      {"",
       {0x0d}},
      // Sequence number
      {"Unable to read retire connection ID frame sequence number.",
       {kVarInt62TwoBytes + 0x11, 0x22}}
  };
  // clang-format on

  std::unique_ptr<QuicEncryptedPacket> encrypted(
      AssemblePacketFromFragments(packet99));
  EXPECT_TRUE(framer_.ProcessPacket(*encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(
      *encrypted, !kIncludeVersion, !kIncludeDiversificationNonce,
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());

  EXPECT_EQ(0x1122u, visitor_.retire_connection_id_.sequence_number);

  ASSERT_EQ(0u, visitor_.ack_frames_.size());

  CheckFramingBoundaries(packet99, QUIC_INVALID_RETIRE_CONNECTION_ID_DATA);
}

TEST_P(QuicFramerTest, BuildRetireConnectionIdFramePacket) {
  if (framer_.transport_version() != QUIC_VERSION_99) {
    // This frame is only for version 99.
    return;
  }
  QuicPacketHeader header;
  header.destination_connection_id = kConnectionId;
  header.reset_flag = false;
  header.version_flag = false;
  header.packet_number = kPacketNumber;

  QuicRetireConnectionIdFrame frame;
  frame.sequence_number = 0x1122;

  QuicFrames frames = {QuicFrame(&frame)};

  // clang-format off
  unsigned char packet99[] = {
    // type (short header, 4 byte packet number)
    0x32,
    // connection_id
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    // packet number
    0x12, 0x34, 0x56, 0x78,

    // frame type (retire connection id frame)
    0x0d,
    // sequence number
    kVarInt62TwoBytes + 0x11, 0x22
  };
  // clang-format on

  std::unique_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet99),
                                      QUIC_ARRAYSIZE(packet99));
}

}  // namespace
}  // namespace test
}  // namespace quic
