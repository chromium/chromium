// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Common utilities for Quic tests

#ifndef NET_THIRD_PARTY_QUIC_TEST_TOOLS_QUIC_TEST_UTILS_H_
#define NET_THIRD_PARTY_QUIC_TEST_TOOLS_QUIC_TEST_UTILS_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "net/test/gtest_util.h"
#include "net/third_party/quic/core/congestion_control/loss_detection_interface.h"
#include "net/third_party/quic/core/congestion_control/send_algorithm_interface.h"
#include "net/third_party/quic/core/http/quic_client_push_promise_index.h"
#include "net/third_party/quic/core/http/quic_server_session_base.h"
#include "net/third_party/quic/core/quic_connection.h"
#include "net/third_party/quic/core/quic_connection_close_delegate_interface.h"
#include "net/third_party/quic/core/quic_framer.h"
#include "net/third_party/quic/core/quic_packet_writer.h"
#include "net/third_party/quic/core/quic_sent_packet_manager.h"
#include "net/third_party/quic/core/quic_simple_buffer_allocator.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"
#include "net/third_party/quic/test_tools/mock_clock.h"
#include "net/third_party/quic/test_tools/mock_quic_session_visitor.h"
#include "net/third_party/quic/test_tools/mock_random.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace quic {

namespace test {

static const QuicConnectionId kTestConnectionId = 42;
static const uint16_t kTestPort = 12345;
static const uint32_t kInitialStreamFlowControlWindowForTest =
    1024 * 1024;  // 1 MB
static const uint32_t kInitialSessionFlowControlWindowForTest =
    1536 * 1024;  // 1.5 MB

// Returns the test peer IP address.
QuicIpAddress TestPeerIPAddress();

// Upper limit on versions we support.
ParsedQuicVersion QuicVersionMax();

// Lower limit on versions we support.
ParsedQuicVersion QuicVersionMin();

// Upper limit on versions we support.
// TODO(nharper): Remove this function when it is no longer used.
QuicTransportVersion QuicTransportVersionMax();

// Lower limit on versions we support.
// TODO(nharper): Remove this function when it is no longer used.
QuicTransportVersion QuicTransportVersionMin();

// Create an encrypted packet for testing.
// If versions == nullptr, uses &AllSupportedVersions().
// Note that the packet is encrypted with NullEncrypter, so to decrypt the
// constructed packet, the framer must be set to use NullDecrypter.
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
    Perspective perspective);

// Create an encrypted packet for testing.
// If versions == nullptr, uses &AllSupportedVersions().
// Note that the packet is encrypted with NullEncrypter, so to decrypt the
// constructed packet, the framer must be set to use NullDecrypter.
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
    ParsedQuicVersionVector* versions);

// This form assumes |versions| == nullptr.
QuicEncryptedPacket* ConstructEncryptedPacket(
    QuicConnectionId destination_connection_id,
    QuicConnectionId source_connection_id,
    bool version_flag,
    bool reset_flag,
    QuicPacketNumber packet_number,
    const QuicString& data,
    QuicConnectionIdLength destination_connection_id_length,
    QuicConnectionIdLength source_connection_id_length,
    QuicPacketNumberLength packet_number_length);

// This form assumes |connection_id_length| == PACKET_8BYTE_CONNECTION_ID,
// |packet_number_length| == PACKET_4BYTE_PACKET_NUMBER and
// |versions| == nullptr.
QuicEncryptedPacket* ConstructEncryptedPacket(
    QuicConnectionId destination_connection_id,
    QuicConnectionId source_connection_id,
    bool version_flag,
    bool reset_flag,
    QuicPacketNumber packet_number,
    const QuicString& data);

// Constructs a received packet for testing. The caller must take ownership of
// the returned pointer.
QuicReceivedPacket* ConstructReceivedPacket(
    const QuicEncryptedPacket& encrypted_packet,
    QuicTime receipt_time);

// Create an encrypted packet for testing whose data portion erroneous.
// The specific way the data portion is erroneous is not specified, but
// it is an error that QuicFramer detects.
// Note that the packet is encrypted with NullEncrypter, so to decrypt the
// constructed packet, the framer must be set to use NullDecrypter.
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
    Perspective perspective);

void CompareCharArraysWithHexError(const QuicString& description,
                                   const char* actual,
                                   const int actual_len,
                                   const char* expected,
                                   const int expected_len);

// Returns the length of a QuicPacket that is capable of holding either a
// stream frame or a minimal ack frame.  Sets |*payload_length| to the number
// of bytes of stream data that will fit in such a packet.
size_t GetPacketLengthForOneStream(
    QuicTransportVersion version,
    bool include_version,
    bool include_diversification_nonce,
    QuicConnectionIdLength destination_connection_id_length,
    QuicConnectionIdLength source_connection_id_length,
    QuicPacketNumberLength packet_number_length,
    size_t* payload_length);

// Returns QuicConfig set to default values.
QuicConfig DefaultQuicConfig();

// Returns a QuicConfig set to default values that supports stateless rejects.
QuicConfig DefaultQuicConfigStatelessRejects();

// Returns a version vector consisting of |version|.
QuicTransportVersionVector SupportedTransportVersions(
    QuicTransportVersion version);

ParsedQuicVersionVector SupportedVersions(ParsedQuicVersion version);

struct QuicAckBlock {
  QuicPacketNumber start;  // Included
  QuicPacketNumber limit;  // Excluded
};

// Testing convenience method to construct a QuicAckFrame with arbitrary ack
// blocks. Each block is given by a (closed-open) range of packet numbers. e.g.:
// InitAckFrame({{1, 10}})
//   => 1 ack block acking packet numbers 1 to 9.
//
// InitAckFrame({{1, 2}, {3, 4}})
//   => 2 ack blocks acking packet 1 and 3. Packet 2 is missing.
QuicAckFrame InitAckFrame(const std::vector<QuicAckBlock>& ack_blocks);

// Testing convenience method to construct a QuicAckFrame with 1 ack block which
// covers packet number range [1, |largest_acked| + 1).
// Equivalent to InitAckFrame({{1, largest_acked + 1}})
QuicAckFrame InitAckFrame(QuicPacketNumber largest_acked);

// Testing convenience method to construct a QuicAckFrame with |num_ack_blocks|
// ack blocks of width 1 packet, starting from |least_unacked| + 2.
QuicAckFrame MakeAckFrameWithAckBlocks(size_t num_ack_blocks,
                                       QuicPacketNumber least_unacked);

