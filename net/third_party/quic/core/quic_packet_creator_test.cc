// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/quic_packet_creator.h"

#include <cstdint>
#include <memory>
#include <ostream>

#include "base/macros.h"
#include "net/third_party/quic/core/crypto/null_encrypter.h"
#include "net/third_party/quic/core/crypto/quic_decrypter.h"
#include "net/third_party/quic/core/crypto/quic_encrypter.h"
#include "net/third_party/quic/core/quic_data_writer.h"
#include "net/third_party/quic/core/quic_pending_retransmission.h"
#include "net/third_party/quic/core/quic_simple_buffer_allocator.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/platform/api/quic_expect_bug.h"
#include "net/third_party/quic/platform/api/quic_socket_address.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"
#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/test_tools/quic_framer_peer.h"
#include "net/third_party/quic/test_tools/quic_packet_creator_peer.h"
#include "net/third_party/quic/test_tools/quic_test_utils.h"
#include "net/third_party/quic/test_tools/simple_data_producer.h"

using testing::_;
using testing::DoAll;
using testing::InSequence;
using testing::Invoke;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;

namespace quic {
namespace test {
namespace {

// Run tests with combinations of {ParsedQuicVersion,
// ToggleVersionSerialization}.
struct TestParams {
  TestParams(ParsedQuicVersion version, bool version_serialization)
      : version(version), version_serialization(version_serialization) {}

  friend std::ostream& operator<<(std::ostream& os, const TestParams& p) {
    os << "{ version: " << ParsedQuicVersionToString(p.version)
       << " include version: " << p.version_serialization << " }";
    return os;
  }

  ParsedQuicVersion version;
  bool version_serialization;
};

// Constructs various test permutations.
std::vector<TestParams> GetTestParams() {
  std::vector<TestParams> params;
  ParsedQuicVersionVector all_supported_versions = AllSupportedVersions();
  for (size_t i = 0; i < all_supported_versions.size(); ++i) {
    params.push_back(TestParams(all_supported_versions[i], true));
    params.push_back(TestParams(all_supported_versions[i], false));
  }
  params.push_back(TestParams(all_supported_versions[0], true));
  params.push_back(TestParams(all_supported_versions[0], true));
  return params;
}

class TestPacketCreator : public QuicPacketCreator {
 public:
  TestPacketCreator(QuicConnectionId connection_id,
                    QuicFramer* framer,
                    DelegateInterface* delegate,
                    SimpleDataProducer* producer)
      : QuicPacketCreator(connection_id, framer, delegate),
        producer_(producer),
        version_(framer->transport_version()) {}

  bool ConsumeData(QuicStreamId id,
                   const struct iovec* iov,
                   int iov_count,
                   size_t total_length,
                   size_t iov_offset,
                   QuicStreamOffset offset,
                   bool fin,
                   bool needs_full_padding,
                   QuicFrame* frame) {
    // Save data before data is consumed.
    QuicByteCount data_length = total_length - iov_offset;
    if (data_length > 0) {
      producer_->SaveStreamData(id, iov, iov_count, iov_offset, offset,
                                data_length);
    }
    return QuicPacketCreator::ConsumeData(id, data_length, iov_offset, offset,
                                          fin, needs_full_padding, frame);
  }

  void StopSendingVersion() {
    if (version_ > QUIC_VERSION_43) {
      set_encryption_level(ENCRYPTION_FORWARD_SECURE);
      return;
    }
    QuicPacketCreator::StopSendingVersion();
  }

  SimpleDataProducer* producer_;
  QuicTransportVersion version_;
};

class QuicPacketCreatorTest : public QuicTestWithParam<TestParams> {
 public:
  void ClearSerializedPacketForTests(SerializedPacket* serialized_packet) {
    if (serialized_packet == nullptr) {
      return;
    }
    ClearSerializedPacket(serialized_packet);
  }

  void SaveSerializedPacket(SerializedPacket* serialized_packet) {
    if (serialized_packet == nullptr) {
      return;
    }
    delete[] serialized_packet_.encrypted_buffer;
    serialized_packet_ = *serialized_packet;
    serialized_packet_.encrypted_buffer = CopyBuffer(*serialized_packet);
    serialized_packet->retransmittable_frames.clear();
  }

  void DeleteSerializedPacket() {
    delete[] serialized_packet_.encrypted_buffer;
    serialized_packet_.encrypted_buffer = nullptr;
    ClearSerializedPacket(&serialized_packet_);
  }

 protected:
  QuicPacketCreatorTest()
      : server_framer_(SupportedVersions(GetParam().version),
                       QuicTime::Zero(),
                       Perspective::IS_SERVER),
        client_framer_(SupportedVersions(GetParam().version),
                       QuicTime::Zero(),
                       Perspective::IS_CLIENT),
        connection_id_(2),
        data_("foo"),
        creator_(connection_id_, &client_framer_, &delegate_, &producer_),
        serialized_packet_(creator_.NoPacket()) {
    EXPECT_CALL(delegate_, GetPacketBuffer()).WillRepeatedly(Return(nullptr));
    creator_.SetEncrypter(ENCRYPTION_INITIAL, QuicMakeUnique<NullEncrypter>(
                                                  Perspective::IS_CLIENT));
    creator_.SetEncrypter(
        ENCRYPTION_FORWARD_SECURE,
        QuicMakeUnique<NullEncrypter>(Perspective::IS_CLIENT));
    client_framer_.set_visitor(&framer_visitor_);
    server_framer_.set_visitor(&framer_visitor_);
    client_framer_.set_data_producer(&producer_);
  }

  ~QuicPacketCreatorTest() override {
    delete[] serialized_packet_.encrypted_buffer;
    ClearSerializedPacket(&serialized_packet_);
  }

  SerializedPacket SerializeAllFrames(const QuicFrames& frames) {
    SerializedPacket packet = QuicPacketCreatorPeer::SerializeAllFrames(
        &creator_, frames, buffer_, kMaxPacketSize);
    EXPECT_EQ(QuicPacketCreatorPeer::GetEncryptionLevel(&creator_),
              packet.encryption_level);
    return packet;
  }

  void ProcessPacket(const SerializedPacket& packet) {
    QuicEncryptedPacket encrypted_packet(packet.encrypted_buffer,
                                         packet.encrypted_length);
    server_framer_.ProcessPacket(encrypted_packet);
  }

  void CheckStreamFrame(const QuicFrame& frame,
                        QuicStreamId stream_id,
                        const QuicString& data,
                        QuicStreamOffset offset,
                        bool fin) {
    EXPECT_EQ(STREAM_FRAME, frame.type);
    EXPECT_EQ(stream_id, frame.stream_frame.stream_id);
    char buf[kMaxPacketSize];
    QuicDataWriter writer(kMaxPacketSize, buf, HOST_BYTE_ORDER);
    if (frame.stream_frame.data_length > 0) {
      producer_.WriteStreamData(stream_id, frame.stream_frame.offset,
                                frame.stream_frame.data_length, &writer);
    }
    EXPECT_EQ(data, QuicStringPiece(buf, frame.stream_frame.data_length));
    EXPECT_EQ(offset, frame.stream_frame.offset);
    EXPECT_EQ(fin, frame.stream_frame.fin);
  }

  // Returns the number of bytes consumed by the header of packet, including
  // the version.
  size_t GetPacketHeaderOverhead(QuicTransportVersion version) {
    return GetPacketHeaderSize(
        version, creator_.GetDestinationConnectionIdLength(),
        creator_.GetSourceConnectionIdLength(),
        QuicPacketCreatorPeer::SendVersionInPacket(&creator_),
        !kIncludeDiversificationNonce,
        QuicPacketCreatorPeer::GetPacketNumberLength(&creator_));
  }

  // Returns the number of bytes of overhead that will be added to a packet
  // of maximum length.
  size_t GetEncryptionOverhead() {
    return creator_.max_packet_length() -
           client_framer_.GetMaxPlaintextSize(creator_.max_packet_length());
  }

  // Returns the number of bytes consumed by the non-data fields of a stream
  // frame, assuming it is the last frame in the packet
  size_t GetStreamFrameOverhead(QuicTransportVersion version) {
    return QuicFramer::GetMinStreamFrameSize(
        version, GetNthClientInitiatedStreamId(1), kOffset, true,
        /* data_length= */ 0);
  }

