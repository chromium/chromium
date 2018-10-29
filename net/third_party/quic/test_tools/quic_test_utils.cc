// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/test_tools/quic_test_utils.h"

#include <algorithm>
#include <memory>

#include "net/third_party/quic/core/crypto/crypto_framer.h"
#include "net/third_party/quic/core/crypto/crypto_handshake.h"
#include "net/third_party/quic/core/crypto/crypto_utils.h"
#include "net/third_party/quic/core/crypto/null_encrypter.h"
#include "net/third_party/quic/core/crypto/quic_decrypter.h"
#include "net/third_party/quic/core/crypto/quic_encrypter.h"
#include "net/third_party/quic/core/quic_data_writer.h"
#include "net/third_party/quic/core/quic_framer.h"
#include "net/third_party/quic/core/quic_packet_creator.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/platform/api/quic_arraysize.h"
#include "net/third_party/quic/platform/api/quic_endian.h"
#include "net/third_party/quic/platform/api/quic_flags.h"
#include "net/third_party/quic/platform/api/quic_logging.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quic/test_tools/quic_config_peer.h"
#include "net/third_party/quic/test_tools/quic_connection_peer.h"
#include "net/third_party/spdy/core/spdy_frame_builder.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

using testing::_;
using testing::Invoke;

namespace quic {
namespace test {

QuicAckFrame InitAckFrame(const std::vector<QuicAckBlock>& ack_blocks) {
  DCHECK_GT(ack_blocks.size(), 0u);

  QuicAckFrame ack;
  QuicPacketNumber end_of_previous_block = 1;
  for (const QuicAckBlock& block : ack_blocks) {
    DCHECK_GE(block.start, end_of_previous_block);
    DCHECK_GT(block.limit, block.start);
    ack.packets.AddRange(block.start, block.limit);
    end_of_previous_block = block.limit;
  }

  ack.largest_acked = ack.packets.Max();

  return ack;
}

QuicAckFrame InitAckFrame(QuicPacketNumber largest_acked) {
  return InitAckFrame({{1, largest_acked + 1}});
}

QuicAckFrame MakeAckFrameWithAckBlocks(size_t num_ack_blocks,
                                       QuicPacketNumber least_unacked) {
  QuicAckFrame ack;
  ack.largest_acked = 2 * num_ack_blocks + least_unacked;
  // Add enough received packets to get num_ack_blocks ack blocks.
  for (QuicPacketNumber i = 2; i < 2 * num_ack_blocks + 1; i += 2) {
    ack.packets.Add(least_unacked + i);
  }
  return ack;
}

std::unique_ptr<QuicPacket> BuildUnsizedDataPacket(
    QuicFramer* framer,
    const QuicPacketHeader& header,
    const QuicFrames& frames) {
  const size_t max_plaintext_size = framer->GetMaxPlaintextSize(kMaxPacketSize);
  size_t packet_size = GetPacketHeaderSize(framer->transport_version(), header);
  for (size_t i = 0; i < frames.size(); ++i) {
    DCHECK_LE(packet_size, max_plaintext_size);
    bool first_frame = i == 0;
    bool last_frame = i == frames.size() - 1;
    const size_t frame_size = framer->GetSerializedFrameLength(
        frames[i], max_plaintext_size - packet_size, first_frame, last_frame,
        header.packet_number_length);
    DCHECK(frame_size);
    packet_size += frame_size;
  }
  return BuildUnsizedDataPacket(framer, header, frames, packet_size);
}

std::unique_ptr<QuicPacket> BuildUnsizedDataPacket(
    QuicFramer* framer,
    const QuicPacketHeader& header,
    const QuicFrames& frames,
    size_t packet_size) {
  char* buffer = new char[packet_size];
  size_t length = framer->BuildDataPacket(header, frames, buffer, packet_size);
  DCHECK_NE(0u, length);
  // Re-construct the data packet with data ownership.
  return QuicMakeUnique<QuicPacket>(
      buffer, length, /* owns_buffer */ true,
      header.destination_connection_id_length,
      header.source_connection_id_length, header.version_flag,
      header.nonce != nullptr, header.packet_number_length);
}

QuicString Sha1Hash(QuicStringPiece data) {
  char buffer[SHA_DIGEST_LENGTH];
  SHA1(reinterpret_cast<const uint8_t*>(data.data()), data.size(),
       reinterpret_cast<uint8_t*>(buffer));
  return QuicString(buffer, QUIC_ARRAYSIZE(buffer));
}

uint64_t SimpleRandom::RandUint64() {
  QuicString hash =
      Sha1Hash(QuicStringPiece(reinterpret_cast<char*>(&seed_), sizeof(seed_)));
  DCHECK_EQ(static_cast<size_t>(SHA_DIGEST_LENGTH), hash.length());
  memcpy(&seed_, hash.data(), sizeof(seed_));
  return seed_;
}

void SimpleRandom::RandBytes(void* data, size_t len) {
  uint8_t* real_data = static_cast<uint8_t*>(data);
  for (size_t offset = 0; offset < len; offset++) {
    real_data[offset] = RandUint64() & 0xff;
  }
}

MockFramerVisitor::MockFramerVisitor() {
  // By default, we want to accept packets.
  ON_CALL(*this, OnProtocolVersionMismatch(_))
      .WillByDefault(testing::Return(false));

  // By default, we want to accept packets.
  ON_CALL(*this, OnUnauthenticatedHeader(_))
      .WillByDefault(testing::Return(true));

  ON_CALL(*this, OnUnauthenticatedPublicHeader(_))
      .WillByDefault(testing::Return(true));

  ON_CALL(*this, OnPacketHeader(_)).WillByDefault(testing::Return(true));

  ON_CALL(*this, OnStreamFrame(_)).WillByDefault(testing::Return(true));

  ON_CALL(*this, OnCryptoFrame(_)).WillByDefault(testing::Return(true));

  ON_CALL(*this, OnStopWaitingFrame(_)).WillByDefault(testing::Return(true));

  ON_CALL(*this, OnPaddingFrame(_)).WillByDefault(testing::Return(true));

  ON_CALL(*this, OnPingFrame(_)).WillByDefault(testing::Return(true));

  ON_CALL(*this, OnRstStreamFrame(_)).WillByDefault(testing::Return(true));

  ON_CALL(*this, OnConnectionCloseFrame(_))
      .WillByDefault(testing::Return(true));

  ON_CALL(*this, OnApplicationCloseFrame(_))
      .WillByDefault(testing::Return(true));

  ON_CALL(*this, OnStopSendingFrame(_)).WillByDefault(testing::Return(true));

  ON_CALL(*this, OnPathChallengeFrame(_)).WillByDefault(testing::Return(true));

  ON_CALL(*this, OnPathResponseFrame(_)).WillByDefault(testing::Return(true));

  ON_CALL(*this, OnGoAwayFrame(_)).WillByDefault(testing::Return(true));
  ON_CALL(*this, OnMaxStreamIdFrame(_)).WillByDefault(testing::Return(true));
  ON_CALL(*this, OnStreamIdBlockedFrame(_))
      .WillByDefault(testing::Return(true));
}

MockFramerVisitor::~MockFramerVisitor() {}

bool NoOpFramerVisitor::OnProtocolVersionMismatch(ParsedQuicVersion version) {
  return false;
}

bool NoOpFramerVisitor::OnUnauthenticatedPublicHeader(
    const QuicPacketHeader& header) {
  return true;
}

bool NoOpFramerVisitor::OnUnauthenticatedHeader(
    const QuicPacketHeader& header) {
  return true;
}

bool NoOpFramerVisitor::OnPacketHeader(const QuicPacketHeader& header) {
  return true;
}

bool NoOpFramerVisitor::OnStreamFrame(const QuicStreamFrame& frame) {
  return true;
}

bool NoOpFramerVisitor::OnCryptoFrame(const QuicCryptoFrame& frame) {
  return true;
}

bool NoOpFramerVisitor::OnAckFrameStart(QuicPacketNumber largest_acked,
                                        QuicTime::Delta ack_delay_time) {
  return true;
}

bool NoOpFramerVisitor::OnAckRange(QuicPacketNumber start,
                                   QuicPacketNumber end) {
  return true;
}

bool NoOpFramerVisitor::OnAckTimestamp(QuicPacketNumber packet_number,
                                       QuicTime timestamp) {
  return true;
}

bool NoOpFramerVisitor::OnAckFrameEnd(QuicPacketNumber start) {
  return true;
}

bool NoOpFramerVisitor::OnStopWaitingFrame(const QuicStopWaitingFrame& frame) {
  return true;
}

bool NoOpFramerVisitor::OnPaddingFrame(const QuicPaddingFrame& frame) {
  return true;
}

bool NoOpFramerVisitor::OnPingFrame(const QuicPingFrame& frame) {
  return true;
}

bool NoOpFramerVisitor::OnRstStreamFrame(const QuicRstStreamFrame& frame) {
  return true;
}

bool NoOpFramerVisitor::OnConnectionCloseFrame(
    const QuicConnectionCloseFrame& frame) {
  return true;
}

bool NoOpFramerVisitor::OnApplicationCloseFrame(
    const QuicApplicationCloseFrame& frame) {
  return true;
}

bool NoOpFramerVisitor::OnNewConnectionIdFrame(
    const QuicNewConnectionIdFrame& frame) {
  return true;
}

bool NoOpFramerVisitor::OnRetireConnectionIdFrame(
    const QuicRetireConnectionIdFrame& frame) {
  return true;
}

bool NoOpFramerVisitor::OnNewTokenFrame(const QuicNewTokenFrame& frame) {
  return true;
}

bool NoOpFramerVisitor::OnStopSendingFrame(const QuicStopSendingFrame& frame) {
  return true;
}

bool NoOpFramerVisitor::OnPathChallengeFrame(
    const QuicPathChallengeFrame& frame) {
  return true;
}

bool NoOpFramerVisitor::OnPathResponseFrame(
    const QuicPathResponseFrame& frame) {
  return true;
}

bool NoOpFramerVisitor::OnGoAwayFrame(const QuicGoAwayFrame& frame) {
  return true;
}

bool NoOpFramerVisitor::OnMaxStreamIdFrame(const QuicMaxStreamIdFrame& frame) {
  return true;
}

bool NoOpFramerVisitor::OnStreamIdBlockedFrame(
    const QuicStreamIdBlockedFrame& frame) {
  return true;
}

bool NoOpFramerVisitor::OnWindowUpdateFrame(
    const QuicWindowUpdateFrame& frame) {
  return true;
}

bool NoOpFramerVisitor::OnBlockedFrame(const QuicBlockedFrame& frame) {
  return true;
}

bool NoOpFramerVisitor::OnMessageFrame(const QuicMessageFrame& frame) {
  return true;
}

bool NoOpFramerVisitor::IsValidStatelessResetToken(QuicUint128 token) const {
  return false;
}

MockQuicConnectionVisitor::MockQuicConnectionVisitor() {}

MockQuicConnectionVisitor::~MockQuicConnectionVisitor() {}

MockQuicConnectionHelper::MockQuicConnectionHelper() {}

MockQuicConnectionHelper::~MockQuicConnectionHelper() {}

const QuicClock* MockQuicConnectionHelper::GetClock() const {
  return &clock_;
}

QuicRandom* MockQuicConnectionHelper::GetRandomGenerator() {
  return &random_generator_;
}

QuicAlarm* MockAlarmFactory::CreateAlarm(QuicAlarm::Delegate* delegate) {
  return new MockAlarmFactory::TestAlarm(
      QuicArenaScopedPtr<QuicAlarm::Delegate>(delegate));
}

QuicArenaScopedPtr<QuicAlarm> MockAlarmFactory::CreateAlarm(
    QuicArenaScopedPtr<QuicAlarm::Delegate> delegate,
    QuicConnectionArena* arena) {
  if (arena != nullptr) {
    return arena->New<TestAlarm>(std::move(delegate));
  } else {
    return QuicArenaScopedPtr<TestAlarm>(new TestAlarm(std::move(delegate)));
  }
}

QuicBufferAllocator* MockQuicConnectionHelper::GetStreamSendBufferAllocator() {
  return &buffer_allocator_;
}

void MockQuicConnectionHelper::AdvanceTime(QuicTime::Delta delta) {
  clock_.AdvanceTime(delta);
}

MockQuicConnection::MockQuicConnection(MockQuicConnectionHelper* helper,
                                       MockAlarmFactory* alarm_factory,
                                       Perspective perspective)
    : MockQuicConnection(QuicEndian::NetToHost64(kTestConnectionId),
                         QuicSocketAddress(TestPeerIPAddress(), kTestPort),
                         helper,
                         alarm_factory,
                         perspective,
                         ParsedVersionOfIndex(CurrentSupportedVersions(), 0)) {}

MockQuicConnection::MockQuicConnection(QuicSocketAddress address,
                                       MockQuicConnectionHelper* helper,
                                       MockAlarmFactory* alarm_factory,
                                       Perspective perspective)
    : MockQuicConnection(QuicEndian::NetToHost64(kTestConnectionId),
                         address,
                         helper,
                         alarm_factory,
                         perspective,
                         ParsedVersionOfIndex(CurrentSupportedVersions(), 0)) {}

MockQuicConnection::MockQuicConnection(QuicConnectionId connection_id,
                                       MockQuicConnectionHelper* helper,
                                       MockAlarmFactory* alarm_factory,
                                       Perspective perspective)
    : MockQuicConnection(connection_id,
                         QuicSocketAddress(TestPeerIPAddress(), kTestPort),
                         helper,
                         alarm_factory,
                         perspective,
                         ParsedVersionOfIndex(CurrentSupportedVersions(), 0)) {}

MockQuicConnection::MockQuicConnection(
    MockQuicConnectionHelper* helper,
    MockAlarmFactory* alarm_factory,
    Perspective perspective,
    const ParsedQuicVersionVector& supported_versions)
    : MockQuicConnection(QuicEndian::NetToHost64(kTestConnectionId),
                         QuicSocketAddress(TestPeerIPAddress(), kTestPort),
                         helper,
                         alarm_factory,
                         perspective,
                         supported_versions) {}

MockQuicConnection::MockQuicConnection(
    QuicConnectionId connection_id,
    QuicSocketAddress address,
    MockQuicConnectionHelper* helper,
    MockAlarmFactory* alarm_factory,
    Perspective perspective,
    const ParsedQuicVersionVector& supported_versions)
    : QuicConnection(connection_id,
                     address,
                     helper,
                     alarm_factory,
                     new testing::NiceMock<MockPacketWriter>(),
                     /* owns_writer= */ true,
                     perspective,
                     supported_versions) {
  ON_CALL(*this, OnError(_))
      .WillByDefault(
          Invoke(this, &PacketSavingConnection::QuicConnection_OnError));
}

MockQuicConnection::~MockQuicConnection() {}

void MockQuicConnection::AdvanceTime(QuicTime::Delta delta) {
  static_cast<MockQuicConnectionHelper*>(helper())->AdvanceTime(delta);
}

bool MockQuicConnection::OnProtocolVersionMismatch(ParsedQuicVersion version) {
  return false;
}

PacketSavingConnection::PacketSavingConnection(MockQuicConnectionHelper* helper,
                                               MockAlarmFactory* alarm_factory,
                                               Perspective perspective)
    : MockQuicConnection(helper, alarm_factory, perspective) {}

PacketSavingConnection::PacketSavingConnection(
    MockQuicConnectionHelper* helper,
    MockAlarmFactory* alarm_factory,
    Perspective perspective,
    const ParsedQuicVersionVector& supported_versions)
    : MockQuicConnection(helper,
                         alarm_factory,
                         perspective,
                         supported_versions) {}

PacketSavingConnection::~PacketSavingConnection() {}

void PacketSavingConnection::SendOrQueuePacket(SerializedPacket* packet) {
  encrypted_packets_.push_back(QuicMakeUnique<QuicEncryptedPacket>(
      CopyBuffer(*packet), packet->encrypted_length, true));
  // Transfer ownership of the packet to the SentPacketManager and the
  // ack notifier to the AckNotifierManager.
  QuicConnectionPeer::GetSentPacketManager(this)->OnPacketSent(
      packet, 0, QuicTime::Zero(), NOT_RETRANSMISSION,
      HAS_RETRANSMITTABLE_DATA);
}

MockQuicSession::MockQuicSession(QuicConnection* connection)
    : MockQuicSession(connection, true) {}

MockQuicSession::MockQuicSession(QuicConnection* connection,
                                 bool create_mock_crypto_stream)
    : QuicSession(connection,
                  nullptr,
                  DefaultQuicConfig(),
                  CurrentSupportedVersions()) {
  if (create_mock_crypto_stream) {
    crypto_stream_ = QuicMakeUnique<MockQuicCryptoStream>(this);
  }
  ON_CALL(*this, WritevData(_, _, _, _, _))
      .WillByDefault(testing::Return(QuicConsumedData(0, false)));
}

MockQuicSession::~MockQuicSession() {
  delete connection();
}

QuicCryptoStream* MockQuicSession::GetMutableCryptoStream() {
  return crypto_stream_.get();
}

const QuicCryptoStream* MockQuicSession::GetCryptoStream() const {
  return crypto_stream_.get();
}

void MockQuicSession::SetCryptoStream(QuicCryptoStream* crypto_stream) {
  crypto_stream_.reset(crypto_stream);
}

// static
QuicConsumedData MockQuicSession::ConsumeData(QuicStream* stream,
                                              QuicStreamId /*id*/,
                                              size_t write_length,
                                              QuicStreamOffset offset,
                                              StreamSendingState state) {
  if (write_length > 0) {
    auto buf = QuicMakeUnique<char[]>(write_length);
    QuicDataWriter writer(write_length, buf.get(), HOST_BYTE_ORDER);
    stream->WriteStreamData(offset, write_length, &writer);
  } else {
    DCHECK(state != NO_FIN);
  }
  return QuicConsumedData(write_length, state != NO_FIN);
}

MockQuicCryptoStream::MockQuicCryptoStream(QuicSession* session)
    : QuicCryptoStream(session), params_(new QuicCryptoNegotiatedParameters) {}

MockQuicCryptoStream::~MockQuicCryptoStream() {}

QuicLongHeaderType MockQuicCryptoStream::GetLongHeaderType(
    QuicStreamOffset /*offset*/) const {
  return HANDSHAKE;
}

bool MockQuicCryptoStream::encryption_established() const {
  return false;
}

bool MockQuicCryptoStream::handshake_confirmed() const {
  return false;
}

const QuicCryptoNegotiatedParameters&
MockQuicCryptoStream::crypto_negotiated_params() const {
  return *params_;
}

CryptoMessageParser* MockQuicCryptoStream::crypto_message_parser() {
  return &crypto_framer_;
}

MockQuicSpdySession::MockQuicSpdySession(QuicConnection* connection)
    : MockQuicSpdySession(connection, true) {}

MockQuicSpdySession::MockQuicSpdySession(QuicConnection* connection,
                                         bool create_mock_crypto_stream)
    : QuicSpdySession(connection,
                      nullptr,
                      DefaultQuicConfig(),
                      connection->supported_versions()) {
  if (create_mock_crypto_stream) {
    crypto_stream_ = QuicMakeUnique<MockQuicCryptoStream>(this);
  }

  ON_CALL(*this, WritevData(_, _, _, _, _))
      .WillByDefault(testing::Return(QuicConsumedData(0, false)));
}

MockQuicSpdySession::~MockQuicSpdySession() {
  delete connection();
}

QuicCryptoStream* MockQuicSpdySession::GetMutableCryptoStream() {
  return crypto_stream_.get();
}

const QuicCryptoStream* MockQuicSpdySession::GetCryptoStream() const {
  return crypto_stream_.get();
}

void MockQuicSpdySession::SetCryptoStream(QuicCryptoStream* crypto_stream) {
  crypto_stream_.reset(crypto_stream);
}

size_t MockQuicSpdySession::WriteHeaders(
    QuicStreamId id,
    spdy::SpdyHeaderBlock headers,
    bool fin,
    spdy::SpdyPriority priority,
    QuicReferenceCountedPointer<QuicAckListenerInterface> ack_listener) {
  write_headers_ = std::move(headers);
  return WriteHeadersMock(id, write_headers_, fin, priority, ack_listener);
}

TestQuicSpdyServerSession::TestQuicSpdyServerSession(
    QuicConnection* connection,
    const QuicConfig& config,
    const ParsedQuicVersionVector& supported_versions,
    const QuicCryptoServerConfig* crypto_config,
    QuicCompressedCertsCache* compressed_certs_cache)
    : QuicServerSessionBase(config,
                            supported_versions,
                            connection,
                            &visitor_,
                            &helper_,
                            crypto_config,
                            compressed_certs_cache) {
  Initialize();
  ON_CALL(helper_, GenerateConnectionIdForReject(_))
      .WillByDefault(
          testing::Return(connection->random_generator()->RandUint64()));
  ON_CALL(helper_, CanAcceptClientHello(_, _, _, _, _))
      .WillByDefault(testing::Return(true));
}

TestQuicSpdyServerSession::~TestQuicSpdyServerSession() {
  delete connection();
}

QuicCryptoServerStreamBase*
TestQuicSpdyServerSession::CreateQuicCryptoServerStream(
    const QuicCryptoServerConfig* crypto_config,
    QuicCompressedCertsCache* compressed_certs_cache) {
  return new QuicCryptoServerStream(
      crypto_config, compressed_certs_cache,
      GetQuicReloadableFlag(enable_quic_stateless_reject_support), this,
      &helper_);
}

void TestQuicSpdyServerSession::OnCryptoHandshakeEvent(
    CryptoHandshakeEvent event) {
  QuicSession::OnCryptoHandshakeEvent(event);
}

QuicCryptoServerStream* TestQuicSpdyServerSession::GetMutableCryptoStream() {
  return static_cast<QuicCryptoServerStream*>(
      QuicServerSessionBase::GetMutableCryptoStream());
}

const QuicCryptoServerStream* TestQuicSpdyServerSession::GetCryptoStream()
    const {
  return static_cast<const QuicCryptoServerStream*>(
      QuicServerSessionBase::GetCryptoStream());
}

TestQuicSpdyClientSession::TestQuicSpdyClientSession(
    QuicConnection* connection,
    const QuicConfig& config,
    const QuicServerId& server_id,
    QuicCryptoClientConfig* crypto_config)
    : QuicSpdyClientSessionBase(connection, &push_promise_index_, config) {
  crypto_stream_ = QuicMakeUnique<QuicCryptoClientStream>(
      server_id, this, crypto_test_utils::ProofVerifyContextForTesting(),
      crypto_config, this);
  Initialize();
}

TestQuicSpdyClientSession::~TestQuicSpdyClientSession() {}

bool TestQuicSpdyClientSession::IsAuthorized(const QuicString& authority) {
  return true;
}

void TestQuicSpdyClientSession::OnCryptoHandshakeEvent(
    CryptoHandshakeEvent event) {
  QuicSession::OnCryptoHandshakeEvent(event);
}

QuicCryptoClientStream* TestQuicSpdyClientSession::GetMutableCryptoStream() {
  return crypto_stream_.get();
}

const QuicCryptoClientStream* TestQuicSpdyClientSession::GetCryptoStream()
    const {
  return crypto_stream_.get();
}

TestPushPromiseDelegate::TestPushPromiseDelegate(bool match)
    : match_(match), rendezvous_fired_(false), rendezvous_stream_(nullptr) {}

bool TestPushPromiseDelegate::CheckVary(
    const spdy::SpdyHeaderBlock& client_request,
    const spdy::SpdyHeaderBlock& promise_request,
    const spdy::SpdyHeaderBlock& promise_response) {
  QUIC_DVLOG(1) << "match " << match_;
  return match_;
}

void TestPushPromiseDelegate::OnRendezvousResult(QuicSpdyStream* stream) {
  rendezvous_fired_ = true;
  rendezvous_stream_ = stream;
}

MockPacketWriter::MockPacketWriter() {
  ON_CALL(*this, GetMaxPacketSize(_))
      .WillByDefault(testing::Return(kMaxPacketSize));
  ON_CALL(*this, IsBatchMode()).WillByDefault(testing::Return(false));
  ON_CALL(*this, GetNextWriteLocation(_, _))
      .WillByDefault(testing::Return(nullptr));
  ON_CALL(*this, Flush())
      .WillByDefault(testing::Return(WriteResult(WRITE_STATUS_OK, 0)));
}

MockPacketWriter::~MockPacketWriter() {}

MockSendAlgorithm::MockSendAlgorithm() {}

MockSendAlgorithm::~MockSendAlgorithm() {}

MockLossAlgorithm::MockLossAlgorithm() {}

MockLossAlgorithm::~MockLossAlgorithm() {}

MockAckListener::MockAckListener() {}

MockAckListener::~MockAckListener() {}

MockNetworkChangeVisitor::MockNetworkChangeVisitor() {}

MockNetworkChangeVisitor::~MockNetworkChangeVisitor() {}

namespace {

QuicString HexDumpWithMarks(const char* data,
                            int length,
                            const bool* marks,
                            int mark_length) {
  static const char kHexChars[] = "0123456789abcdef";
  static const int kColumns = 4;

  const int kSizeLimit = 1024;
  if (length > kSizeLimit || mark_length > kSizeLimit) {
    QUIC_LOG(ERROR) << "Only dumping first " << kSizeLimit << " bytes.";
    length = std::min(length, kSizeLimit);
    mark_length = std::min(mark_length, kSizeLimit);
  }

  QuicString hex;
  for (const char *row = data; length > 0;
       row += kColumns, length -= kColumns) {
    for (const char* p = row; p < row + 4; ++p) {
      if (p < row + length) {
        const bool mark =
            (marks && (p - data) < mark_length && marks[p - data]);
        hex += mark ? '*' : ' ';
        hex += kHexChars[(*p & 0xf0) >> 4];
        hex += kHexChars[*p & 0x0f];
        hex += mark ? '*' : ' ';
      } else {
        hex += "    ";
      }
    }
    hex = hex + "  ";

    for (const char* p = row; p < row + 4 && p < row + length; ++p) {
      hex += (*p >= 0x20 && *p <= 0x7f) ? (*p) : '.';
    }

    hex = hex + '\n';
  }
  return hex;
}

}  // namespace

QuicIpAddress TestPeerIPAddress() {
  return QuicIpAddress::Loopback4();
}

ParsedQuicVersion QuicVersionMax() {
  return AllSupportedVersions().front();
}

ParsedQuicVersion QuicVersionMin() {
  return AllSupportedVersions().back();
}

QuicTransportVersion QuicTransportVersionMax() {
  return AllSupportedTransportVersions().front();
}

QuicTransportVersion QuicTransportVersionMin() {
  return AllSupportedTransportVersions().back();
}

QuicEncryptedPacket* ConstructEncryptedPacket(
    QuicConnectionId destination_connection_id,
    QuicConnectionId source_connection_id,
    bool version_flag,
    bool reset_flag,
    QuicPacketNumber packet_number,
    const QuicString& data) {
  return ConstructEncryptedPacket(
      destination_connection_id, source_connection_id, version_flag, reset_flag,
      packet_number, data, PACKET_8BYTE_CONNECTION_ID,
      PACKET_0BYTE_CONNECTION_ID, PACKET_4BYTE_PACKET_NUMBER);
}

QuicEncryptedPacket* ConstructEncryptedPacket(
    QuicConnectionId destination_connection_id,
    QuicConnectionId source_connection_id,
    bool version_flag,
    bool reset_flag,
    QuicPacketNumber packet_number,
    const QuicString& data,
    QuicConnectionIdLength destination_connection_id_length,
    QuicConnectionIdLength source_connection_id_length,
    QuicPacketNumberLength packet_number_length) {
  return ConstructEncryptedPacket(
      destination_connection_id, source_connection_id, version_flag, reset_flag,
      packet_number, data, destination_connection_id_length,
      source_connection_id_length, packet_number_length, nullptr);
}

QuicEncryptedPacket* ConstructEncryptedPacket(
    QuicConnectionId destination_connection_id,
    QuicConnectionId source_connection_id,
    bool version_flag,
    bool reset_flag,
    QuicPacketNumber packet_number,
    const QuicString& data,
    QuicConnectionIdLength destination_connection_id_length,
    QuicConnectionIdLength source_connection_id_length,
    QuicPacketNumberLength packet_number_length,
    ParsedQuicVersionVector* versions) {
  return ConstructEncryptedPacket(
      destination_connection_id, source_connection_id, version_flag, reset_flag,
      packet_number, data, destination_connection_id_length,
      source_connection_id_length, packet_number_length, versions,
      Perspective::IS_CLIENT);
}
QuicEncryptedPacket* ConstructEncryptedPacket(
    QuicConnectionId destination_connection_id,
    QuicConnectionId source_connection_id,
    bool version_flag,
    bool reset_flag,
    QuicPacketNumber packet_number,
    const QuicString& data,
    QuicConnectionIdLength destination_connection_id_length,
    QuicConnectionIdLength source_connection_id_length,
    QuicPacketNumberLength packet_number_length,
    ParsedQuicVersionVector* versions,
    Perspective perspective) {
  QuicPacketHeader header;
  header.destination_connection_id = destination_connection_id;
  header.destination_connection_id_length = destination_connection_id_length;
  header.source_connection_id = source_connection_id;
  header.source_connection_id_length = source_connection_id_length;
  header.version_flag = version_flag;
  header.reset_flag = reset_flag;
  header.packet_number_length = packet_number_length;
  header.packet_number = packet_number;
  QuicFrame frame(QuicStreamFrame(
      QuicUtils::GetCryptoStreamId(
          versions != nullptr
              ? (*versions)[0].transport_version
              : CurrentSupportedVersions()[0].transport_version),
      false, 0, QuicStringPiece(data)));
  QuicFrames frames;
  frames.push_back(frame);
  QuicFramer framer(
      versions != nullptr ? *versions : CurrentSupportedVersions(),
      QuicTime::Zero(), perspective);

  std::unique_ptr<QuicPacket> packet(
      BuildUnsizedDataPacket(&framer, header, frames));
  EXPECT_TRUE(packet != nullptr);
  char* buffer = new char[kMaxPacketSize];
  size_t encrypted_length = framer.EncryptPayload(
      ENCRYPTION_NONE, packet_number, *packet, buffer, kMaxPacketSize);
  EXPECT_NE(0u, encrypted_length);
  return new QuicEncryptedPacket(buffer, encrypted_length, true);
}

QuicReceivedPacket* ConstructReceivedPacket(
    const QuicEncryptedPacket& encrypted_packet,
    QuicTime receipt_time) {
  char* buffer = new char[encrypted_packet.length()];
  memcpy(buffer, encrypted_packet.data(), encrypted_packet.length());
  return new QuicReceivedPacket(buffer, encrypted_packet.length(), receipt_time,
                                true);
}

QuicEncryptedPacket* ConstructMisFramedEncryptedPacket(
    QuicConnectionId destination_connection_id,
    QuicConnectionId source_connection_id,
    bool version_flag,
    bool reset_flag,
    QuicPacketNumber packet_number,
    const QuicString& data,
    QuicConnectionIdLength destination_connection_id_length,
    QuicConnectionIdLength source_connection_id_length,
    QuicPacketNumberLength packet_number_length,
    ParsedQuicVersionVector* versions,
    Perspective perspective) {
  QuicPacketHeader header;
  header.destination_connection_id = destination_connection_id;
  header.destination_connection_id_length = destination_connection_id_length;
  header.source_connection_id = source_connection_id;
  header.source_connection_id_length = source_connection_id_length;
  header.version_flag = version_flag;
  header.reset_flag = reset_flag;
  header.packet_number_length = packet_number_length;
  header.packet_number = packet_number;
  QuicFrame frame(QuicStreamFrame(1, false, 0, QuicStringPiece(data)));
  QuicFrames frames;
  frames.push_back(frame);
  QuicFramer framer(versions != nullptr ? *versions : AllSupportedVersions(),
                    QuicTime::Zero(), perspective);

  std::unique_ptr<QuicPacket> packet(
      BuildUnsizedDataPacket(&framer, header, frames));
  EXPECT_TRUE(packet != nullptr);

  // Now set the frame type to 0x1F, which is an invalid frame type.
  reinterpret_cast<unsigned char*>(
      packet->mutable_data())[GetStartOfEncryptedData(
      framer.transport_version(), destination_connection_id_length,
      source_connection_id_length, version_flag,
      false /* no diversification nonce */, packet_number_length)] = 0x1F;

  char* buffer = new char[kMaxPacketSize];
  size_t encrypted_length = framer.EncryptPayload(
      ENCRYPTION_NONE, packet_number, *packet, buffer, kMaxPacketSize);
  EXPECT_NE(0u, encrypted_length);
  return new QuicEncryptedPacket(buffer, encrypted_length, true);
}

void CompareCharArraysWithHexError(const QuicString& description,
                                   const char* actual,
                                   const int actual_len,
                                   const char* expected,
                                   const int expected_len) {
  EXPECT_EQ(actual_len, expected_len);
  const int min_len = std::min(actual_len, expected_len);
  const int max_len = std::max(actual_len, expected_len);
  std::unique_ptr<bool[]> marks(new bool[max_len]);
  bool identical = (actual_len == expected_len);
  for (int i = 0; i < min_len; ++i) {
    if (actual[i] != expected[i]) {
      marks[i] = true;
      identical = false;
    } else {
      marks[i] = false;
    }
  }
  for (int i = min_len; i < max_len; ++i) {
    marks[i] = true;
  }
  if (identical)
    return;
  ADD_FAILURE() << "Description:\n"
                << description << "\n\nExpected:\n"
                << HexDumpWithMarks(expected, expected_len, marks.get(),
                                    max_len)
                << "\nActual:\n"
                << HexDumpWithMarks(actual, actual_len, marks.get(), max_len);
}

size_t GetPacketLengthForOneStream(
    QuicTransportVersion version,
    bool include_version,
    bool include_diversification_nonce,
    QuicConnectionIdLength destination_connection_id_length,
    QuicConnectionIdLength source_connection_id_length,
    QuicPacketNumberLength packet_number_length,
    size_t* payload_length) {
  *payload_length = 1;
  const size_t stream_length =
      NullEncrypter(Perspective::IS_CLIENT).GetCiphertextSize(*payload_length) +
      QuicPacketCreator::StreamFramePacketOverhead(
          version, destination_connection_id_length,
          source_connection_id_length, include_version,
          include_diversification_nonce, packet_number_length, 0u);
  const size_t ack_length =
      NullEncrypter(Perspective::IS_CLIENT)
          .GetCiphertextSize(QuicFramer::GetMinAckFrameSize(
              version, PACKET_1BYTE_PACKET_NUMBER)) +
      GetPacketHeaderSize(version, destination_connection_id_length,
                          source_connection_id_length, include_version,
                          include_diversification_nonce, packet_number_length);
  if (stream_length < ack_length) {
    *payload_length = 1 + ack_length - stream_length;
  }

  return NullEncrypter(Perspective::IS_CLIENT)
             .GetCiphertextSize(*payload_length) +
         QuicPacketCreator::StreamFramePacketOverhead(
             version, destination_connection_id_length,
             source_connection_id_length, include_version,
             include_diversification_nonce, packet_number_length, 0u);
}

QuicConfig DefaultQuicConfig() {
  QuicConfig config;
  config.SetInitialStreamFlowControlWindowToSend(
      kInitialStreamFlowControlWindowForTest);
  config.SetInitialSessionFlowControlWindowToSend(
      kInitialSessionFlowControlWindowForTest);
  QuicConfigPeer::SetReceivedMaxIncomingDynamicStreams(
      &config, kDefaultMaxStreamsPerConnection);
  // Default enable NSTP.
  // This is unnecessary for versions > 44
  if (!config.HasClientSentConnectionOption(quic::kNSTP,
                                            quic::Perspective::IS_CLIENT)) {
    quic::QuicTagVector connection_options;
    connection_options.push_back(quic::kNSTP);
    config.SetConnectionOptionsToSend(connection_options);
  }
  return config;
}

QuicConfig DefaultQuicConfigStatelessRejects() {
  QuicConfig config = DefaultQuicConfig();
  QuicTagVector copt;
  copt.push_back(kSREJ);
  config.SetConnectionOptionsToSend(copt);
  return config;
}

QuicTransportVersionVector SupportedTransportVersions(
    QuicTransportVersion version) {
  QuicTransportVersionVector versions;
  versions.push_back(version);
  return versions;
}

ParsedQuicVersionVector SupportedVersions(ParsedQuicVersion version) {
  ParsedQuicVersionVector versions;
  versions.push_back(version);
  return versions;
}

MockQuicConnectionDebugVisitor::MockQuicConnectionDebugVisitor() {}

MockQuicConnectionDebugVisitor::~MockQuicConnectionDebugVisitor() {}

MockReceivedPacketManager::MockReceivedPacketManager(QuicConnectionStats* stats)
    : QuicReceivedPacketManager(stats) {}

MockReceivedPacketManager::~MockReceivedPacketManager() {}

MockConnectionCloseDelegate::MockConnectionCloseDelegate() {}

MockConnectionCloseDelegate::~MockConnectionCloseDelegate() {}

MockPacketCreatorDelegate::MockPacketCreatorDelegate() {}
MockPacketCreatorDelegate::~MockPacketCreatorDelegate() {}

MockSessionNotifier::MockSessionNotifier() {}
MockSessionNotifier::~MockSessionNotifier() {}

void CreateClientSessionForTest(
    QuicServerId server_id,
    bool supports_stateless_rejects,
    QuicTime::Delta connection_start_time,
    const ParsedQuicVersionVector& supported_versions,
    MockQuicConnectionHelper* helper,
    MockAlarmFactory* alarm_factory,
    QuicCryptoClientConfig* crypto_client_config,
    PacketSavingConnection** client_connection,
    TestQuicSpdyClientSession** client_session) {
  CHECK(crypto_client_config);
  CHECK(client_connection);
  CHECK(client_session);
  CHECK(!connection_start_time.IsZero())
      << "Connections must start at non-zero times, otherwise the "
      << "strike-register will be unhappy.";

  QuicConfig config = supports_stateless_rejects
                          ? DefaultQuicConfigStatelessRejects()
                          : DefaultQuicConfig();
  *client_connection = new PacketSavingConnection(
      helper, alarm_factory, Perspective::IS_CLIENT, supported_versions);
  *client_session = new TestQuicSpdyClientSession(
      *client_connection, config, server_id, crypto_client_config);
  (*client_connection)->AdvanceTime(connection_start_time);
}

void CreateServerSessionForTest(
    QuicServerId server_id,
    QuicTime::Delta connection_start_time,
    ParsedQuicVersionVector supported_versions,
    MockQuicConnectionHelper* helper,
    MockAlarmFactory* alarm_factory,
    QuicCryptoServerConfig* server_crypto_config,
    QuicCompressedCertsCache* compressed_certs_cache,
    PacketSavingConnection** server_connection,
    TestQuicSpdyServerSession** server_session) {
  CHECK(server_crypto_config);
  CHECK(server_connection);
  CHECK(server_session);
  CHECK(!connection_start_time.IsZero())
      << "Connections must start at non-zero times, otherwise the "
      << "strike-register will be unhappy.";

  *server_connection =
      new PacketSavingConnection(helper, alarm_factory, Perspective::IS_SERVER,
                                 ParsedVersionOfIndex(supported_versions, 0));
  *server_session = new TestQuicSpdyServerSession(
      *server_connection, DefaultQuicConfig(), supported_versions,
      server_crypto_config, compressed_certs_cache);

  // We advance the clock initially because the default time is zero and the
  // strike register worries that we've just overflowed a uint32_t time.
  (*server_connection)->AdvanceTime(connection_start_time);
}

QuicStreamId NextStreamId(QuicTransportVersion version) {
  // TODO(ckrasic) - when version for http stream pairs re-lands, this
  // will be conditional.
  return 2;
}

QuicStreamId GetNthClientInitiatedStreamId(QuicTransportVersion version,
                                           int n) {
  return 5 + NextStreamId(version) * n;
}

QuicStreamId GetNthServerInitiatedStreamId(QuicTransportVersion version,
                                           int n) {
  return 2 + NextStreamId(version) * n;
}

}  // namespace test
}  // namespace quic