// Returns a QuicPacket that is owned by the caller, and
// is populated with the fields in |header| and |frames|, or is nullptr if the
// packet could not be created.
std::unique_ptr<QuicPacket> BuildUnsizedDataPacket(
    QuicFramer* framer,
    const QuicPacketHeader& header,
    const QuicFrames& frames);
// Returns a QuicPacket that is owned by the caller, and of size |packet_size|.
std::unique_ptr<QuicPacket> BuildUnsizedDataPacket(
    QuicFramer* framer,
    const QuicPacketHeader& header,
    const QuicFrames& frames,
    size_t packet_size);

// Compute SHA-1 hash of the supplied QuicString.
QuicString Sha1Hash(QuicStringPiece data);

// Simple random number generator used to compute random numbers suitable
// for pseudo-randomly dropping packets in tests.  It works by computing
// the sha1 hash of the current seed, and using the first 64 bits as
// the next random number, and the next seed.
class SimpleRandom : public QuicRandom {
 public:
  SimpleRandom() : seed_(0) {}
  SimpleRandom(const SimpleRandom&) = delete;
  SimpleRandom& operator=(const SimpleRandom&) = delete;
  ~SimpleRandom() override {}

  // Returns a random number in the range [0, kuint64max].
  uint64_t RandUint64() override;

  void RandBytes(void* data, size_t len) override;

  void set_seed(uint64_t seed) { seed_ = seed; }

 private:
  uint64_t seed_;
};

class MockFramerVisitor : public QuicFramerVisitorInterface {
 public:
  MockFramerVisitor();
  MockFramerVisitor(const MockFramerVisitor&) = delete;
  MockFramerVisitor& operator=(const MockFramerVisitor&) = delete;
  ~MockFramerVisitor() override;

  MOCK_METHOD1(OnError, void(QuicFramer* framer));
  // The constructor sets this up to return false by default.
  MOCK_METHOD1(OnProtocolVersionMismatch, bool(ParsedQuicVersion version));
  MOCK_METHOD0(OnPacket, void());
  MOCK_METHOD1(OnPublicResetPacket, void(const QuicPublicResetPacket& header));
  MOCK_METHOD1(OnVersionNegotiationPacket,
               void(const QuicVersionNegotiationPacket& packet));
  // The constructor sets this up to return true by default.
  MOCK_METHOD1(OnUnauthenticatedHeader, bool(const QuicPacketHeader& header));
  // The constructor sets this up to return true by default.
  MOCK_METHOD1(OnUnauthenticatedPublicHeader,
               bool(const QuicPacketHeader& header));
  MOCK_METHOD1(OnDecryptedPacket, void(EncryptionLevel level));
  MOCK_METHOD1(OnPacketHeader, bool(const QuicPacketHeader& header));
  MOCK_METHOD1(OnStreamFrame, bool(const QuicStreamFrame& frame));
  MOCK_METHOD1(OnCryptoFrame, bool(const QuicCryptoFrame& frame));
  MOCK_METHOD2(OnAckFrameStart, bool(QuicPacketNumber, QuicTime::Delta));
  MOCK_METHOD2(OnAckRange, bool(QuicPacketNumber, QuicPacketNumber));
  MOCK_METHOD2(OnAckTimestamp, bool(QuicPacketNumber, QuicTime));
  MOCK_METHOD1(OnAckFrameEnd, bool(QuicPacketNumber));
  MOCK_METHOD1(OnStopWaitingFrame, bool(const QuicStopWaitingFrame& frame));
  MOCK_METHOD1(OnPaddingFrame, bool(const QuicPaddingFrame& frame));
  MOCK_METHOD1(OnPingFrame, bool(const QuicPingFrame& frame));
  MOCK_METHOD1(OnRstStreamFrame, bool(const QuicRstStreamFrame& frame));
  MOCK_METHOD1(OnConnectionCloseFrame,
               bool(const QuicConnectionCloseFrame& frame));
  MOCK_METHOD1(OnApplicationCloseFrame,
               bool(const QuicApplicationCloseFrame& frame));
  MOCK_METHOD1(OnNewConnectionIdFrame,
               bool(const QuicNewConnectionIdFrame& frame));
  MOCK_METHOD1(OnRetireConnectionIdFrame,
               bool(const QuicRetireConnectionIdFrame& frame));
  MOCK_METHOD1(OnNewTokenFrame, bool(const QuicNewTokenFrame& frame));
  MOCK_METHOD1(OnStopSendingFrame, bool(const QuicStopSendingFrame& frame));
  MOCK_METHOD1(OnPathChallengeFrame, bool(const QuicPathChallengeFrame& frame));
  MOCK_METHOD1(OnPathResponseFrame, bool(const QuicPathResponseFrame& frame));
  MOCK_METHOD1(OnGoAwayFrame, bool(const QuicGoAwayFrame& frame));
  MOCK_METHOD1(OnMaxStreamIdFrame, bool(const QuicMaxStreamIdFrame& frame));
  MOCK_METHOD1(OnStreamIdBlockedFrame,
               bool(const QuicStreamIdBlockedFrame& frame));
  MOCK_METHOD1(OnWindowUpdateFrame, bool(const QuicWindowUpdateFrame& frame));
  MOCK_METHOD1(OnBlockedFrame, bool(const QuicBlockedFrame& frame));
  MOCK_METHOD1(OnMessageFrame, bool(const QuicMessageFrame& frame));
  MOCK_METHOD0(OnPacketComplete, void());
  MOCK_CONST_METHOD1(IsValidStatelessResetToken, bool(QuicUint128));
  MOCK_METHOD1(OnAuthenticatedIetfStatelessResetPacket,
               void(const QuicIetfStatelessResetPacket&));
};

class NoOpFramerVisitor : public QuicFramerVisitorInterface {
 public:
  NoOpFramerVisitor() {}
  NoOpFramerVisitor(const NoOpFramerVisitor&) = delete;
  NoOpFramerVisitor& operator=(const NoOpFramerVisitor&) = delete;