  QuicPendingRetransmission CreateRetransmission(
      const QuicFrames& retransmittable_frames,
      bool has_crypto_handshake,
      int num_padding_bytes,
      EncryptionLevel encryption_level,
      QuicPacketNumberLength packet_number_length) {
    return QuicPendingRetransmission(
        1u, NOT_RETRANSMISSION, retransmittable_frames, has_crypto_handshake,
        num_padding_bytes, encryption_level, packet_number_length);
  }

  bool IsDefaultTestConfiguration() {
    TestParams p = GetParam();
    return p.version == AllSupportedVersions()[0] && p.version_serialization;
  }

  QuicStreamId GetNthClientInitiatedStreamId(int n) const {
    return QuicUtils::GetHeadersStreamId(creator_.transport_version()) + n * 2;
  }

  static const QuicStreamOffset kOffset = 0u;

  char buffer_[kMaxPacketSize];
  QuicFrames frames_;
  QuicFramer server_framer_;
  QuicFramer client_framer_;
  StrictMock<MockFramerVisitor> framer_visitor_;
  StrictMock<MockPacketCreatorDelegate> delegate_;
  QuicConnectionId connection_id_;
  QuicString data_;
  struct iovec iov_;
  TestPacketCreator creator_;
  SerializedPacket serialized_packet_;
  SimpleDataProducer producer_;
};

// Run all packet creator tests with all supported versions of QUIC, and with
// and without version in the packet header, as well as doing a run for each
// length of truncated connection id.
INSTANTIATE_TEST_CASE_P(QuicPacketCreatorTests,
                        QuicPacketCreatorTest,
                        ::testing::ValuesIn(GetTestParams()));

TEST_P(QuicPacketCreatorTest, SerializeFrames) {
  for (int i = ENCRYPTION_NONE; i < NUM_ENCRYPTION_LEVELS; ++i) {
    EncryptionLevel level = static_cast<EncryptionLevel>(i);
    creator_.set_encryption_level(level);
    frames_.push_back(QuicFrame(new QuicAckFrame()));
    frames_.push_back(QuicFrame(QuicStreamFrame(
        QuicUtils::GetCryptoStreamId(client_framer_.transport_version()), false,
        0u, QuicStringPiece())));
    frames_.push_back(QuicFrame(QuicStreamFrame(
        QuicUtils::GetCryptoStreamId(client_framer_.transport_version()), true,
        0u, QuicStringPiece())));
    SerializedPacket serialized = SerializeAllFrames(frames_);
    EXPECT_EQ(level, serialized.encryption_level);
    delete frames_[0].ack_frame;
    frames_.clear();

    {
      InSequence s;
      EXPECT_CALL(framer_visitor_, OnPacket());
      EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
      EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
      EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_));
      EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
      EXPECT_CALL(framer_visitor_, OnAckFrameStart(_, _))
          .WillOnce(Return(true));
      // This test includes an ack frame with largest_acked == 0 and
      // the size of the first ack-block == 1 (serialized as
      // 0). This is an invalid format for pre-version99, valid
      // for version 99.
      if (client_framer_.transport_version() != QUIC_VERSION_99) {
        // pre-version 99; ensure that the error is gracefully
        // handled.
        EXPECT_CALL(framer_visitor_, OnAckRange(1, 1)).WillOnce(Return(true));
        EXPECT_CALL(framer_visitor_, OnAckFrameEnd(1)).WillOnce(Return(true));
      } else {
        // version 99; ensure that the correct packet is signalled
        // properly.
        EXPECT_CALL(framer_visitor_, OnAckRange(0, 1)).WillOnce(Return(true));
        EXPECT_CALL(framer_visitor_, OnAckFrameEnd(0)).WillOnce(Return(true));
      }
      EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
      EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
      EXPECT_CALL(framer_visitor_, OnPacketComplete());
    }
    ProcessPacket(serialized);
  }
}

TEST_P(QuicPacketCreatorTest, ReserializeFramesWithSequenceNumberLength) {
  if (client_framer_.transport_version() > QUIC_VERSION_43) {
    creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  }
  // If the original packet number length, the current packet number
  // length, and the configured send packet number length are different, the
  // retransmit must sent with the original length and the others do not change.
  QuicPacketCreatorPeer::SetPacketNumberLength(&creator_,
                                               PACKET_2BYTE_PACKET_NUMBER);
  QuicStreamFrame stream_frame(
      QuicUtils::GetCryptoStreamId(client_framer_.transport_version()),
      /*fin=*/false, 0u, QuicStringPiece());
  QuicFrames frames;
  frames.push_back(QuicFrame(stream_frame));
  char buffer[kMaxPacketSize];
  QuicPendingRetransmission retransmission(CreateRetransmission(
      frames, true /* has_crypto_handshake */, -1 /* needs full padding */,
      ENCRYPTION_NONE, PACKET_4BYTE_PACKET_NUMBER));
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
  creator_.ReserializeAllFrames(retransmission, buffer, kMaxPacketSize);
  // The packet number length is updated after every packet is sent,
  // so there is no need to restore the old length after sending.
  EXPECT_EQ(PACKET_4BYTE_PACKET_NUMBER,
            QuicPacketCreatorPeer::GetPacketNumberLength(&creator_));
  EXPECT_EQ(PACKET_4BYTE_PACKET_NUMBER,
            serialized_packet_.packet_number_length);

  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
    EXPECT_CALL(framer_visitor_, OnPaddingFrame(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(serialized_packet_);
}

TEST_P(QuicPacketCreatorTest, ReserializeCryptoFrameWithForwardSecurity) {
  QuicStreamFrame stream_frame(
      QuicUtils::GetCryptoStreamId(client_framer_.transport_version()),
      /*fin=*/false, 0u, QuicStringPiece());
  QuicFrames frames;
  frames.push_back(QuicFrame(stream_frame));
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  char buffer[kMaxPacketSize];
  QuicPendingRetransmission retransmission(CreateRetransmission(
      frames, true /* has_crypto_handshake */, -1 /* needs full padding */,
      ENCRYPTION_NONE,
      QuicPacketCreatorPeer::GetPacketNumberLength(&creator_)));
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
  creator_.ReserializeAllFrames(retransmission, buffer, kMaxPacketSize);
  EXPECT_EQ(ENCRYPTION_NONE, serialized_packet_.encryption_level);
}

TEST_P(QuicPacketCreatorTest, ReserializeFrameWithForwardSecurity) {
  QuicStreamFrame stream_frame(0u, /*fin=*/false, 0u, QuicStringPiece());
  QuicFrames frames;
  frames.push_back(QuicFrame(stream_frame));
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  char buffer[kMaxPacketSize];
  QuicPendingRetransmission retransmission(CreateRetransmission(
      frames, false /* has_crypto_handshake */, 0 /* no padding */,
      ENCRYPTION_NONE,
      QuicPacketCreatorPeer::GetPacketNumberLength(&creator_)));
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
  creator_.ReserializeAllFrames(retransmission, buffer, kMaxPacketSize);
  EXPECT_EQ(ENCRYPTION_FORWARD_SECURE, serialized_packet_.encryption_level);
}

TEST_P(QuicPacketCreatorTest, ReserializeFramesWithFullPadding) {
  QuicFrame frame;
  MakeIOVector("fake handshake message data", &iov_);
  producer_.SaveStreamData(
      QuicUtils::GetCryptoStreamId(client_framer_.transport_version()), &iov_,
      1u, 0u, 0u, iov_.iov_len);
  QuicPacketCreatorPeer::CreateStreamFrame(
      &creator_,
      QuicUtils::GetCryptoStreamId(client_framer_.transport_version()),
      iov_.iov_len, 0u, 0u, false, &frame);
  QuicFrames frames;
  frames.push_back(frame);
  char buffer[kMaxPacketSize];
  QuicPendingRetransmission retransmission(CreateRetransmission(
      frames, true /* has_crypto_handshake */, -1 /* needs full padding */,
      ENCRYPTION_NONE,
      QuicPacketCreatorPeer::GetPacketNumberLength(&creator_)));
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
  creator_.ReserializeAllFrames(retransmission, buffer, kMaxPacketSize);
  EXPECT_EQ(kDefaultMaxPacketSize, serialized_packet_.encrypted_length);
}

TEST_P(QuicPacketCreatorTest, DoNotRetransmitPendingPadding) {
  QuicFrame frame;
  MakeIOVector("fake message data", &iov_);
  producer_.SaveStreamData(
      QuicUtils::GetCryptoStreamId(client_framer_.transport_version()), &iov_,
      1u, 0u, 0u, iov_.iov_len);
  QuicPacketCreatorPeer::CreateStreamFrame(
      &creator_,
      QuicUtils::GetCryptoStreamId(client_framer_.transport_version()),
      iov_.iov_len, 0u, 0u, false, &frame);

  const int kNumPaddingBytes1 = 4;
  int packet_size = 0;
  {
    QuicFrames frames;
    frames.push_back(frame);
    char buffer[kMaxPacketSize];
    QuicPendingRetransmission retransmission(CreateRetransmission(
        frames, false /* has_crypto_handshake */,
        kNumPaddingBytes1 /* padding bytes */, ENCRYPTION_NONE,
        QuicPacketCreatorPeer::GetPacketNumberLength(&creator_)));
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
    creator_.ReserializeAllFrames(retransmission, buffer, kMaxPacketSize);
    packet_size = serialized_packet_.encrypted_length;
  }

  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
    // Pending paddings are not retransmitted.
    EXPECT_CALL(framer_visitor_, OnPaddingFrame(_)).Times(0);
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(serialized_packet_);

  const int kNumPaddingBytes2 = 44;
  QuicFrames frames;
  frames.push_back(frame);
  char buffer[kMaxPacketSize];
  QuicPendingRetransmission retransmission(CreateRetransmission(
      frames, false /* has_crypto_handshake */,
      kNumPaddingBytes2 /* padding bytes */, ENCRYPTION_NONE,
      QuicPacketCreatorPeer::GetPacketNumberLength(&creator_)));
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
  creator_.ReserializeAllFrames(retransmission, buffer, kMaxPacketSize);

  EXPECT_EQ(packet_size, serialized_packet_.encrypted_length);
}

TEST_P(QuicPacketCreatorTest, ReserializeFramesWithFullPacketAndPadding) {
  const size_t overhead =
      GetPacketHeaderOverhead(client_framer_.transport_version()) +
      GetEncryptionOverhead() +
      GetStreamFrameOverhead(client_framer_.transport_version());
  size_t capacity = kDefaultMaxPacketSize - overhead;
  for (int delta = -5; delta <= 0; ++delta) {
    QuicString data(capacity + delta, 'A');
    size_t bytes_free = 0 - delta;

    QuicFrame frame;
    MakeIOVector(data, &iov_);
    SimpleDataProducer producer;
    producer.SaveStreamData(
        QuicUtils::GetCryptoStreamId(client_framer_.transport_version()), &iov_,
        1u, 0u, 0u, iov_.iov_len);
    QuicPacketCreatorPeer::framer(&creator_)->set_data_producer(&producer);
    QuicPacketCreatorPeer::CreateStreamFrame(
        &creator_,
        QuicUtils::GetCryptoStreamId(client_framer_.transport_version()),
        iov_.iov_len, 0, kOffset, false, &frame);
    QuicFrames frames;
    frames.push_back(frame);
    char buffer[kMaxPacketSize];
    QuicPendingRetransmission retransmission(CreateRetransmission(
        frames, true /* has_crypto_handshake */, -1 /* needs full padding */,
        ENCRYPTION_NONE,
        QuicPacketCreatorPeer::GetPacketNumberLength(&creator_)));
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
    creator_.ReserializeAllFrames(retransmission, buffer, kMaxPacketSize);

    // If there is not enough space in the packet to fit a padding frame
    // (1 byte) and to expand the stream frame (another 2 bytes) the packet
    // will not be padded.
    if (bytes_free < 3) {
      EXPECT_EQ(kDefaultMaxPacketSize - bytes_free,
                serialized_packet_.encrypted_length);
    } else {
      EXPECT_EQ(kDefaultMaxPacketSize, serialized_packet_.encrypted_length);
    }

    frames_.clear();
  }
}

TEST_P(QuicPacketCreatorTest, SerializeConnectionClose) {
  QuicConnectionCloseFrame frame;
  frame.error_code = QUIC_NO_ERROR;
  frame.error_details = "error";

  QuicFrames frames;
  frames.push_back(QuicFrame(&frame));
  SerializedPacket serialized = SerializeAllFrames(frames);
  EXPECT_EQ(ENCRYPTION_NONE, serialized.encryption_level);
  ASSERT_EQ(1u, serialized.packet_number);
  ASSERT_EQ(1u, creator_.packet_number());

  InSequence s;
  EXPECT_CALL(framer_visitor_, OnPacket());
  EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
  EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
  EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_));
  EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
  EXPECT_CALL(framer_visitor_, OnConnectionCloseFrame(_));
  EXPECT_CALL(framer_visitor_, OnPacketComplete());

  ProcessPacket(serialized);
}

TEST_P(QuicPacketCreatorTest, ConsumeData) {
  QuicFrame frame;
  MakeIOVector("test", &iov_);
  ASSERT_TRUE(creator_.ConsumeData(
      QuicUtils::GetCryptoStreamId(client_framer_.transport_version()), &iov_,
      1u, iov_.iov_len, 0u, 0u, false, false, &frame));
  size_t consumed = frame.stream_frame.data_length;
  EXPECT_EQ(4u, consumed);
  CheckStreamFrame(
      frame, QuicUtils::GetCryptoStreamId(client_framer_.transport_version()),
      "test", 0u, false);
  EXPECT_TRUE(creator_.HasPendingFrames());
}

TEST_P(QuicPacketCreatorTest, ConsumeDataFin) {
  QuicFrame frame;
  MakeIOVector("test", &iov_);
  ASSERT_TRUE(creator_.ConsumeData(
      QuicUtils::GetCryptoStreamId(client_framer_.transport_version()), &iov_,
      1u, iov_.iov_len, 0u, 0u, true, false, &frame));
  size_t consumed = frame.stream_frame.data_length;
  EXPECT_EQ(4u, consumed);
  CheckStreamFrame(
      frame, QuicUtils::GetCryptoStreamId(client_framer_.transport_version()),
      "test", 0u, true);
  EXPECT_TRUE(creator_.HasPendingFrames());
}

TEST_P(QuicPacketCreatorTest, ConsumeDataFinOnly) {
  QuicFrame frame;
  ASSERT_TRUE(creator_.ConsumeData(
      QuicUtils::GetCryptoStreamId(client_framer_.transport_version()), nullptr,
      0u, 0u, 0u, 0u, true, false, &frame));
  size_t consumed = frame.stream_frame.data_length;
  EXPECT_EQ(0u, consumed);
  CheckStreamFrame(
      frame, QuicUtils::GetCryptoStreamId(client_framer_.transport_version()),
      QuicString(), 0u, true);
  EXPECT_TRUE(creator_.HasPendingFrames());
}

TEST_P(QuicPacketCreatorTest, CreateAllFreeBytesForStreamFrames) {
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  const size_t overhead =
      GetPacketHeaderOverhead(client_framer_.transport_version()) +
      GetEncryptionOverhead();
  for (size_t i = overhead; i < overhead + 100; ++i) {
    creator_.SetMaxPacketLength(i);
    const bool should_have_room =
        i >
        overhead + GetStreamFrameOverhead(client_framer_.transport_version());
    ASSERT_EQ(should_have_room,
              creator_.HasRoomForStreamFrame(GetNthClientInitiatedStreamId(1),
                                             kOffset, /* data_size=*/0xffff));
    if (should_have_room) {
      QuicFrame frame;
      MakeIOVector("testdata", &iov_);
      EXPECT_CALL(delegate_, OnSerializedPacket(_))
          .WillRepeatedly(Invoke(
              this, &QuicPacketCreatorTest::ClearSerializedPacketForTests));
      ASSERT_TRUE(creator_.ConsumeData(GetNthClientInitiatedStreamId(1), &iov_,
                                       1u, iov_.iov_len, 0u, kOffset, false,
                                       false, &frame));
      size_t bytes_consumed = frame.stream_frame.data_length;
      EXPECT_LT(0u, bytes_consumed);
      creator_.Flush();
    }
  }
}

TEST_P(QuicPacketCreatorTest, StreamFrameConsumption) {
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  // Compute the total overhead for a single frame in packet.
  const size_t overhead =
      GetPacketHeaderOverhead(client_framer_.transport_version()) +
      GetEncryptionOverhead() +
      GetStreamFrameOverhead(client_framer_.transport_version());
  size_t capacity = kDefaultMaxPacketSize - overhead;
  // Now, test various sizes around this size.
  for (int delta = -5; delta <= 5; ++delta) {
    QuicString data(capacity + delta, 'A');
    size_t bytes_free = delta > 0 ? 0 : 0 - delta;
    QuicFrame frame;
    MakeIOVector(data, &iov_);
    ASSERT_TRUE(creator_.ConsumeData(GetNthClientInitiatedStreamId(1), &iov_,
                                     1u, iov_.iov_len, 0u, kOffset, false,
                                     false, &frame));

    // BytesFree() returns bytes available for the next frame, which will
    // be two bytes smaller since the stream frame would need to be grown.
    EXPECT_EQ(2u, creator_.ExpansionOnNewFrame());
    size_t expected_bytes_free = bytes_free < 3 ? 0 : bytes_free - 2;
    EXPECT_EQ(expected_bytes_free, creator_.BytesFree()) << "delta: " << delta;
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
    creator_.Flush();
    ASSERT_TRUE(serialized_packet_.encrypted_buffer);
    DeleteSerializedPacket();
  }
}

TEST_P(QuicPacketCreatorTest, CryptoStreamFramePacketPadding) {
  // Compute the total overhead for a single frame in packet.
  const size_t overhead =
      GetPacketHeaderOverhead(client_framer_.transport_version()) +
      GetEncryptionOverhead() +
      GetStreamFrameOverhead(client_framer_.transport_version());
  ASSERT_GT(kMaxPacketSize, overhead);
  size_t capacity = kDefaultMaxPacketSize - overhead;
  // Now, test various sizes around this size.
  for (int delta = -5; delta <= 5; ++delta) {
    QuicString data(capacity + delta, 'A');
    size_t bytes_free = delta > 0 ? 0 : 0 - delta;

    QuicFrame frame;
    MakeIOVector(data, &iov_);
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillRepeatedly(
            Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
    ASSERT_TRUE(creator_.ConsumeData(
        QuicUtils::GetCryptoStreamId(client_framer_.transport_version()), &iov_,
        1u, iov_.iov_len, 0u, kOffset, false, true, &frame));
    size_t bytes_consumed = frame.stream_frame.data_length;
    EXPECT_LT(0u, bytes_consumed);
    creator_.Flush();
    ASSERT_TRUE(serialized_packet_.encrypted_buffer);
    // If there is not enough space in the packet to fit a padding frame
    // (1 byte) and to expand the stream frame (another 2 bytes) the packet
    // will not be padded.
    if (bytes_free < 3) {
      EXPECT_EQ(kDefaultMaxPacketSize - bytes_free,
                serialized_packet_.encrypted_length);
    } else {
      EXPECT_EQ(kDefaultMaxPacketSize, serialized_packet_.encrypted_length);
    }
    DeleteSerializedPacket();
  }
}

TEST_P(QuicPacketCreatorTest, NonCryptoStreamFramePacketNonPadding) {
  creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  // Compute the total overhead for a single frame in packet.
  const size_t overhead =
      GetPacketHeaderOverhead(client_framer_.transport_version()) +
      GetEncryptionOverhead() +
      GetStreamFrameOverhead(client_framer_.transport_version());
  ASSERT_GT(kDefaultMaxPacketSize, overhead);
  size_t capacity = kDefaultMaxPacketSize - overhead;
  // Now, test various sizes around this size.
  for (int delta = -5; delta <= 5; ++delta) {
    QuicString data(capacity + delta, 'A');
    size_t bytes_free = delta > 0 ? 0 : 0 - delta;

    QuicFrame frame;
    MakeIOVector(data, &iov_);
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
    ASSERT_TRUE(creator_.ConsumeData(GetNthClientInitiatedStreamId(1), &iov_,
                                     1u, iov_.iov_len, 0u, kOffset, false,
                                     false, &frame));
    size_t bytes_consumed = frame.stream_frame.data_length;
    EXPECT_LT(0u, bytes_consumed);
    creator_.Flush();
    ASSERT_TRUE(serialized_packet_.encrypted_buffer);
    if (bytes_free > 0) {
      EXPECT_EQ(kDefaultMaxPacketSize - bytes_free,
                serialized_packet_.encrypted_length);
    } else {
      EXPECT_EQ(kDefaultMaxPacketSize, serialized_packet_.encrypted_length);
    }
    DeleteSerializedPacket();
  }
}

TEST_P(QuicPacketCreatorTest, SerializeVersionNegotiationPacket) {
  QuicFramerPeer::SetPerspective(&client_framer_, Perspective::IS_SERVER);
  ParsedQuicVersionVector versions;
  versions.push_back(test::QuicVersionMax());
  const bool ietf_quic = GetParam().version.transport_version > QUIC_VERSION_43;
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      creator_.SerializeVersionNegotiationPacket(ietf_quic, versions));

  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnVersionNegotiationPacket(_));
  }
  QuicFramerPeer::SetPerspective(&client_framer_, Perspective::IS_CLIENT);
  client_framer_.ProcessPacket(*encrypted);
}

TEST_P(QuicPacketCreatorTest, SerializeConnectivityProbingPacket) {
  for (int i = ENCRYPTION_NONE; i < NUM_ENCRYPTION_LEVELS; ++i) {
    EncryptionLevel level = static_cast<EncryptionLevel>(i);

    creator_.set_encryption_level(level);

    OwningSerializedPacketPointer encrypted;
    if (GetParam().version.transport_version == QUIC_VERSION_99) {
      QuicPathFrameBuffer payload = {
          {0xde, 0xad, 0xbe, 0xef, 0xba, 0xdc, 0x0f, 0xfe}};
      encrypted =
          creator_.SerializePathChallengeConnectivityProbingPacket(&payload);
    } else {
      encrypted = creator_.SerializeConnectivityProbingPacket();
    }
    {
      InSequence s;
      EXPECT_CALL(framer_visitor_, OnPacket());
      EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
      EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
      EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_));
      EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
      if (GetParam().version.transport_version == QUIC_VERSION_99) {
        EXPECT_CALL(framer_visitor_, OnPathChallengeFrame(_));
        EXPECT_CALL(framer_visitor_, OnPaddingFrame(_));
      } else {
        EXPECT_CALL(framer_visitor_, OnPingFrame(_));
        EXPECT_CALL(framer_visitor_, OnPaddingFrame(_));
      }
      EXPECT_CALL(framer_visitor_, OnPacketComplete());
    }
    // QuicFramerPeer::SetPerspective(&client_framer_, Perspective::IS_SERVER);
    server_framer_.ProcessPacket(QuicEncryptedPacket(
        encrypted->encrypted_buffer, encrypted->encrypted_length));
  }
}