  void OnError(QuicFramer* framer) override {}
  void OnPacket() override {}
  void OnPublicResetPacket(const QuicPublicResetPacket& packet) override {}
  void OnVersionNegotiationPacket(
      const QuicVersionNegotiationPacket& packet) override {}
  bool OnProtocolVersionMismatch(ParsedQuicVersion version) override;
  bool OnUnauthenticatedHeader(const QuicPacketHeader& header) override;
  bool OnUnauthenticatedPublicHeader(const QuicPacketHeader& header) override;
  void OnDecryptedPacket(EncryptionLevel level) override {}
  bool OnPacketHeader(const QuicPacketHeader& header) override;
  bool OnStreamFrame(const QuicStreamFrame& frame) override;
  bool OnCryptoFrame(const QuicCryptoFrame& frame) override;
  bool OnAckFrameStart(QuicPacketNumber largest_acked,
                       QuicTime::Delta ack_delay_time) override;
  bool OnAckRange(QuicPacketNumber start, QuicPacketNumber end) override;
  bool OnAckTimestamp(QuicPacketNumber packet_number,
                      QuicTime timestamp) override;
  bool OnAckFrameEnd(QuicPacketNumber start) override;
  bool OnStopWaitingFrame(const QuicStopWaitingFrame& frame) override;
  bool OnPaddingFrame(const QuicPaddingFrame& frame) override;
  bool OnPingFrame(const QuicPingFrame& frame) override;
  bool OnRstStreamFrame(const QuicRstStreamFrame& frame) override;
  bool OnConnectionCloseFrame(const QuicConnectionCloseFrame& frame) override;
  bool OnApplicationCloseFrame(const QuicApplicationCloseFrame& frame) override;
  bool OnNewConnectionIdFrame(const QuicNewConnectionIdFrame& frame) override;
  bool OnRetireConnectionIdFrame(
      const QuicRetireConnectionIdFrame& frame) override;
  bool OnNewTokenFrame(const QuicNewTokenFrame& frame) override;
  bool OnStopSendingFrame(const QuicStopSendingFrame& frame) override;
  bool OnPathChallengeFrame(const QuicPathChallengeFrame& frame) override;
  bool OnPathResponseFrame(const QuicPathResponseFrame& frame) override;
  bool OnGoAwayFrame(const QuicGoAwayFrame& frame) override;
  bool OnMaxStreamIdFrame(const QuicMaxStreamIdFrame& frame) override;
  bool OnStreamIdBlockedFrame(const QuicStreamIdBlockedFrame& frame) override;
  bool OnWindowUpdateFrame(const QuicWindowUpdateFrame& frame) override;
  bool OnBlockedFrame(const QuicBlockedFrame& frame) override;
  bool OnMessageFrame(const QuicMessageFrame& frame) override;
  void OnPacketComplete() override {}
  bool IsValidStatelessResetToken(QuicUint128 token) const override;
  void OnAuthenticatedIetfStatelessResetPacket(
      const QuicIetfStatelessResetPacket& packet) override {}
};

class MockQuicConnectionVisitor : public QuicConnectionVisitorInterface {
 public:
  MockQuicConnectionVisitor();
  MockQuicConnectionVisitor(const MockQuicConnectionVisitor&) = delete;
  MockQuicConnectionVisitor& operator=(const MockQuicConnectionVisitor&) =
      delete;
  ~MockQuicConnectionVisitor() override;

  MOCK_METHOD1(OnStreamFrame, void(const QuicStreamFrame& frame));
  MOCK_METHOD1(OnWindowUpdateFrame, void(const QuicWindowUpdateFrame& frame));
  MOCK_METHOD1(OnBlockedFrame, void(const QuicBlockedFrame& frame));
  MOCK_METHOD1(OnRstStream, void(const QuicRstStreamFrame& frame));
  MOCK_METHOD1(OnGoAway, void(const QuicGoAwayFrame& frame));
  MOCK_METHOD1(OnMessageReceived, void(QuicStringPiece message));
  MOCK_METHOD3(OnConnectionClosed,
               void(QuicErrorCode error,
                    const QuicString& error_details,
                    ConnectionCloseSource source));
  MOCK_METHOD0(OnWriteBlocked, void());
  MOCK_METHOD0(OnCanWrite, void());
  MOCK_METHOD1(OnCongestionWindowChange, void(QuicTime now));
  MOCK_METHOD1(OnConnectionMigration, void(AddressChangeType type));
  MOCK_METHOD0(OnPathDegrading, void());
  MOCK_CONST_METHOD0(WillingAndAbleToWrite, bool());
  MOCK_CONST_METHOD0(HasPendingHandshake, bool());
  MOCK_CONST_METHOD0(HasOpenDynamicStreams, bool());
  MOCK_METHOD1(OnSuccessfulVersionNegotiation,
               void(const ParsedQuicVersion& version));
  MOCK_METHOD2(OnConnectivityProbeReceived,
               void(const QuicSocketAddress& self_address,
                    const QuicSocketAddress& peer_address));
  MOCK_METHOD0(OnConfigNegotiated, void());
  MOCK_METHOD0(PostProcessAfterData, void());
  MOCK_METHOD0(OnAckNeedsRetransmittableFrame, void());
  MOCK_METHOD0(SendPing, void());
  MOCK_CONST_METHOD0(AllowSelfAddressChange, bool());
  MOCK_METHOD0(OnForwardProgressConfirmed, void());
};

class MockQuicConnectionHelper : public QuicConnectionHelperInterface {
 public:
  MockQuicConnectionHelper();
  MockQuicConnectionHelper(const MockQuicConnectionHelper&) = delete;
  MockQuicConnectionHelper& operator=(const MockQuicConnectionHelper&) = delete;
  ~MockQuicConnectionHelper() override;
  const QuicClock* GetClock() const override;
  QuicRandom* GetRandomGenerator() override;
  QuicBufferAllocator* GetStreamSendBufferAllocator() override;
  void AdvanceTime(QuicTime::Delta delta);

 private:
  MockClock clock_;
  MockRandom random_generator_;
  SimpleBufferAllocator buffer_allocator_;
};

class MockAlarmFactory : public QuicAlarmFactory {
 public:
  QuicAlarm* CreateAlarm(QuicAlarm::Delegate* delegate) override;
  QuicArenaScopedPtr<QuicAlarm> CreateAlarm(
      QuicArenaScopedPtr<QuicAlarm::Delegate> delegate,
      QuicConnectionArena* arena) override;

  // No-op alarm implementation
  class TestAlarm : public QuicAlarm {
   public:
    explicit TestAlarm(QuicArenaScopedPtr<QuicAlarm::Delegate> delegate)
        : QuicAlarm(std::move(delegate)) {}

    void SetImpl() override {}
    void CancelImpl() override {}

    using QuicAlarm::Fire;
  };