TEST_P(QuicPacketCreatorTest, SerializePathChallengeProbePacket) {
  if (GetParam().version.transport_version != QUIC_VERSION_99) {
    return;
  }
  QuicPathFrameBuffer payload = {
      {0xde, 0xad, 0xbe, 0xef, 0xba, 0xdc, 0x0f, 0xee}};

  for (int i = ENCRYPTION_NONE; i < NUM_ENCRYPTION_LEVELS; ++i) {
    EncryptionLevel level = static_cast<EncryptionLevel>(i);

    creator_.set_encryption_level(level);

    OwningSerializedPacketPointer encrypted(
        creator_.SerializePathChallengeConnectivityProbingPacket(&payload));
    {
      InSequence s;
      EXPECT_CALL(framer_visitor_, OnPacket());
      EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
      EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
      EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_));
      EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
      EXPECT_CALL(framer_visitor_, OnPathChallengeFrame(_));
      EXPECT_CALL(framer_visitor_, OnPaddingFrame(_));
      EXPECT_CALL(framer_visitor_, OnPacketComplete());
    }
    // QuicFramerPeer::SetPerspective(&client_framer_, Perspective::IS_SERVER);
    server_framer_.ProcessPacket(QuicEncryptedPacket(
        encrypted->encrypted_buffer, encrypted->encrypted_length));
  }
}