  void FireAlarm(QuicAlarm* alarm) {
    reinterpret_cast<TestAlarm*>(alarm)->Fire();
  }
};

class MockQuicConnection : public QuicConnection {
 public:
  // Uses a ConnectionId of 42 and 127.0.0.1:123.
  MockQuicConnection(MockQuicConnectionHelper* helper,
                     MockAlarmFactory* alarm_factory,
                     Perspective perspective);

  // Uses a ConnectionId of 42.
  MockQuicConnection(QuicSocketAddress address,
                     MockQuicConnectionHelper* helper,
                     MockAlarmFactory* alarm_factory,
                     Perspective perspective);

  // Uses 127.0.0.1:123.
  MockQuicConnection(QuicConnectionId connection_id,
                     MockQuicConnectionHelper* helper,
                     MockAlarmFactory* alarm_factory,
                     Perspective perspective);

  // Uses a ConnectionId of 42, and 127.0.0.1:123.
  MockQuicConnection(MockQuicConnectionHelper* helper,
                     MockAlarmFactory* alarm_factory,
                     Perspective perspective,
                     const ParsedQuicVersionVector& supported_versions);

  MockQuicConnection(QuicConnectionId connection_id,
                     QuicSocketAddress address,
                     MockQuicConnectionHelper* helper,
                     MockAlarmFactory* alarm_factory,
                     Perspective perspective,
                     const ParsedQuicVersionVector& supported_versions);
  MockQuicConnection(const MockQuicConnection&) = delete;
  MockQuicConnection& operator=(const MockQuicConnection&) = delete;

  ~MockQuicConnection() override;

  // If the constructor that uses a MockQuicConnectionHelper has been used then
  // this method
  // will advance the time of the MockClock.
  void AdvanceTime(QuicTime::Delta delta);

  MOCK_METHOD3(ProcessUdpPacket,
               void(const QuicSocketAddress& self_address,
                    const QuicSocketAddress& peer_address,
                    const QuicReceivedPacket& packet));
  MOCK_METHOD1(SendConnectionClose, void(QuicErrorCode error));
  MOCK_METHOD3(CloseConnection,
               void(QuicErrorCode error,
                    const QuicString& details,
                    ConnectionCloseBehavior connection_close_behavior));
  MOCK_METHOD3(SendConnectionClosePacket,
               void(QuicErrorCode error,
                    const QuicString& details,
                    AckBundling ack_mode));
  MOCK_METHOD3(SendRstStream,
               void(QuicStreamId id,
                    QuicRstStreamErrorCode error,
                    QuicStreamOffset bytes_written));
  MOCK_METHOD3(SendGoAway,
               void(QuicErrorCode error,
                    QuicStreamId last_good_stream_id,
                    const QuicString& reason));
  MOCK_METHOD1(SendBlocked, void(QuicStreamId id));
  MOCK_METHOD2(SendWindowUpdate,
               void(QuicStreamId id, QuicStreamOffset byte_offset));
  MOCK_METHOD0(OnCanWrite, void());
  MOCK_METHOD1(SendConnectivityProbingResponsePacket,
               void(const QuicSocketAddress& peer_address));
  MOCK_METHOD2(SendConnectivityProbingPacket,
               bool(QuicPacketWriter* probing_writer,
                    const QuicSocketAddress& peer_address));

  MOCK_METHOD1(OnSendConnectionState, void(const CachedNetworkParameters&));
  MOCK_METHOD2(ResumeConnectionState,
               void(const CachedNetworkParameters&, bool));
  MOCK_METHOD1(SetMaxPacingRate, void(QuicBandwidth));

  MOCK_METHOD2(OnStreamReset, void(QuicStreamId, QuicRstStreamErrorCode));
  MOCK_METHOD1(SendControlFrame, bool(const QuicFrame& frame));
  MOCK_METHOD2(SendMessage, MessageStatus(QuicMessageId, QuicStringPiece));

  MOCK_METHOD1(OnError, void(QuicFramer* framer));
  void QuicConnection_OnError(QuicFramer* framer) {
    QuicConnection::OnError(framer);
  }

  void ReallyProcessUdpPacket(const QuicSocketAddress& self_address,
                              const QuicSocketAddress& peer_address,
                              const QuicReceivedPacket& packet) {
    QuicConnection::ProcessUdpPacket(self_address, peer_address, packet);
  }

  bool OnProtocolVersionMismatch(ParsedQuicVersion version) override;

  bool ReallySendControlFrame(const QuicFrame& frame) {
    return QuicConnection::SendControlFrame(frame);
  }

  bool ReallySendConnectivityProbingPacket(
      QuicPacketWriter* probing_writer,
      const QuicSocketAddress& peer_address) {
    return QuicConnection::SendConnectivityProbingPacket(probing_writer,
                                                         peer_address);
  }

  void ReallySendConnectivityProbingResponsePacket(
      const QuicSocketAddress& peer_address) {
    QuicConnection::SendConnectivityProbingResponsePacket(peer_address);
  }
  MOCK_METHOD1(OnPathResponseFrame, bool(const QuicPathResponseFrame&));
};

class PacketSavingConnection : public MockQuicConnection {
 public:
  PacketSavingConnection(MockQuicConnectionHelper* helper,
                         MockAlarmFactory* alarm_factory,
                         Perspective perspective);

  PacketSavingConnection(MockQuicConnectionHelper* helper,
                         MockAlarmFactory* alarm_factory,
                         Perspective perspective,
                         const ParsedQuicVersionVector& supported_versions);
  PacketSavingConnection(const PacketSavingConnection&) = delete;
  PacketSavingConnection& operator=(const PacketSavingConnection&) = delete;

  ~PacketSavingConnection() override;

  void SendOrQueuePacket(SerializedPacket* packet) override;

  std::vector<std::unique_ptr<QuicEncryptedPacket>> encrypted_packets_;
};

class MockQuicSession : public QuicSession {
 public:
  // Takes ownership of |connection|.
  MockQuicSession(QuicConnection* connection, bool create_mock_crypto_stream);

  // Takes ownership of |connection|.
  explicit MockQuicSession(QuicConnection* connection);
  MockQuicSession(const MockQuicSession&) = delete;
  MockQuicSession& operator=(const MockQuicSession&) = delete;
  ~MockQuicSession() override;

  QuicCryptoStream* GetMutableCryptoStream() override;
  const QuicCryptoStream* GetCryptoStream() const override;
  void SetCryptoStream(QuicCryptoStream* crypto_stream);

  MOCK_METHOD3(OnConnectionClosed,
               void(QuicErrorCode error,
                    const QuicString& error_details,
                    ConnectionCloseSource source));
  MOCK_METHOD1(CreateIncomingStream, QuicStream*(QuicStreamId id));
  MOCK_METHOD1(ShouldCreateIncomingStream2, bool(QuicStreamId id));
  MOCK_METHOD0(ShouldCreateOutgoingStream2, bool());
  MOCK_METHOD5(WritevData,
               QuicConsumedData(QuicStream* stream,
                                QuicStreamId id,
                                size_t write_length,
                                QuicStreamOffset offset,
                                StreamSendingState state));

  MOCK_METHOD3(SendRstStream,
               void(QuicStreamId stream_id,
                    QuicRstStreamErrorCode error,
                    QuicStreamOffset bytes_written));

  MOCK_METHOD2(OnStreamHeaders,
               void(QuicStreamId stream_id, QuicStringPiece headers_data));
  MOCK_METHOD2(OnStreamHeadersPriority,
               void(QuicStreamId stream_id, spdy::SpdyPriority priority));
  MOCK_METHOD3(OnStreamHeadersComplete,
               void(QuicStreamId stream_id, bool fin, size_t frame_len));
  MOCK_CONST_METHOD0(IsCryptoHandshakeConfirmed, bool());

  using QuicSession::ActivateStream;

  // Returns a QuicConsumedData that indicates all of |write_length| (and |fin|
  // if set) has been consumed.
  static QuicConsumedData ConsumeData(QuicStream* stream,
                                      QuicStreamId id,
                                      size_t write_length,
                                      QuicStreamOffset offset,
                                      StreamSendingState state);

 private:
  std::unique_ptr<QuicCryptoStream> crypto_stream_;
};

class MockQuicCryptoStream : public QuicCryptoStream {
 public:
  explicit MockQuicCryptoStream(QuicSession* session);

  ~MockQuicCryptoStream() override;

  QuicLongHeaderType GetLongHeaderType(QuicStreamOffset offset) const override;
  bool encryption_established() const override;
  bool handshake_confirmed() const override;
  const QuicCryptoNegotiatedParameters& crypto_negotiated_params()
      const override;
  CryptoMessageParser* crypto_message_parser() override;

 private:
  QuicReferenceCountedPointer<QuicCryptoNegotiatedParameters> params_;
  CryptoFramer crypto_framer_;
};

class MockQuicSpdySession : public QuicSpdySession {
 public:
  // Takes ownership of |connection|.
  explicit MockQuicSpdySession(QuicConnection* connection);
  // Takes ownership of |connection|.
  MockQuicSpdySession(QuicConnection* connection,
                      bool create_mock_crypto_stream);
  MockQuicSpdySession(const MockQuicSpdySession&) = delete;
  MockQuicSpdySession& operator=(const MockQuicSpdySession&) = delete;
  ~MockQuicSpdySession() override;

  QuicCryptoStream* GetMutableCryptoStream() override;
  const QuicCryptoStream* GetCryptoStream() const override;
  void SetCryptoStream(QuicCryptoStream* crypto_stream);
  const spdy::SpdyHeaderBlock& GetWriteHeaders() { return write_headers_; }

  // From QuicSession.
  MOCK_METHOD3(OnConnectionClosed,
               void(QuicErrorCode error,
                    const QuicString& error_details,
                    ConnectionCloseSource source));
  MOCK_METHOD1(CreateIncomingStream, QuicSpdyStream*(QuicStreamId id));
  MOCK_METHOD0(CreateOutgoingBidirectionalStream, QuicSpdyStream*());
  MOCK_METHOD0(CreateOutgoingUnidirectionalStream, QuicSpdyStream*());
  MOCK_METHOD1(ShouldCreateIncomingStream, bool(QuicStreamId id));
  MOCK_METHOD0(ShouldCreateOutgoingStream, bool());
  MOCK_METHOD5(WritevData,
               QuicConsumedData(QuicStream* stream,
                                QuicStreamId id,
                                size_t write_length,
                                QuicStreamOffset offset,
                                StreamSendingState state));

  MOCK_METHOD3(SendRstStream,
               void(QuicStreamId stream_id,
                    QuicRstStreamErrorCode error,
                    QuicStreamOffset bytes_written));

  MOCK_METHOD2(OnStreamHeaders,
               void(QuicStreamId stream_id, QuicStringPiece headers_data));
  MOCK_METHOD2(OnStreamHeadersPriority,
               void(QuicStreamId stream_id, spdy::SpdyPriority priority));
  MOCK_METHOD3(OnStreamHeadersComplete,
               void(QuicStreamId stream_id, bool fin, size_t frame_len));
  MOCK_METHOD4(OnStreamHeaderList,
               void(QuicStreamId stream_id,
                    bool fin,
                    size_t frame_len,
                    const QuicHeaderList& header_list));
  MOCK_CONST_METHOD0(IsCryptoHandshakeConfirmed, bool());
  MOCK_METHOD2(OnPromiseHeaders,
               void(QuicStreamId stream_id, QuicStringPiece headers_data));
  MOCK_METHOD3(OnPromiseHeadersComplete,
               void(QuicStreamId stream_id,
                    QuicStreamId promised_stream_id,
                    size_t frame_len));
  MOCK_METHOD4(OnPromiseHeaderList,
               void(QuicStreamId stream_id,
                    QuicStreamId promised_stream_id,
                    size_t frame_len,
                    const QuicHeaderList& header_list));
  MOCK_METHOD2(OnPriorityFrame,
               void(QuicStreamId id, spdy::SpdyPriority priority));

  // Methods taking non-copyable types like spdy::SpdyHeaderBlock by value
  // cannot be mocked directly.
  size_t WriteHeaders(QuicStreamId id,
                      spdy::SpdyHeaderBlock headers,
                      bool fin,
                      spdy::SpdyPriority priority,
                      QuicReferenceCountedPointer<QuicAckListenerInterface>
                          ack_listener) override;
  MOCK_METHOD5(
      WriteHeadersMock,
      size_t(QuicStreamId id,
             const spdy::SpdyHeaderBlock& headers,
             bool fin,
             spdy::SpdyPriority priority,
             const QuicReferenceCountedPointer<QuicAckListenerInterface>&
                 ack_listener));
  MOCK_METHOD1(OnHeadersHeadOfLineBlocking, void(QuicTime::Delta delta));
  MOCK_METHOD4(
      OnStreamFrameData,
      void(QuicStreamId stream_id, const char* data, size_t len, bool fin));