TEST_P(QuicPacketCreatorTest, SerializePathResponseProbePacket1PayloadPadded) {
  if (GetParam().version.transport_version != QUIC_VERSION_99) {
    return;
  }
  QuicPathFrameBuffer payload0 = {
      {0xde, 0xad, 0xbe, 0xef, 0xba, 0xdc, 0x0f, 0xee}};

  for (int i = ENCRYPTION_NONE; i < NUM_ENCRYPTION_LEVELS; ++i) {
    EncryptionLevel level = static_cast<EncryptionLevel>(i);
    creator_.set_encryption_level(level);

    QuicDeque<QuicPathFrameBuffer> payloads;
    payloads.push_back(payload0);

    OwningSerializedPacketPointer encrypted(
        creator_.SerializePathResponseConnectivityProbingPacket(payloads,
                                                                true));
    {
      InSequence s;
      EXPECT_CALL(framer_visitor_, OnPacket());
      EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
      EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
      EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_));
      EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
      EXPECT_CALL(framer_visitor_, OnPathResponseFrame(_));
      EXPECT_CALL(framer_visitor_, OnPaddingFrame(_));
      EXPECT_CALL(framer_visitor_, OnPacketComplete());
    }
    server_framer_.ProcessPacket(QuicEncryptedPacket(
        encrypted->encrypted_buffer, encrypted->encrypted_length));
  }
}

TEST_P(QuicPacketCreatorTest,
       SerializePathResponseProbePacket1PayloadUnPadded) {
  if (GetParam().version.transport_version != QUIC_VERSION_99) {
    return;
  }
  QuicPathFrameBuffer payload0 = {
      {0xde, 0xad, 0xbe, 0xef, 0xba, 0xdc, 0x0f, 0xee}};

  for (int i = ENCRYPTION_NONE; i < NUM_ENCRYPTION_LEVELS; ++i) {
    EncryptionLevel level = static_cast<EncryptionLevel>(i);
    creator_.set_encryption_level(level);

    QuicDeque<QuicPathFrameBuffer> payloads;
    payloads.push_back(payload0);

    OwningSerializedPacketPointer encrypted(
        creator_.SerializePathResponseConnectivityProbingPacket(payloads,
                                                                false));
    {
      InSequence s;
      EXPECT_CALL(framer_visitor_, OnPacket());
      EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
      EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
      EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_));
      EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
      EXPECT_CALL(framer_visitor_, OnPathResponseFrame(_));
      EXPECT_CALL(framer_visitor_, OnPacketComplete());
    }
    server_framer_.ProcessPacket(QuicEncryptedPacket(
        encrypted->encrypted_buffer, encrypted->encrypted_length));
  }
}

TEST_P(QuicPacketCreatorTest, SerializePathResponseProbePacket2PayloadsPadded) {
  if (GetParam().version.transport_version != QUIC_VERSION_99) {
    return;
  }
  QuicPathFrameBuffer payload0 = {
      {0xde, 0xad, 0xbe, 0xef, 0xba, 0xdc, 0x0f, 0xee}};
  QuicPathFrameBuffer payload1 = {
      {0xad, 0xbe, 0xef, 0xba, 0xdc, 0x0f, 0xee, 0xde}};

  for (int i = ENCRYPTION_NONE; i < NUM_ENCRYPTION_LEVELS; ++i) {
    EncryptionLevel level = static_cast<EncryptionLevel>(i);
    creator_.set_encryption_level(level);

    QuicDeque<QuicPathFrameBuffer> payloads;
    payloads.push_back(payload0);
    payloads.push_back(payload1);

    OwningSerializedPacketPointer encrypted(
        creator_.SerializePathResponseConnectivityProbingPacket(payloads,
                                                                true));
    {
      InSequence s;
      EXPECT_CALL(framer_visitor_, OnPacket());
      EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
      EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
      EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_));
      EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
      EXPECT_CALL(framer_visitor_, OnPathResponseFrame(_)).Times(2);
      EXPECT_CALL(framer_visitor_, OnPaddingFrame(_));
      EXPECT_CALL(framer_visitor_, OnPacketComplete());
    }
    server_framer_.ProcessPacket(QuicEncryptedPacket(
        encrypted->encrypted_buffer, encrypted->encrypted_length));
  }
}

TEST_P(QuicPacketCreatorTest,
       SerializePathResponseProbePacket2PayloadsUnPadded) {
  if (GetParam().version.transport_version != QUIC_VERSION_99) {
    return;
  }
  QuicPathFrameBuffer payload0 = {
      {0xde, 0xad, 0xbe, 0xef, 0xba, 0xdc, 0x0f, 0xee}};
  QuicPathFrameBuffer payload1 = {
      {0xad, 0xbe, 0xef, 0xba, 0xdc, 0x0f, 0xee, 0xde}};

  for (int i = ENCRYPTION_NONE; i < NUM_ENCRYPTION_LEVELS; ++i) {
    EncryptionLevel level = static_cast<EncryptionLevel>(i);
    creator_.set_encryption_level(level);

    QuicDeque<QuicPathFrameBuffer> payloads;
    payloads.push_back(payload0);
    payloads.push_back(payload1);

    OwningSerializedPacketPointer encrypted(
        creator_.SerializePathResponseConnectivityProbingPacket(payloads,
                                                                false));
    {
      InSequence s;
      EXPECT_CALL(framer_visitor_, OnPacket());
      EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
      EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
      EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_));
      EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
      EXPECT_CALL(framer_visitor_, OnPathResponseFrame(_)).Times(2);
      EXPECT_CALL(framer_visitor_, OnPacketComplete());
    }
    server_framer_.ProcessPacket(QuicEncryptedPacket(
        encrypted->encrypted_buffer, encrypted->encrypted_length));
  }
}

TEST_P(QuicPacketCreatorTest, SerializePathResponseProbePacket3PayloadsPadded) {
  if (GetParam().version.transport_version != QUIC_VERSION_99) {
    return;
  }
  QuicPathFrameBuffer payload0 = {
      {0xde, 0xad, 0xbe, 0xef, 0xba, 0xdc, 0x0f, 0xee}};
  QuicPathFrameBuffer payload1 = {
      {0xad, 0xbe, 0xef, 0xba, 0xdc, 0x0f, 0xee, 0xde}};
  QuicPathFrameBuffer payload2 = {
      {0xbe, 0xef, 0xba, 0xdc, 0x0f, 0xee, 0xde, 0xad}};

  for (int i = ENCRYPTION_NONE; i < NUM_ENCRYPTION_LEVELS; ++i) {
    EncryptionLevel level = static_cast<EncryptionLevel>(i);
    creator_.set_encryption_level(level);

    QuicDeque<QuicPathFrameBuffer> payloads;
    payloads.push_back(payload0);
    payloads.push_back(payload1);
    payloads.push_back(payload2);

    OwningSerializedPacketPointer encrypted(
        creator_.SerializePathResponseConnectivityProbingPacket(payloads,
                                                                true));
    {
      InSequence s;
      EXPECT_CALL(framer_visitor_, OnPacket());
      EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
      EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
      EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_));
      EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
      EXPECT_CALL(framer_visitor_, OnPathResponseFrame(_)).Times(3);
      EXPECT_CALL(framer_visitor_, OnPaddingFrame(_));
      EXPECT_CALL(framer_visitor_, OnPacketComplete());
    }
    server_framer_.ProcessPacket(QuicEncryptedPacket(
        encrypted->encrypted_buffer, encrypted->encrypted_length));
  }
}

TEST_P(QuicPacketCreatorTest,
       SerializePathResponseProbePacket3PayloadsUnpadded) {
  if (GetParam().version.transport_version != QUIC_VERSION_99) {
    return;
  }
  QuicPathFrameBuffer payload0 = {
      {0xde, 0xad, 0xbe, 0xef, 0xba, 0xdc, 0x0f, 0xee}};
  QuicPathFrameBuffer payload1 = {
      {0xad, 0xbe, 0xef, 0xba, 0xdc, 0x0f, 0xee, 0xde}};
  QuicPathFrameBuffer payload2 = {
      {0xbe, 0xef, 0xba, 0xdc, 0x0f, 0xee, 0xde, 0xad}};

  for (int i = ENCRYPTION_NONE; i < NUM_ENCRYPTION_LEVELS; ++i) {
    EncryptionLevel level = static_cast<EncryptionLevel>(i);
    creator_.set_encryption_level(level);

    QuicDeque<QuicPathFrameBuffer> payloads;
    payloads.push_back(payload0);
    payloads.push_back(payload1);
    payloads.push_back(payload2);

    OwningSerializedPacketPointer encrypted(
        creator_.SerializePathResponseConnectivityProbingPacket(payloads,
                                                                false));
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    EXPECT_CALL(framer_visitor_, OnPathResponseFrame(_)).Times(3);
    EXPECT_CALL(framer_visitor_, OnPacketComplete());

    server_framer_.ProcessPacket(QuicEncryptedPacket(
        encrypted->encrypted_buffer, encrypted->encrypted_length));
  }
}

TEST_P(QuicPacketCreatorTest, UpdatePacketSequenceNumberLengthLeastAwaiting) {
  if (GetParam().version.transport_version > QUIC_VERSION_43) {
    EXPECT_EQ(PACKET_4BYTE_PACKET_NUMBER,
              QuicPacketCreatorPeer::GetPacketNumberLength(&creator_));
    creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  } else {
    EXPECT_EQ(PACKET_1BYTE_PACKET_NUMBER,
              QuicPacketCreatorPeer::GetPacketNumberLength(&creator_));
  }

  QuicPacketCreatorPeer::SetPacketNumber(&creator_, 64);
  creator_.UpdatePacketNumberLength(2, 10000 / kDefaultMaxPacketSize);
  EXPECT_EQ(PACKET_1BYTE_PACKET_NUMBER,
            QuicPacketCreatorPeer::GetPacketNumberLength(&creator_));

  QuicPacketCreatorPeer::SetPacketNumber(&creator_, 64 * 256);
  creator_.UpdatePacketNumberLength(2, 10000 / kDefaultMaxPacketSize);
  EXPECT_EQ(PACKET_2BYTE_PACKET_NUMBER,
            QuicPacketCreatorPeer::GetPacketNumberLength(&creator_));

  QuicPacketCreatorPeer::SetPacketNumber(&creator_, 64 * 256 * 256);
  creator_.UpdatePacketNumberLength(2, 10000 / kDefaultMaxPacketSize);
  EXPECT_EQ(PACKET_4BYTE_PACKET_NUMBER,
            QuicPacketCreatorPeer::GetPacketNumberLength(&creator_));

  QuicPacketCreatorPeer::SetPacketNumber(&creator_,
                                         UINT64_C(64) * 256 * 256 * 256 * 256);
  creator_.UpdatePacketNumberLength(2, 10000 / kDefaultMaxPacketSize);
  EXPECT_EQ(PACKET_6BYTE_PACKET_NUMBER,
            QuicPacketCreatorPeer::GetPacketNumberLength(&creator_));
}

TEST_P(QuicPacketCreatorTest, UpdatePacketSequenceNumberLengthCwnd) {
  if (GetParam().version.transport_version > QUIC_VERSION_43) {
    EXPECT_EQ(PACKET_4BYTE_PACKET_NUMBER,
              QuicPacketCreatorPeer::GetPacketNumberLength(&creator_));
    creator_.set_encryption_level(ENCRYPTION_FORWARD_SECURE);
  } else {
    EXPECT_EQ(PACKET_1BYTE_PACKET_NUMBER,
              QuicPacketCreatorPeer::GetPacketNumberLength(&creator_));
  }

  creator_.UpdatePacketNumberLength(1, 10000 / kDefaultMaxPacketSize);
  EXPECT_EQ(PACKET_1BYTE_PACKET_NUMBER,
            QuicPacketCreatorPeer::GetPacketNumberLength(&creator_));

  creator_.UpdatePacketNumberLength(1, 10000 * 256 / kDefaultMaxPacketSize);
  EXPECT_EQ(PACKET_2BYTE_PACKET_NUMBER,
            QuicPacketCreatorPeer::GetPacketNumberLength(&creator_));

  creator_.UpdatePacketNumberLength(1,
                                    10000 * 256 * 256 / kDefaultMaxPacketSize);
  EXPECT_EQ(PACKET_4BYTE_PACKET_NUMBER,
            QuicPacketCreatorPeer::GetPacketNumberLength(&creator_));

  creator_.UpdatePacketNumberLength(
      1, UINT64_C(1000) * 256 * 256 * 256 * 256 / kDefaultMaxPacketSize);
  EXPECT_EQ(PACKET_6BYTE_PACKET_NUMBER,
            QuicPacketCreatorPeer::GetPacketNumberLength(&creator_));
}

TEST_P(QuicPacketCreatorTest, SerializeFrame) {
  if (!GetParam().version_serialization) {
    creator_.StopSendingVersion();
  }
  frames_.push_back(QuicFrame(QuicStreamFrame(
      QuicUtils::GetCryptoStreamId(client_framer_.transport_version()), false,
      0u, QuicStringPiece())));
  SerializedPacket serialized = SerializeAllFrames(frames_);

  QuicPacketHeader header;
  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_))
        .WillOnce(DoAll(SaveArg<0>(&header), Return(true)));
    EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(serialized);
  EXPECT_EQ(GetParam().version_serialization, header.version_flag);
}

TEST_P(QuicPacketCreatorTest, ConsumeDataLargerThanOneStreamFrame) {
  if (!GetParam().version_serialization) {
    creator_.StopSendingVersion();
  }
  // A string larger than fits into a frame.
  size_t payload_length;
  creator_.SetMaxPacketLength(GetPacketLengthForOneStream(
      client_framer_.transport_version(),
      QuicPacketCreatorPeer::SendVersionInPacket(&creator_),
      !kIncludeDiversificationNonce,
      creator_.GetDestinationConnectionIdLength(),
      creator_.GetSourceConnectionIdLength(),
      QuicPacketCreatorPeer::GetPacketNumberLength(&creator_),
      &payload_length));
  QuicFrame frame;
  const QuicString too_long_payload(payload_length * 2, 'a');
  MakeIOVector(too_long_payload, &iov_);
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
  ASSERT_TRUE(creator_.ConsumeData(
      QuicUtils::GetCryptoStreamId(client_framer_.transport_version()), &iov_,
      1u, iov_.iov_len, 0u, 0u, true, false, &frame));
  size_t consumed = frame.stream_frame.data_length;
  EXPECT_EQ(payload_length, consumed);
  const QuicString payload(payload_length, 'a');
  CheckStreamFrame(
      frame, QuicUtils::GetCryptoStreamId(client_framer_.transport_version()),
      payload, 0u, false);
  creator_.Flush();
  DeleteSerializedPacket();
}

TEST_P(QuicPacketCreatorTest, AddFrameAndFlush) {
  if (!GetParam().version_serialization) {
    creator_.StopSendingVersion();
  }
  const size_t max_plaintext_size =
      client_framer_.GetMaxPlaintextSize(creator_.max_packet_length());
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingStreamFramesOfStream(
      QuicUtils::GetCryptoStreamId(client_framer_.transport_version())));
  EXPECT_EQ(max_plaintext_size -
                GetPacketHeaderSize(
                    client_framer_.transport_version(),
                    creator_.GetDestinationConnectionIdLength(),
                    creator_.GetSourceConnectionIdLength(),
                    QuicPacketCreatorPeer::SendVersionInPacket(&creator_),
                    !kIncludeDiversificationNonce,
                    QuicPacketCreatorPeer::GetPacketNumberLength(&creator_)),
            creator_.BytesFree());

  // Add a variety of frame types and then a padding frame.
  QuicAckFrame ack_frame(InitAckFrame(10u));
  EXPECT_TRUE(creator_.AddSavedFrame(QuicFrame(&ack_frame)));
  EXPECT_TRUE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingStreamFramesOfStream(
      QuicUtils::GetCryptoStreamId(client_framer_.transport_version())));

  QuicFrame frame;
  MakeIOVector("test", &iov_);
  ASSERT_TRUE(creator_.ConsumeData(
      QuicUtils::GetCryptoStreamId(client_framer_.transport_version()), &iov_,
      1u, iov_.iov_len, 0u, 0u, false, false, &frame));
  size_t consumed = frame.stream_frame.data_length;
  EXPECT_EQ(4u, consumed);
  EXPECT_TRUE(creator_.HasPendingFrames());
  EXPECT_TRUE(creator_.HasPendingStreamFramesOfStream(
      QuicUtils::GetCryptoStreamId(client_framer_.transport_version())));

  QuicPaddingFrame padding_frame;
  EXPECT_TRUE(creator_.AddSavedFrame(QuicFrame(padding_frame)));
  EXPECT_TRUE(creator_.HasPendingFrames());
  EXPECT_EQ(0u, creator_.BytesFree());

  // Packet is full. Creator will flush.
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
  EXPECT_FALSE(creator_.AddSavedFrame(QuicFrame(&ack_frame)));

  // Ensure the packet is successfully created.
  ASSERT_TRUE(serialized_packet_.encrypted_buffer);
  ASSERT_FALSE(serialized_packet_.retransmittable_frames.empty());
  const QuicFrames& retransmittable = serialized_packet_.retransmittable_frames;
  ASSERT_EQ(1u, retransmittable.size());
  EXPECT_EQ(STREAM_FRAME, retransmittable[0].type);
  EXPECT_TRUE(serialized_packet_.has_ack);
  EXPECT_EQ(10u, serialized_packet_.largest_acked);
  DeleteSerializedPacket();

  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_FALSE(creator_.HasPendingStreamFramesOfStream(
      QuicUtils::GetCryptoStreamId(client_framer_.transport_version())));
  EXPECT_EQ(max_plaintext_size -
                GetPacketHeaderSize(
                    client_framer_.transport_version(),
                    creator_.GetDestinationConnectionIdLength(),
                    creator_.GetSourceConnectionIdLength(),
                    QuicPacketCreatorPeer::SendVersionInPacket(&creator_),
                    !kIncludeDiversificationNonce,
                    QuicPacketCreatorPeer::GetPacketNumberLength(&creator_)),
            creator_.BytesFree());
}