  using QuicSession::ActivateStream;

 private:
  std::unique_ptr<QuicCryptoStream> crypto_stream_;
  spdy::SpdyHeaderBlock write_headers_;
};

class TestQuicSpdyServerSession : public QuicServerSessionBase {
 public:
  // Takes ownership of |connection|.
  TestQuicSpdyServerSession(QuicConnection* connection,
                            const QuicConfig& config,
                            const ParsedQuicVersionVector& supported_versions,
                            const QuicCryptoServerConfig* crypto_config,
                            QuicCompressedCertsCache* compressed_certs_cache);
  TestQuicSpdyServerSession(const TestQuicSpdyServerSession&) = delete;
  TestQuicSpdyServerSession& operator=(const TestQuicSpdyServerSession&) =
      delete;
  ~TestQuicSpdyServerSession() override;

  MOCK_METHOD1(CreateIncomingStream, QuicSpdyStream*(QuicStreamId id));
  MOCK_METHOD0(CreateOutgoingBidirectionalStream, QuicSpdyStream*());
  MOCK_METHOD0(CreateOutgoingUnidirectionalStream, QuicSpdyStream*());
  QuicCryptoServerStreamBase* CreateQuicCryptoServerStream(
      const QuicCryptoServerConfig* crypto_config,
      QuicCompressedCertsCache* compressed_certs_cache) override;

  // Override to not send max header list size.
  void OnCryptoHandshakeEvent(CryptoHandshakeEvent event) override;

  QuicCryptoServerStream* GetMutableCryptoStream() override;

  const QuicCryptoServerStream* GetCryptoStream() const override;

  MockQuicCryptoServerStreamHelper* helper() { return &helper_; }

 private:
  MockQuicSessionVisitor visitor_;
  MockQuicCryptoServerStreamHelper helper_;
};

// A test implementation of QuicClientPushPromiseIndex::Delegate.
class TestPushPromiseDelegate : public QuicClientPushPromiseIndex::Delegate {
 public:
  // |match| sets the validation result for checking whether designated header
  // fields match for promise request and client request.
  explicit TestPushPromiseDelegate(bool match);

  bool CheckVary(const spdy::SpdyHeaderBlock& client_request,
                 const spdy::SpdyHeaderBlock& promise_request,
                 const spdy::SpdyHeaderBlock& promise_response) override;

  void OnRendezvousResult(QuicSpdyStream* stream) override;

  QuicSpdyStream* rendezvous_stream() { return rendezvous_stream_; }
  bool rendezvous_fired() { return rendezvous_fired_; }

 private:
  bool match_;
  bool rendezvous_fired_;
  QuicSpdyStream* rendezvous_stream_;
};

class TestQuicSpdyClientSession : public QuicSpdyClientSessionBase {
 public:
  TestQuicSpdyClientSession(QuicConnection* connection,
                            const QuicConfig& config,
                            const QuicServerId& server_id,
                            QuicCryptoClientConfig* crypto_config);
  TestQuicSpdyClientSession(const TestQuicSpdyClientSession&) = delete;
  TestQuicSpdyClientSession& operator=(const TestQuicSpdyClientSession&) =
      delete;
  ~TestQuicSpdyClientSession() override;

  bool IsAuthorized(const QuicString& authority) override;

  // QuicSpdyClientSessionBase
  MOCK_METHOD1(OnProofValid,
               void(const QuicCryptoClientConfig::CachedState& cached));
  MOCK_METHOD1(OnProofVerifyDetailsAvailable,
               void(const ProofVerifyDetails& verify_details));

  // TestQuicSpdyClientSession
  MOCK_METHOD1(CreateIncomingStream, QuicSpdyStream*(QuicStreamId id));
  MOCK_METHOD0(CreateOutgoingBidirectionalStream, QuicSpdyStream*());
  MOCK_METHOD0(CreateOutgoingUnidirectionalStream, QuicSpdyStream*());
  MOCK_METHOD1(ShouldCreateIncomingStream, bool(QuicStreamId id));
  MOCK_METHOD0(ShouldCreateOutgoingStream, bool());

  // Override to not send max header list size.
  void OnCryptoHandshakeEvent(CryptoHandshakeEvent event) override;
  QuicCryptoClientStream* GetMutableCryptoStream() override;
  const QuicCryptoClientStream* GetCryptoStream() const override;

 private:
  std::unique_ptr<QuicCryptoClientStream> crypto_stream_;
  QuicClientPushPromiseIndex push_promise_index_;
};

class MockPacketWriter : public QuicPacketWriter {
 public:
  MockPacketWriter();
  MockPacketWriter(const MockPacketWriter&) = delete;
  MockPacketWriter& operator=(const MockPacketWriter&) = delete;
  ~MockPacketWriter() override;

  MOCK_METHOD5(WritePacket,
               WriteResult(const char* buffer,
                           size_t buf_len,
                           const QuicIpAddress& self_address,
                           const QuicSocketAddress& peer_address,
                           PerPacketOptions* options));
  MOCK_CONST_METHOD0(IsWriteBlockedDataBuffered, bool());
  MOCK_CONST_METHOD0(IsWriteBlocked, bool());
  MOCK_METHOD0(SetWritable, void());
  MOCK_CONST_METHOD1(GetMaxPacketSize,
                     QuicByteCount(const QuicSocketAddress& peer_address));
  MOCK_CONST_METHOD0(SupportsReleaseTime, bool());
  MOCK_CONST_METHOD0(IsBatchMode, bool());
  MOCK_METHOD2(GetNextWriteLocation,
               char*(const QuicIpAddress& self_address,
                     const QuicSocketAddress& peer_address));
  MOCK_METHOD0(Flush, WriteResult());
};

class MockSendAlgorithm : public SendAlgorithmInterface {
 public:
  MockSendAlgorithm();
  MockSendAlgorithm(const MockSendAlgorithm&) = delete;
  MockSendAlgorithm& operator=(const MockSendAlgorithm&) = delete;
  ~MockSendAlgorithm() override;