TEST_P(QuicPacketCreatorTest, SerializeAndSendStreamFrame) {
  if (!GetParam().version_serialization) {
    creator_.StopSendingVersion();
  }
  EXPECT_FALSE(creator_.HasPendingFrames());

  MakeIOVector("test", &iov_);
  producer_.SaveStreamData(
      QuicUtils::GetHeadersStreamId(client_framer_.transport_version()), &iov_,
      1u, 0u, 0u, iov_.iov_len);
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
  size_t num_bytes_consumed;
  creator_.CreateAndSerializeStreamFrame(
      QuicUtils::GetHeadersStreamId(client_framer_.transport_version()),
      iov_.iov_len, 0, 0, true, &num_bytes_consumed);
  EXPECT_EQ(4u, num_bytes_consumed);

  // Ensure the packet is successfully created.
  ASSERT_TRUE(serialized_packet_.encrypted_buffer);
  ASSERT_FALSE(serialized_packet_.retransmittable_frames.empty());
  const QuicFrames& retransmittable = serialized_packet_.retransmittable_frames;
  ASSERT_EQ(1u, retransmittable.size());
  EXPECT_EQ(STREAM_FRAME, retransmittable[0].type);
  DeleteSerializedPacket();

  EXPECT_FALSE(creator_.HasPendingFrames());
}

TEST_P(QuicPacketCreatorTest, AddUnencryptedStreamDataClosesConnection) {
  // EXPECT_QUIC_BUG tests are expensive so only run one instance of them.
  if (!IsDefaultTestConfiguration()) {
    return;
  }

  creator_.set_encryption_level(ENCRYPTION_NONE);
  EXPECT_CALL(delegate_, OnUnrecoverableError(_, _, _));
  QuicStreamFrame stream_frame(
      QuicUtils::GetHeadersStreamId(client_framer_.transport_version()),
      /*fin=*/false, 0u, QuicStringPiece());
  EXPECT_QUIC_BUG(creator_.AddSavedFrame(QuicFrame(stream_frame)),
                  "Cannot send stream data without encryption.");
}

TEST_P(QuicPacketCreatorTest, ChloTooLarge) {
  // EXPECT_QUIC_BUG tests are expensive so only run one instance of them.
  if (!IsDefaultTestConfiguration()) {
    return;
  }

  CryptoHandshakeMessage message;
  message.set_tag(kCHLO);
  message.set_minimum_size(kMaxPacketSize);
  CryptoFramer framer;
  std::unique_ptr<QuicData> message_data;
  message_data.reset(framer.ConstructHandshakeMessage(message));

  struct iovec iov;
  MakeIOVector(QuicStringPiece(message_data->data(), message_data->length()),
               &iov);
  QuicFrame frame;
  EXPECT_CALL(delegate_,
              OnUnrecoverableError(QUIC_CRYPTO_CHLO_TOO_LARGE, _, _));
  EXPECT_QUIC_BUG(
      creator_.ConsumeData(
          QuicUtils::GetCryptoStreamId(client_framer_.transport_version()),
          &iov, 1u, iov.iov_len, 0u, 0u, false, false, &frame),
      "Client hello won't fit in a single packet.");
}

TEST_P(QuicPacketCreatorTest, PendingPadding) {
  EXPECT_EQ(0u, creator_.pending_padding_bytes());
  creator_.AddPendingPadding(kMaxNumRandomPaddingBytes * 10);
  EXPECT_EQ(kMaxNumRandomPaddingBytes * 10, creator_.pending_padding_bytes());

  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillRepeatedly(
          Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
  // Flush all paddings.
  while (creator_.pending_padding_bytes() > 0) {
    creator_.Flush();
    {
      InSequence s;
      EXPECT_CALL(framer_visitor_, OnPacket());
      EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
      EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
      EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_));
      EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
      EXPECT_CALL(framer_visitor_, OnPaddingFrame(_));
      EXPECT_CALL(framer_visitor_, OnPacketComplete());
    }
    // Packet only contains padding.
    ProcessPacket(serialized_packet_);
  }
  EXPECT_EQ(0u, creator_.pending_padding_bytes());
}

TEST_P(QuicPacketCreatorTest, FullPaddingDoesNotConsumePendingPadding) {
  creator_.AddPendingPadding(kMaxNumRandomPaddingBytes);
  QuicFrame frame;
  MakeIOVector("test", &iov_);
  ASSERT_TRUE(creator_.ConsumeData(
      QuicUtils::GetCryptoStreamId(client_framer_.transport_version()), &iov_,
      1u, iov_.iov_len, 0u, 0u, false,
      /*needs_full_padding=*/true, &frame));
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
  creator_.Flush();
  EXPECT_EQ(kMaxNumRandomPaddingBytes, creator_.pending_padding_bytes());
}