  MOCK_METHOD2(SetFromConfig,
               void(const QuicConfig& config, Perspective perspective));
  MOCK_METHOD1(SetNumEmulatedConnections, void(int num_connections));
  MOCK_METHOD1(SetInitialCongestionWindowInPackets,
               void(QuicPacketCount packets));
  MOCK_METHOD1(SetMaxCongestionWindow,
               void(QuicByteCount max_congestion_window));
  MOCK_METHOD5(OnCongestionEvent,
               void(bool rtt_updated,
                    QuicByteCount bytes_in_flight,
                    QuicTime event_time,
                    const AckedPacketVector& acked_packets,
                    const LostPacketVector& lost_packets));
  MOCK_METHOD5(OnPacketSent,
               void(QuicTime,
                    QuicByteCount,
                    QuicPacketNumber,
                    QuicByteCount,
                    HasRetransmittableData));
  MOCK_METHOD1(OnRetransmissionTimeout, void(bool));
  MOCK_METHOD0(OnConnectionMigration, void());
  MOCK_METHOD0(RevertRetransmissionTimeout, void());
  MOCK_METHOD1(CanSend, bool(QuicByteCount));
  MOCK_CONST_METHOD1(PacingRate, QuicBandwidth(QuicByteCount));
  MOCK_CONST_METHOD0(BandwidthEstimate, QuicBandwidth(void));
  MOCK_CONST_METHOD0(HasReliableBandwidthEstimate, bool());
  MOCK_METHOD1(OnRttUpdated, void(QuicPacketNumber));
  MOCK_CONST_METHOD0(GetCongestionWindow, QuicByteCount());
  MOCK_CONST_METHOD0(GetDebugState, QuicString());
  MOCK_CONST_METHOD0(InSlowStart, bool());
  MOCK_CONST_METHOD0(InRecovery, bool());
  MOCK_CONST_METHOD0(ShouldSendProbingPacket, bool());
  MOCK_CONST_METHOD0(GetSlowStartThreshold, QuicByteCount());
  MOCK_CONST_METHOD0(GetCongestionControlType, CongestionControlType());
  MOCK_METHOD2(AdjustNetworkParameters, void(QuicBandwidth, QuicTime::Delta));
  MOCK_METHOD1(OnApplicationLimited, void(QuicByteCount));
};

class MockLossAlgorithm : public LossDetectionInterface {
 public:
  MockLossAlgorithm();
  MockLossAlgorithm(const MockLossAlgorithm&) = delete;
  MockLossAlgorithm& operator=(const MockLossAlgorithm&) = delete;
  ~MockLossAlgorithm() override;

  MOCK_CONST_METHOD0(GetLossDetectionType, LossDetectionType());
  MOCK_METHOD6(DetectLosses,
               void(const QuicUnackedPacketMap& unacked_packets,
                    QuicTime time,
                    const RttStats& rtt_stats,
                    QuicPacketNumber largest_recently_acked,
                    const AckedPacketVector& packets_acked,
                    LostPacketVector* packets_lost));
  MOCK_CONST_METHOD0(GetLossTimeout, QuicTime());
  MOCK_METHOD4(SpuriousRetransmitDetected,
               void(const QuicUnackedPacketMap&,
                    QuicTime,
                    const RttStats&,
                    QuicPacketNumber));
};

class MockAckListener : public QuicAckListenerInterface {
 public:
  MockAckListener();
  MockAckListener(const MockAckListener&) = delete;
  MockAckListener& operator=(const MockAckListener&) = delete;

  MOCK_METHOD2(OnPacketAcked,
               void(int acked_bytes, QuicTime::Delta ack_delay_time));

  MOCK_METHOD1(OnPacketRetransmitted, void(int retransmitted_bytes));

 protected:
  // Object is ref counted.
  ~MockAckListener() override;
};

class MockNetworkChangeVisitor
    : public QuicSentPacketManager::NetworkChangeVisitor {
 public:
  MockNetworkChangeVisitor();
  MockNetworkChangeVisitor(const MockNetworkChangeVisitor&) = delete;
  MockNetworkChangeVisitor& operator=(const MockNetworkChangeVisitor&) = delete;
  ~MockNetworkChangeVisitor() override;

  MOCK_METHOD0(OnCongestionChange, void());
  MOCK_METHOD1(OnPathMtuIncreased, void(QuicPacketLength));
};

class MockQuicConnectionDebugVisitor : public QuicConnectionDebugVisitor {
 public:
  MockQuicConnectionDebugVisitor();
  ~MockQuicConnectionDebugVisitor() override;

  MOCK_METHOD1(OnFrameAddedToPacket, void(const QuicFrame&));

  MOCK_METHOD4(OnPacketSent,
               void(const SerializedPacket&,
                    QuicPacketNumber,
                    TransmissionType,
                    QuicTime));

  MOCK_METHOD0(OnPingSent, void());

  MOCK_METHOD3(OnPacketReceived,
               void(const QuicSocketAddress&,
                    const QuicSocketAddress&,
                    const QuicEncryptedPacket&));

  MOCK_METHOD1(OnIncorrectConnectionId, void(QuicConnectionId));

  MOCK_METHOD1(OnProtocolVersionMismatch, void(ParsedQuicVersion));

  MOCK_METHOD1(OnPacketHeader, void(const QuicPacketHeader& header));

  MOCK_METHOD1(OnSuccessfulVersionNegotiation, void(const ParsedQuicVersion&));

  MOCK_METHOD1(OnStreamFrame, void(const QuicStreamFrame&));

  MOCK_METHOD1(OnAckFrame, void(const QuicAckFrame& frame));

  MOCK_METHOD1(OnStopWaitingFrame, void(const QuicStopWaitingFrame&));

  MOCK_METHOD1(OnRstStreamFrame, void(const QuicRstStreamFrame&));

  MOCK_METHOD1(OnConnectionCloseFrame, void(const QuicConnectionCloseFrame&));

  MOCK_METHOD1(OnApplicationCloseFrame, void(const QuicApplicationCloseFrame&));

  MOCK_METHOD1(OnStopSendingFrame, void(const QuicStopSendingFrame&));

  MOCK_METHOD1(OnPathChallengeFrame, void(const QuicPathChallengeFrame&));

  MOCK_METHOD1(OnPathResponseFrame, void(const QuicPathResponseFrame&));

  MOCK_METHOD1(OnPublicResetPacket, void(const QuicPublicResetPacket&));

  MOCK_METHOD1(OnVersionNegotiationPacket,
               void(const QuicVersionNegotiationPacket&));
};

class MockReceivedPacketManager : public QuicReceivedPacketManager {
 public:
  explicit MockReceivedPacketManager(QuicConnectionStats* stats);
  ~MockReceivedPacketManager() override;