TEST_P(QuicPacketCreatorTest, SendPendingPaddingInRetransmission) {
  QuicStreamFrame stream_frame(
      QuicUtils::GetCryptoStreamId(client_framer_.transport_version()),
      /*fin=*/false, 0u, QuicStringPiece());
  QuicFrames frames;
  frames.push_back(QuicFrame(stream_frame));
  char buffer[kMaxPacketSize];
  QuicPendingRetransmission retransmission(CreateRetransmission(
      frames, true, /*num_padding_bytes=*/0, ENCRYPTION_NONE,
      QuicPacketCreatorPeer::GetPacketNumberLength(&creator_)));
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
  creator_.AddPendingPadding(kMaxNumRandomPaddingBytes);
  creator_.ReserializeAllFrames(retransmission, buffer, kMaxPacketSize);
  EXPECT_EQ(0u, creator_.pending_padding_bytes());
  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
    EXPECT_CALL(framer_visitor_, OnPaddingFrame(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(serialized_packet_);
}

TEST_P(QuicPacketCreatorTest, SendPacketAfterFullPaddingRetransmission) {
  // Making sure needs_full_padding gets reset after a full padding
  // retransmission.
  EXPECT_EQ(0u, creator_.pending_padding_bytes());
  QuicFrame frame;
  MakeIOVector("fake handshake message data", &iov_);
  producer_.SaveStreamData(
      QuicUtils::GetCryptoStreamId(client_framer_.transport_version()), &iov_,
      1u, 0u, 0u, iov_.iov_len);
  QuicPacketCreatorPeer::CreateStreamFrame(
      &creator_,
      QuicUtils::GetCryptoStreamId(client_framer_.transport_version()),
      iov_.iov_len, 0u, 0u, false, &frame);
  QuicFrames frames;
  frames.push_back(frame);
  char buffer[kMaxPacketSize];
  QuicPendingRetransmission retransmission(CreateRetransmission(
      frames, true, /*num_padding_bytes=*/-1, ENCRYPTION_NONE,
      QuicPacketCreatorPeer::GetPacketNumberLength(&creator_)));
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillRepeatedly(
          Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
  creator_.ReserializeAllFrames(retransmission, buffer, kMaxPacketSize);
  EXPECT_EQ(kDefaultMaxPacketSize, serialized_packet_.encrypted_length);
  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
    // Full padding.
    EXPECT_CALL(framer_visitor_, OnPaddingFrame(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(serialized_packet_);

  creator_.ConsumeData(
      QuicUtils::GetCryptoStreamId(client_framer_.transport_version()), &iov_,
      1u, iov_.iov_len, 0u, 0u, false, false, &frame);
  creator_.Flush();
  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnDecryptedPacket(_));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
    // needs_full_padding gets reset.
    EXPECT_CALL(framer_visitor_, OnPaddingFrame(_)).Times(0);
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(serialized_packet_);
}

TEST_P(QuicPacketCreatorTest, ConsumeDataAndRandomPadding) {
  const QuicByteCount kStreamFramePayloadSize = 100u;
  // Set the packet size be enough for one stream frame with 0 stream offset +
  // 1.
  size_t length =
      GetPacketHeaderOverhead(client_framer_.transport_version()) +
      GetEncryptionOverhead() +
      QuicFramer::GetMinStreamFrameSize(
          client_framer_.transport_version(),
          QuicUtils::GetCryptoStreamId(client_framer_.transport_version()), 0,
          /*last_frame_in_packet=*/false, kStreamFramePayloadSize + 1) +
      kStreamFramePayloadSize + 1;
  creator_.SetMaxPacketLength(length);
  creator_.AddPendingPadding(kMaxNumRandomPaddingBytes);
  QuicByteCount pending_padding_bytes = creator_.pending_padding_bytes();
  QuicFrame frame;
  char buf[kStreamFramePayloadSize + 1] = {};
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillRepeatedly(
          Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
  // Send stream frame of size kStreamFramePayloadSize.
  MakeIOVector(QuicStringPiece(buf, kStreamFramePayloadSize), &iov_);
  creator_.ConsumeData(
      QuicUtils::GetCryptoStreamId(client_framer_.transport_version()), &iov_,
      1u, iov_.iov_len, 0u, 0u, false, false, &frame);
  creator_.Flush();
  // 1 byte padding is sent.
  EXPECT_EQ(pending_padding_bytes - 1, creator_.pending_padding_bytes());
  // Send stream frame of size kStreamFramePayloadSize + 1.
  MakeIOVector(QuicStringPiece(buf, kStreamFramePayloadSize + 1), &iov_);
  creator_.ConsumeData(
      QuicUtils::GetCryptoStreamId(client_framer_.transport_version()), &iov_,
      1u, iov_.iov_len, 0u, kStreamFramePayloadSize, false, false, &frame);
  // No padding is sent.
  creator_.Flush();
  EXPECT_EQ(pending_padding_bytes - 1, creator_.pending_padding_bytes());
  // Flush all paddings.
  while (creator_.pending_padding_bytes() > 0) {
    creator_.Flush();
  }
  EXPECT_EQ(0u, creator_.pending_padding_bytes());
}

TEST_P(QuicPacketCreatorTest, FlushWithExternalBuffer) {
  char external_buffer[kMaxPacketSize];
  char* expected_buffer = external_buffer;
  EXPECT_CALL(delegate_, GetPacketBuffer()).WillOnce(Return(expected_buffer));

  QuicFrame frame;
  MakeIOVector("test", &iov_);
  ASSERT_TRUE(creator_.ConsumeData(
      QuicUtils::GetCryptoStreamId(client_framer_.transport_version()), &iov_,
      1u, iov_.iov_len, 0u, 0u, false,
      /*needs_full_padding=*/true, &frame));

  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke([expected_buffer](SerializedPacket* serialized_packet) {
        EXPECT_EQ(expected_buffer, serialized_packet->encrypted_buffer);
        ClearSerializedPacket(serialized_packet);
      }));
  creator_.Flush();
}

// Test for error found in
// https://bugs.chromium.org/p/chromium/issues/detail?id=859949 where a gap
// length that crosses an IETF VarInt length boundary would cause a
// failure. While this test is not applicable to versions other than version 99,
// it should still work. Hence, it is not made version-specific.
TEST_P(QuicPacketCreatorTest, IetfAckGapErrorRegression) {
  QuicAckFrame ack_frame = InitAckFrame({{60, 61}, {125, 126}});
  frames_.push_back(QuicFrame(&ack_frame));
  SerializeAllFrames(frames_);
}

TEST_P(QuicPacketCreatorTest, AddMessageFrame) {
  if (client_framer_.transport_version() <= QUIC_VERSION_44) {
    return;
  }
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .Times(3)
      .WillRepeatedly(
          Invoke(this, &QuicPacketCreatorTest::ClearSerializedPacketForTests));
  // Verify that there is enough room for the largest message payload.
  EXPECT_TRUE(
      creator_.HasRoomForMessageFrame(creator_.GetLargestMessagePayload()));
  QuicString message(creator_.GetLargestMessagePayload(), 'a');
  EXPECT_TRUE(
      creator_.AddSavedFrame(QuicFrame(new QuicMessageFrame(1, message))));
  EXPECT_TRUE(creator_.HasPendingFrames());
  creator_.Flush();

  EXPECT_TRUE(
      creator_.AddSavedFrame(QuicFrame(new QuicMessageFrame(2, "message"))));
  EXPECT_TRUE(creator_.HasPendingFrames());
  // Verify if a new frame is added, 1 byte message length will be added.
  EXPECT_EQ(1u, creator_.ExpansionOnNewFrame());
  EXPECT_TRUE(
      creator_.AddSavedFrame(QuicFrame(new QuicMessageFrame(3, "message2"))));
  EXPECT_EQ(1u, creator_.ExpansionOnNewFrame());
  creator_.Flush();

  QuicFrame frame;
  MakeIOVector("test", &iov_);
  EXPECT_TRUE(creator_.ConsumeData(
      QuicUtils::GetCryptoStreamId(client_framer_.transport_version()), &iov_,
      1u, iov_.iov_len, 0u, 0u, false, false, &frame));
  EXPECT_TRUE(
      creator_.AddSavedFrame(QuicFrame(new QuicMessageFrame(1, "message"))));
  EXPECT_TRUE(creator_.HasPendingFrames());
  // Verify there is not enough room for largest payload.
  EXPECT_FALSE(
      creator_.HasRoomForMessageFrame(creator_.GetLargestMessagePayload()));
  // Add largest message will causes the flush of the stream frame.
  QuicMessageFrame message_frame(2, message);
  EXPECT_FALSE(creator_.AddSavedFrame(QuicFrame(&message_frame)));
  EXPECT_FALSE(creator_.HasPendingFrames());
}

TEST_P(QuicPacketCreatorTest, MessageFrameConsumption) {
  if (client_framer_.transport_version() <= QUIC_VERSION_44) {
    return;
  }
  QuicString message_data(kDefaultMaxPacketSize, 'a');
  QuicStringPiece message_buffer(message_data);
  // Test all possible size of message frames.
  for (size_t message_size = 0;
       message_size <= creator_.GetLargestMessagePayload(); ++message_size) {
    EXPECT_TRUE(creator_.AddSavedFrame(QuicFrame(new QuicMessageFrame(
        0, QuicStringPiece(message_buffer.data(), message_size)))));
    EXPECT_TRUE(creator_.HasPendingFrames());

    size_t expansion_bytes = message_size >= 64 ? 2 : 1;
    EXPECT_EQ(expansion_bytes, creator_.ExpansionOnNewFrame());
    // Verify BytesFree returns bytes available for the next frame, which should
    // subtract the message length.
    size_t expected_bytes_free =
        creator_.GetLargestMessagePayload() - message_size < expansion_bytes
            ? 0
            : creator_.GetLargestMessagePayload() - expansion_bytes -
                  message_size;
    EXPECT_EQ(expected_bytes_free, creator_.BytesFree());
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketCreatorTest::SaveSerializedPacket));
    creator_.Flush();
    ASSERT_TRUE(serialized_packet_.encrypted_buffer);
    DeleteSerializedPacket();
  }
}

}  // namespace
}  // namespace test
}  // namespace quic