  MOCK_METHOD2(RecordPacketReceived,
               void(const QuicPacketHeader& header, QuicTime receipt_time));
  MOCK_METHOD1(IsMissing, bool(QuicPacketNumber packet_number));
  MOCK_METHOD1(IsAwaitingPacket, bool(QuicPacketNumber packet_number));
  MOCK_METHOD1(UpdatePacketInformationSentByPeer,
               void(const QuicStopWaitingFrame& stop_waiting));
  MOCK_CONST_METHOD0(HasNewMissingPackets, bool(void));
  MOCK_CONST_METHOD0(ack_frame_updated, bool(void));
};

class MockConnectionCloseDelegate
    : public QuicConnectionCloseDelegateInterface {
 public:
  MockConnectionCloseDelegate();
  ~MockConnectionCloseDelegate() override;

  MOCK_METHOD3(OnUnrecoverableError,
               void(QuicErrorCode,
                    const QuicString&,
                    ConnectionCloseSource source));
};

class MockPacketCreatorDelegate : public QuicPacketCreator::DelegateInterface {
 public:
  MockPacketCreatorDelegate();
  MockPacketCreatorDelegate(const MockPacketCreatorDelegate&) = delete;
  MockPacketCreatorDelegate& operator=(const MockPacketCreatorDelegate&) =
      delete;
  ~MockPacketCreatorDelegate() override;

  MOCK_METHOD0(GetPacketBuffer, char*());
  MOCK_METHOD1(OnSerializedPacket, void(SerializedPacket* packet));
  MOCK_METHOD3(OnUnrecoverableError,
               void(QuicErrorCode,
                    const QuicString&,
                    ConnectionCloseSource source));
};

class MockSessionNotifier : public SessionNotifierInterface {
 public:
  MockSessionNotifier();
  ~MockSessionNotifier() override;

  MOCK_METHOD2(OnFrameAcked, bool(const QuicFrame&, QuicTime::Delta));
  MOCK_METHOD1(OnStreamFrameRetransmitted, void(const QuicStreamFrame&));
  MOCK_METHOD1(OnFrameLost, void(const QuicFrame&));
  MOCK_METHOD2(RetransmitFrames,
               void(const QuicFrames&, TransmissionType type));
  MOCK_CONST_METHOD1(IsFrameOutstanding, bool(const QuicFrame&));
  MOCK_CONST_METHOD0(HasUnackedCryptoData, bool());
};

// Creates a client session for testing.
//
// server_id: The server id associated with this stream.
// supports_stateless_rejects:  Does this client support stateless rejects.
// connection_start_time: The time to set for the connection clock.
//   Needed for strike-register nonce verification.  The client
//   connection_start_time should be synchronized witht the server
//   start time, otherwise nonce verification will fail.
// supported_versions: Set of QUIC versions this client supports.
// helper: Pointer to the MockQuicConnectionHelper to use for the session.
// crypto_client_config: Pointer to the crypto client config.
// client_connection: Pointer reference for newly created
//   connection.  This object will be owned by the
//   client_session.
// client_session: Pointer reference for the newly created client
//   session.  The new object will be owned by the caller.
void CreateClientSessionForTest(
    QuicServerId server_id,
    bool supports_stateless_rejects,
    QuicTime::Delta connection_start_time,
    const ParsedQuicVersionVector& supported_versions,
    MockQuicConnectionHelper* helper,
    MockAlarmFactory* alarm_factory,
    QuicCryptoClientConfig* crypto_client_config,
    PacketSavingConnection** client_connection,
    TestQuicSpdyClientSession** client_session);

// Creates a server session for testing.
//
// server_id: The server id associated with this stream.
// connection_start_time: The time to set for the connection clock.
//   Needed for strike-register nonce verification.  The server
//   connection_start_time should be synchronized witht the client
//   start time, otherwise nonce verification will fail.
// supported_versions: Set of QUIC versions this server supports.
// helper: Pointer to the MockQuicConnectionHelper to use for the session.
// crypto_server_config: Pointer to the crypto server config.
// server_connection: Pointer reference for newly created
//   connection.  This object will be owned by the
//   server_session.
// server_session: Pointer reference for the newly created server
//   session.  The new object will be owned by the caller.
void CreateServerSessionForTest(
    QuicServerId server_id,
    QuicTime::Delta connection_start_time,
    ParsedQuicVersionVector supported_versions,
    MockQuicConnectionHelper* helper,
    MockAlarmFactory* alarm_factory,
    QuicCryptoServerConfig* crypto_server_config,
    QuicCompressedCertsCache* compressed_certs_cache,
    PacketSavingConnection** server_connection,
    TestQuicSpdyServerSession** server_session);

// Verifies that the relative error of |actual| with respect to |expected| is
// no more than |margin|.

template <typename T>
void ExpectApproxEq(T expected, T actual, float relative_margin) {
  // If |relative_margin| > 1 and T is an unsigned type, the comparison will
  // underflow.
  ASSERT_LE(relative_margin, 1);
  ASSERT_GE(relative_margin, 0);

  T absolute_margin = expected * relative_margin;

  EXPECT_GE(expected + absolute_margin, actual);
  EXPECT_LE(expected - absolute_margin, actual);
}

template <typename T>
QuicHeaderList AsHeaderList(const T& container) {
  QuicHeaderList l;
  // No need to enforce header list size limits again in this handler.
  l.set_max_header_list_size(UINT_MAX);
  l.OnHeaderBlockStart();
  size_t total_size = 0;
  for (auto p : container) {
    total_size += p.first.size() + p.second.size();
    l.OnHeader(p.first, p.second);
  }
  l.OnHeaderBlockEnd(total_size, total_size);
  return l;
}

// Utility function that stores |str|'s data in |iov|.
inline void MakeIOVector(QuicStringPiece str, struct iovec* iov) {
  iov->iov_base = const_cast<char*>(str.data());
  iov->iov_len = static_cast<size_t>(str.size());
}

// Utilities that will adapt stream ids when http stream pairs are
// enabled.
QuicStreamId NextStreamId(QuicTransportVersion version);
QuicStreamId GetNthClientInitiatedStreamId(QuicTransportVersion version, int n);
QuicStreamId GetNthServerInitiatedStreamId(QuicTransportVersion version, int n);

}  // namespace test
}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_TEST_TOOLS_QUIC_TEST_UTILS_H_
