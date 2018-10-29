// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/quic_connection.h"

#include <errno.h>
#include <memory>
#include <ostream>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "net/base/net_errors.h"
#include "net/test/gtest_util.h"
#include "net/third_party/quic/core/congestion_control/loss_detection_interface.h"
#include "net/third_party/quic/core/congestion_control/send_algorithm_interface.h"
#include "net/third_party/quic/core/crypto/null_encrypter.h"
#include "net/third_party/quic/core/crypto/quic_decrypter.h"
#include "net/third_party/quic/core/crypto/quic_encrypter.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/quic/core/quic_simple_buffer_allocator.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/platform/api/quic_expect_bug.h"
#include "net/third_party/quic/platform/api/quic_flags.h"
#include "net/third_party/quic/platform/api/quic_logging.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/platform/api/quic_reference_counted.h"
#include "net/third_party/quic/platform/api/quic_str_cat.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"
#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/test_tools/mock_clock.h"
#include "net/third_party/quic/test_tools/mock_random.h"
#include "net/third_party/quic/test_tools/quic_config_peer.h"
#include "net/third_party/quic/test_tools/quic_connection_peer.h"
#include "net/third_party/quic/test_tools/quic_framer_peer.h"
#include "net/third_party/quic/test_tools/quic_packet_creator_peer.h"
#include "net/third_party/quic/test_tools/quic_packet_generator_peer.h"
#include "net/third_party/quic/test_tools/quic_sent_packet_manager_peer.h"
#include "net/third_party/quic/test_tools/quic_test_utils.h"
#include "net/third_party/quic/test_tools/simple_data_producer.h"
#include "net/third_party/quic/test_tools/simple_quic_framer.h"
#include "net/third_party/quic/test_tools/simple_session_notifier.h"
#include "testing/gmock_mutant.h"

using testing::_;
using testing::AnyNumber;
using testing::AtLeast;
using testing::DoAll;
using testing::Exactly;
using testing::Ge;
using testing::InSequence;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Lt;
using testing::Ref;
using testing::Return;
using testing::SaveArg;
using testing::SetArgPointee;
using testing::StrictMock;

namespace quic {
namespace test {
namespace {

const char data1[] = "foo";
const char data2[] = "bar";

const bool kHasStopWaiting = true;

const int kDefaultRetransmissionTimeMs = 500;

const QuicSocketAddress kPeerAddress =
    QuicSocketAddress(QuicIpAddress::Loopback6(),
                      /*port=*/12345);
const QuicSocketAddress kSelfAddress =
    QuicSocketAddress(QuicIpAddress::Loopback6(),
                      /*port=*/443);

Perspective InvertPerspective(Perspective perspective) {
  return perspective == Perspective::IS_CLIENT ? Perspective::IS_SERVER
                                               : Perspective::IS_CLIENT;
}

QuicStreamId GetNthClientInitiatedStreamId(int n,
                                           QuicTransportVersion version) {
  return QuicUtils::GetHeadersStreamId(version) + n * 2;
}

// TaggingEncrypter appends kTagSize bytes of |tag| to the end of each message.
class TaggingEncrypter : public QuicEncrypter {
 public:
  explicit TaggingEncrypter(uint8_t tag) : tag_(tag) {}
  TaggingEncrypter(const TaggingEncrypter&) = delete;
  TaggingEncrypter& operator=(const TaggingEncrypter&) = delete;

  ~TaggingEncrypter() override {}

  // QuicEncrypter interface.
  bool SetKey(QuicStringPiece key) override { return true; }

  bool SetNoncePrefix(QuicStringPiece nonce_prefix) override { return true; }

  bool SetIV(QuicStringPiece iv) override { return true; }

  bool EncryptPacket(QuicTransportVersion /*version*/,
                     QuicPacketNumber packet_number,
                     QuicStringPiece associated_data,
                     QuicStringPiece plaintext,
                     char* output,
                     size_t* output_length,
                     size_t max_output_length) override {
    const size_t len = plaintext.size() + kTagSize;
    if (max_output_length < len) {
      return false;
    }
    // Memmove is safe for inplace encryption.
    memmove(output, plaintext.data(), plaintext.size());
    output += plaintext.size();
    memset(output, tag_, kTagSize);
    *output_length = len;
    return true;
  }

  size_t GetKeySize() const override { return 0; }
  size_t GetNoncePrefixSize() const override { return 0; }
  size_t GetIVSize() const override { return 0; }

  size_t GetMaxPlaintextSize(size_t ciphertext_size) const override {
    return ciphertext_size - kTagSize;
  }

  size_t GetCiphertextSize(size_t plaintext_size) const override {
    return plaintext_size + kTagSize;
  }

  QuicStringPiece GetKey() const override { return QuicStringPiece(); }

  QuicStringPiece GetNoncePrefix() const override { return QuicStringPiece(); }

 private:
  enum {
    kTagSize = 12,
  };

  const uint8_t tag_;
};

// TaggingDecrypter ensures that the final kTagSize bytes of the message all
// have the same value and then removes them.
class TaggingDecrypter : public QuicDecrypter {
 public:
  ~TaggingDecrypter() override {}

  // QuicDecrypter interface
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

  bool DecryptPacket(QuicTransportVersion /*version*/,
                     QuicPacketNumber packet_number,
                     QuicStringPiece associated_data,
                     QuicStringPiece ciphertext,
                     char* output,
                     size_t* output_length,
                     size_t max_output_length) override {
    if (ciphertext.size() < kTagSize) {
      return false;
    }
    if (!CheckTag(ciphertext, GetTag(ciphertext))) {
      return false;
    }
    *output_length = ciphertext.size() - kTagSize;
    memcpy(output, ciphertext.data(), *output_length);
    return true;
  }

  size_t GetKeySize() const override { return 0; }
  size_t GetIVSize() const override { return 0; }
  QuicStringPiece GetKey() const override { return QuicStringPiece(); }
  QuicStringPiece GetNoncePrefix() const override { return QuicStringPiece(); }
  // Use a distinct value starting with 0xFFFFFF, which is never used by TLS.
  uint32_t cipher_id() const override { return 0xFFFFFFF0; }

 protected:
  virtual uint8_t GetTag(QuicStringPiece ciphertext) {
    return ciphertext.data()[ciphertext.size() - 1];
  }

 private:
  enum {
    kTagSize = 12,
  };

  bool CheckTag(QuicStringPiece ciphertext, uint8_t tag) {
    for (size_t i = ciphertext.size() - kTagSize; i < ciphertext.size(); i++) {
      if (ciphertext.data()[i] != tag) {
        return false;
      }
    }

    return true;
  }
};

// StringTaggingDecrypter ensures that the final kTagSize bytes of the message
// match the expected value.
class StrictTaggingDecrypter : public TaggingDecrypter {
 public:
  explicit StrictTaggingDecrypter(uint8_t tag) : tag_(tag) {}
  ~StrictTaggingDecrypter() override {}

  // TaggingQuicDecrypter
  uint8_t GetTag(QuicStringPiece ciphertext) override { return tag_; }

  // Use a distinct value starting with 0xFFFFFF, which is never used by TLS.
  uint32_t cipher_id() const override { return 0xFFFFFFF1; }

 private:
  const uint8_t tag_;
};

class TestConnectionHelper : public QuicConnectionHelperInterface {
 public:
  TestConnectionHelper(MockClock* clock, MockRandom* random_generator)
      : clock_(clock), random_generator_(random_generator) {
    clock_->AdvanceTime(QuicTime::Delta::FromSeconds(1));
  }
  TestConnectionHelper(const TestConnectionHelper&) = delete;
  TestConnectionHelper& operator=(const TestConnectionHelper&) = delete;

  // QuicConnectionHelperInterface
  const QuicClock* GetClock() const override { return clock_; }

  QuicRandom* GetRandomGenerator() override { return random_generator_; }

  QuicBufferAllocator* GetStreamSendBufferAllocator() override {
    return &buffer_allocator_;
  }

 private:
  MockClock* clock_;
  MockRandom* random_generator_;
  SimpleBufferAllocator buffer_allocator_;
};

class TestAlarmFactory : public QuicAlarmFactory {
 public:
  class TestAlarm : public QuicAlarm {
   public:
    explicit TestAlarm(QuicArenaScopedPtr<QuicAlarm::Delegate> delegate)
        : QuicAlarm(std::move(delegate)) {}

    void SetImpl() override {}
    void CancelImpl() override {}
    using QuicAlarm::Fire;
  };

  TestAlarmFactory() {}
  TestAlarmFactory(const TestAlarmFactory&) = delete;
  TestAlarmFactory& operator=(const TestAlarmFactory&) = delete;

  QuicAlarm* CreateAlarm(QuicAlarm::Delegate* delegate) override {
    return new TestAlarm(QuicArenaScopedPtr<QuicAlarm::Delegate>(delegate));
  }

  QuicArenaScopedPtr<QuicAlarm> CreateAlarm(
      QuicArenaScopedPtr<QuicAlarm::Delegate> delegate,
      QuicConnectionArena* arena) override {
    return arena->New<TestAlarm>(std::move(delegate));
  }
};

class TestPacketWriter : public QuicPacketWriter {
 public:
  TestPacketWriter(ParsedQuicVersion version, MockClock* clock)
      : version_(version),
        framer_(SupportedVersions(version_), Perspective::IS_SERVER),
        last_packet_size_(0),
        write_blocked_(false),
        write_should_fail_(false),
        block_on_next_write_(false),
        next_packet_too_large_(false),
        always_get_packet_too_large_(false),
        is_write_blocked_data_buffered_(false),
        final_bytes_of_last_packet_(0),
        final_bytes_of_previous_packet_(0),
        use_tagging_decrypter_(false),
        packets_write_attempts_(0),
        clock_(clock),
        write_pause_time_delta_(QuicTime::Delta::Zero()),
        max_packet_size_(kMaxPacketSize) {}
  TestPacketWriter(const TestPacketWriter&) = delete;
  TestPacketWriter& operator=(const TestPacketWriter&) = delete;

  // QuicPacketWriter interface
  WriteResult WritePacket(const char* buffer,
                          size_t buf_len,
                          const QuicIpAddress& self_address,
                          const QuicSocketAddress& peer_address,
                          PerPacketOptions* options) override {
    QuicEncryptedPacket packet(buffer, buf_len);
    ++packets_write_attempts_;

    if (packet.length() >= sizeof(final_bytes_of_last_packet_)) {
      final_bytes_of_previous_packet_ = final_bytes_of_last_packet_;
      memcpy(&final_bytes_of_last_packet_, packet.data() + packet.length() - 4,
             sizeof(final_bytes_of_last_packet_));
    }

    if (use_tagging_decrypter_) {
      framer_.framer()->SetDecrypter(ENCRYPTION_NONE,
                                     QuicMakeUnique<TaggingDecrypter>());
    }
    EXPECT_TRUE(framer_.ProcessPacket(packet));
    if (block_on_next_write_) {
      write_blocked_ = true;
      block_on_next_write_ = false;
    }
    if (next_packet_too_large_) {
      next_packet_too_large_ = false;
      return WriteResult(WRITE_STATUS_ERROR, net::ERR_MSG_TOO_BIG);
    }
    if (always_get_packet_too_large_) {
      LOG(ERROR) << "RETURNING TOO BIG";
      return WriteResult(WRITE_STATUS_ERROR, net::ERR_MSG_TOO_BIG);
    }
    if (IsWriteBlocked()) {
      return WriteResult(WRITE_STATUS_BLOCKED, -1);
    }

    if (ShouldWriteFail()) {
      return WriteResult(WRITE_STATUS_ERROR, 0);
    }

    last_packet_size_ = packet.length();
    last_packet_header_ = framer_.header();

    if (!write_pause_time_delta_.IsZero()) {
      clock_->AdvanceTime(write_pause_time_delta_);
    }
    return WriteResult(WRITE_STATUS_OK, last_packet_size_);
  }

  bool IsWriteBlockedDataBuffered() const override {
    return is_write_blocked_data_buffered_;
  }

  bool ShouldWriteFail() { return write_should_fail_; }

  bool IsWriteBlocked() const override { return write_blocked_; }

  void SetWriteBlocked() { write_blocked_ = true; }

  void SetWritable() override { write_blocked_ = false; }

  void SetShouldWriteFail() { write_should_fail_ = true; }

  QuicByteCount GetMaxPacketSize(
      const QuicSocketAddress& /*peer_address*/) const override {
    return max_packet_size_;
  }

  bool SupportsReleaseTime() const { return false; }

  bool IsBatchMode() const override { return false; }

  char* GetNextWriteLocation(const QuicIpAddress& self_address,
                             const QuicSocketAddress& peer_address) override {
    return nullptr;
  }

  WriteResult Flush() override { return WriteResult(WRITE_STATUS_OK, 0); }

  void BlockOnNextWrite() { block_on_next_write_ = true; }

  void SimulateNextPacketTooLarge() { next_packet_too_large_ = true; }

  void AlwaysGetPacketTooLarge() { always_get_packet_too_large_ = true; }

  // Sets the amount of time that the writer should before the actual write.
  void SetWritePauseTimeDelta(QuicTime::Delta delta) {
    write_pause_time_delta_ = delta;
  }

  const QuicPacketHeader& header() { return framer_.header(); }

  size_t frame_count() const { return framer_.num_frames(); }

  const std::vector<QuicAckFrame>& ack_frames() const {
    return framer_.ack_frames();
  }

  const std::vector<QuicStopWaitingFrame>& stop_waiting_frames() const {
    return framer_.stop_waiting_frames();
  }

  const std::vector<QuicConnectionCloseFrame>& connection_close_frames() const {
    return framer_.connection_close_frames();
  }

  const std::vector<QuicRstStreamFrame>& rst_stream_frames() const {
    return framer_.rst_stream_frames();
  }

  const std::vector<std::unique_ptr<QuicStreamFrame>>& stream_frames() const {
    return framer_.stream_frames();
  }

  const std::vector<QuicPingFrame>& ping_frames() const {
    return framer_.ping_frames();
  }

  const std::vector<QuicMessageFrame>& message_frames() const {
    return framer_.message_frames();
  }

  const std::vector<QuicWindowUpdateFrame>& window_update_frames() const {
    return framer_.window_update_frames();
  }

  const std::vector<QuicPaddingFrame>& padding_frames() const {
    return framer_.padding_frames();
  }

  const std::vector<QuicPathChallengeFrame>& path_challenge_frames() const {
    return framer_.path_challenge_frames();
  }

  const std::vector<QuicPathResponseFrame>& path_response_frames() const {
    return framer_.path_response_frames();
  }

  size_t last_packet_size() { return last_packet_size_; }

  const QuicPacketHeader& last_packet_header() const {
    return last_packet_header_;
  }

  const QuicVersionNegotiationPacket* version_negotiation_packet() {
    return framer_.version_negotiation_packet();
  }

  void set_is_write_blocked_data_buffered(bool buffered) {
    is_write_blocked_data_buffered_ = buffered;
  }

  void set_perspective(Perspective perspective) {
    // We invert perspective here, because the framer needs to parse packets
    // we send.
    QuicFramerPeer::SetPerspective(framer_.framer(),
                                   InvertPerspective(perspective));
  }

  // final_bytes_of_last_packet_ returns the last four bytes of the previous
  // packet as a little-endian, uint32_t. This is intended to be used with a
  // TaggingEncrypter so that tests can determine which encrypter was used for
  // a given packet.
  uint32_t final_bytes_of_last_packet() { return final_bytes_of_last_packet_; }

  // Returns the final bytes of the second to last packet.
  uint32_t final_bytes_of_previous_packet() {
    return final_bytes_of_previous_packet_;
  }

  void use_tagging_decrypter() { use_tagging_decrypter_ = true; }

  uint32_t packets_write_attempts() { return packets_write_attempts_; }

  void Reset() { framer_.Reset(); }

  void SetSupportedVersions(const ParsedQuicVersionVector& versions) {
    framer_.SetSupportedVersions(versions);
  }

  void set_max_packet_size(QuicByteCount max_packet_size) {
    max_packet_size_ = max_packet_size;
  }

 private:
  ParsedQuicVersion version_;
  SimpleQuicFramer framer_;
  size_t last_packet_size_;
  QuicPacketHeader last_packet_header_;
  bool write_blocked_;
  bool write_should_fail_;
  bool block_on_next_write_;
  bool next_packet_too_large_;
  bool always_get_packet_too_large_;
  bool is_write_blocked_data_buffered_;
  uint32_t final_bytes_of_last_packet_;
  uint32_t final_bytes_of_previous_packet_;
  bool use_tagging_decrypter_;
  uint32_t packets_write_attempts_;
  MockClock* clock_;
  // If non-zero, the clock will pause during WritePacket for this amount of
  // time.
  QuicTime::Delta write_pause_time_delta_;
  QuicByteCount max_packet_size_;
};

class TestConnection : public QuicConnection {
 public:
  TestConnection(QuicConnectionId connection_id,
                 QuicSocketAddress address,
                 TestConnectionHelper* helper,
                 TestAlarmFactory* alarm_factory,
                 TestPacketWriter* writer,
                 Perspective perspective,
                 ParsedQuicVersion version)
      : QuicConnection(connection_id,
                       address,
                       helper,
                       alarm_factory,
                       writer,
                       /* owns_writer= */ false,
                       perspective,
                       SupportedVersions(version)),
        notifier_(nullptr) {
    writer->set_perspective(perspective);
    SetEncrypter(ENCRYPTION_FORWARD_SECURE,
                 QuicMakeUnique<NullEncrypter>(perspective));
    SetDataProducer(&producer_);
  }
  TestConnection(const TestConnection&) = delete;
  TestConnection& operator=(const TestConnection&) = delete;

  void SendAck() { QuicConnectionPeer::SendAck(this); }

  void SetSendAlgorithm(SendAlgorithmInterface* send_algorithm) {
    QuicConnectionPeer::SetSendAlgorithm(this, send_algorithm);
  }

  void SetLossAlgorithm(LossDetectionInterface* loss_algorithm) {
    QuicConnectionPeer::SetLossAlgorithm(this, loss_algorithm);
  }

  void SendPacket(EncryptionLevel level,
                  QuicPacketNumber packet_number,
                  std::unique_ptr<QuicPacket> packet,
                  HasRetransmittableData retransmittable,
                  bool has_ack,
                  bool has_pending_frames) {
    char buffer[kMaxPacketSize];
    size_t encrypted_length =
        QuicConnectionPeer::GetFramer(this)->EncryptPayload(
            ENCRYPTION_NONE, packet_number, *packet, buffer, kMaxPacketSize);
    SerializedPacket serialized_packet(
        packet_number, PACKET_4BYTE_PACKET_NUMBER, buffer, encrypted_length,
        has_ack, has_pending_frames);
    if (retransmittable == HAS_RETRANSMITTABLE_DATA) {
      serialized_packet.retransmittable_frames.push_back(
          QuicFrame(QuicStreamFrame()));
    }
    OnSerializedPacket(&serialized_packet);
  }

  QuicConsumedData SaveAndSendStreamData(QuicStreamId id,
                                         const struct iovec* iov,
                                         int iov_count,
                                         size_t total_length,
                                         QuicStreamOffset offset,
                                         StreamSendingState state) {
    ScopedPacketFlusher flusher(this, NO_ACK);
    producer_.SaveStreamData(id, iov, iov_count, 0u, offset, total_length);
    if (notifier_ != nullptr) {
      return notifier_->WriteOrBufferData(id, total_length, state);
    }
    return QuicConnection::SendStreamData(id, total_length, offset, state);
  }

  QuicConsumedData SendStreamDataWithString(QuicStreamId id,
                                            QuicStringPiece data,
                                            QuicStreamOffset offset,
                                            StreamSendingState state) {
    ScopedPacketFlusher flusher(this, NO_ACK);
    if (id != QuicUtils::GetCryptoStreamId(transport_version()) &&
        this->encryption_level() == ENCRYPTION_NONE) {
      this->SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
    }
    struct iovec iov;
    MakeIOVector(data, &iov);
    return SaveAndSendStreamData(id, &iov, 1, data.length(), offset, state);
  }

  QuicConsumedData SendStreamData3() {
    return SendStreamDataWithString(
        GetNthClientInitiatedStreamId(1, transport_version()), "food", 0,
        NO_FIN);
  }

  QuicConsumedData SendStreamData5() {
    return SendStreamDataWithString(
        GetNthClientInitiatedStreamId(2, transport_version()), "food2", 0,
        NO_FIN);
  }

  // Ensures the connection can write stream data before writing.
  QuicConsumedData EnsureWritableAndSendStreamData5() {
    EXPECT_TRUE(CanWriteStreamData());
    return SendStreamData5();
  }

  // The crypto stream has special semantics so that it is not blocked by a
  // congestion window limitation, and also so that it gets put into a separate
  // packet (so that it is easier to reason about a crypto frame not being
  // split needlessly across packet boundaries).  As a result, we have separate
  // tests for some cases for this stream.
  QuicConsumedData SendCryptoStreamData() {
    return SendStreamDataWithString(
        QuicUtils::GetCryptoStreamId(transport_version()), "chlo", 0, NO_FIN);
  }

  void set_version(ParsedQuicVersion version) {
    QuicConnectionPeer::GetFramer(this)->set_version(version);
  }

  void SetSupportedVersions(const ParsedQuicVersionVector& versions) {
    QuicConnectionPeer::GetFramer(this)->SetSupportedVersions(versions);
    QuicConnectionPeer::SetNoVersionNegotiation(this, versions.size() == 1);
    writer()->SetSupportedVersions(versions);
  }

  void set_perspective(Perspective perspective) {
    writer()->set_perspective(perspective);
    QuicConnectionPeer::SetPerspective(this, perspective);
  }

  // Enable path MTU discovery.  Assumes that the test is performed from the
  // client perspective and the higher value of MTU target is used.
  void EnablePathMtuDiscovery(MockSendAlgorithm* send_algorithm) {
    ASSERT_EQ(Perspective::IS_CLIENT, perspective());

    QuicConfig config;
    QuicTagVector connection_options;
    connection_options.push_back(kMTUH);
    config.SetConnectionOptionsToSend(connection_options);
    EXPECT_CALL(*send_algorithm, SetFromConfig(_, _));
    SetFromConfig(config);

    // Normally, the pacing would be disabled in the test, but calling
    // SetFromConfig enables it.  Set nearly-infinite bandwidth to make the
    // pacing algorithm work.
    EXPECT_CALL(*send_algorithm, PacingRate(_))
        .WillRepeatedly(Return(QuicBandwidth::Infinite()));
  }

  TestAlarmFactory::TestAlarm* GetAckAlarm() {
    return reinterpret_cast<TestAlarmFactory::TestAlarm*>(
        QuicConnectionPeer::GetAckAlarm(this));
  }

  TestAlarmFactory::TestAlarm* GetPingAlarm() {
    return reinterpret_cast<TestAlarmFactory::TestAlarm*>(
        QuicConnectionPeer::GetPingAlarm(this));
  }

  TestAlarmFactory::TestAlarm* GetRetransmissionAlarm() {
    return reinterpret_cast<TestAlarmFactory::TestAlarm*>(
        QuicConnectionPeer::GetRetransmissionAlarm(this));
  }

  TestAlarmFactory::TestAlarm* GetSendAlarm() {
    return reinterpret_cast<TestAlarmFactory::TestAlarm*>(
        QuicConnectionPeer::GetSendAlarm(this));
  }

  TestAlarmFactory::TestAlarm* GetTimeoutAlarm() {
    return reinterpret_cast<TestAlarmFactory::TestAlarm*>(
        QuicConnectionPeer::GetTimeoutAlarm(this));
  }

  TestAlarmFactory::TestAlarm* GetMtuDiscoveryAlarm() {
    return reinterpret_cast<TestAlarmFactory::TestAlarm*>(
        QuicConnectionPeer::GetMtuDiscoveryAlarm(this));
  }

  TestAlarmFactory::TestAlarm* GetRetransmittableOnWireAlarm() {
    return reinterpret_cast<TestAlarmFactory::TestAlarm*>(
        QuicConnectionPeer::GetRetransmittableOnWireAlarm(this));
  }

  TestAlarmFactory::TestAlarm* GetPathDegradingAlarm() {
    return reinterpret_cast<TestAlarmFactory::TestAlarm*>(
        QuicConnectionPeer::GetPathDegradingAlarm(this));
  }

  TestAlarmFactory::TestAlarm* GetProcessUndecryptablePacketsAlarm() {
    return reinterpret_cast<TestAlarmFactory::TestAlarm*>(
        QuicConnectionPeer::GetProcessUndecryptablePacketsAlarm(this));
  }

  void SetMaxTailLossProbes(size_t max_tail_loss_probes) {
    QuicSentPacketManagerPeer::SetMaxTailLossProbes(
        QuicConnectionPeer::GetSentPacketManager(this), max_tail_loss_probes);
  }

  QuicByteCount GetBytesInFlight() {
    return QuicSentPacketManagerPeer::GetBytesInFlight(
        QuicConnectionPeer::GetSentPacketManager(this));
  }

  void set_notifier(SimpleSessionNotifier* notifier) { notifier_ = notifier; }

  void ReturnEffectivePeerAddressForNextPacket(const QuicSocketAddress& addr) {
    next_effective_peer_addr_ = QuicMakeUnique<QuicSocketAddress>(addr);
  }

  using QuicConnection::active_effective_peer_migration_type;
  using QuicConnection::IsCurrentPacketConnectivityProbing;
  using QuicConnection::SelectMutualVersion;
  using QuicConnection::SendProbingRetransmissions;
  using QuicConnection::set_defer_send_in_response_to_packets;

 protected:
  QuicSocketAddress GetEffectivePeerAddressFromCurrentPacket() const override {
    if (next_effective_peer_addr_) {
      return *std::move(next_effective_peer_addr_);
    }
    return QuicConnection::GetEffectivePeerAddressFromCurrentPacket();
  }

 private:
  TestPacketWriter* writer() {
    return static_cast<TestPacketWriter*>(QuicConnection::writer());
  }

  SimpleDataProducer producer_;

  SimpleSessionNotifier* notifier_;

  std::unique_ptr<QuicSocketAddress> next_effective_peer_addr_;
};

enum class AckResponse { kDefer, kImmediate };

// Run tests with combinations of {ParsedQuicVersion, AckResponse}.
struct TestParams {
  TestParams(ParsedQuicVersion version,
             AckResponse ack_response,
             bool no_stop_waiting)
      : version(version),
        ack_response(ack_response),
        no_stop_waiting(no_stop_waiting) {}

  friend std::ostream& operator<<(std::ostream& os, const TestParams& p) {
    os << "{ client_version: " << ParsedQuicVersionToString(p.version)
       << " ack_response: "
       << (p.ack_response == AckResponse::kDefer ? "defer" : "immediate")
       << " no_stop_waiting: " << p.no_stop_waiting << " }";
    return os;
  }

  ParsedQuicVersion version;
  AckResponse ack_response;
  bool no_stop_waiting;
};

// Constructs various test permutations.
std::vector<TestParams> GetTestParams() {
  QuicFlagSaver flags;
  SetQuicFlag(&FLAGS_quic_supports_tls_handshake, true);
  std::vector<TestParams> params;
  ParsedQuicVersionVector all_supported_versions = AllSupportedVersions();
  for (size_t i = 0; i < all_supported_versions.size(); ++i) {
    for (AckResponse ack_response :
         {AckResponse::kDefer, AckResponse::kImmediate}) {
      for (bool no_stop_waiting : {true, false}) {
        // After version 43, never use STOP_WAITING.
        if (all_supported_versions[i].transport_version <= QUIC_VERSION_43 ||
            no_stop_waiting) {
          params.push_back(TestParams(all_supported_versions[i], ack_response,
                                      no_stop_waiting));
        }
      }
    }
  }
  return params;
}

class QuicConnectionTest : public QuicTestWithParam<TestParams> {
 protected:
  QuicConnectionTest()
      : connection_id_(42),
        framer_(SupportedVersions(version()),
                QuicTime::Zero(),
                Perspective::IS_CLIENT),
        send_algorithm_(new StrictMock<MockSendAlgorithm>),
        loss_algorithm_(new MockLossAlgorithm()),
        helper_(new TestConnectionHelper(&clock_, &random_generator_)),
        alarm_factory_(new TestAlarmFactory()),
        peer_framer_(SupportedVersions(version()),
                     QuicTime::Zero(),
                     Perspective::IS_SERVER),
        peer_creator_(connection_id_,
                      &peer_framer_,
                      /*delegate=*/nullptr),
        writer_(new TestPacketWriter(version(), &clock_)),
        connection_(connection_id_,
                    kPeerAddress,
                    helper_.get(),
                    alarm_factory_.get(),
                    writer_.get(),
                    Perspective::IS_CLIENT,
                    version()),
        creator_(QuicConnectionPeer::GetPacketCreator(&connection_)),
        generator_(QuicConnectionPeer::GetPacketGenerator(&connection_)),
        manager_(QuicConnectionPeer::GetSentPacketManager(&connection_)),
        frame1_(QuicUtils::GetCryptoStreamId(version().transport_version),
                false,
                0,
                QuicStringPiece(data1)),
        frame2_(QuicUtils::GetCryptoStreamId(version().transport_version),
                false,
                3,
                QuicStringPiece(data2)),
        packet_number_length_(PACKET_4BYTE_PACKET_NUMBER),
        connection_id_length_(PACKET_8BYTE_CONNECTION_ID),
        notifier_(&connection_) {
    SetQuicFlag(&FLAGS_quic_supports_tls_handshake, true);
    connection_.set_defer_send_in_response_to_packets(GetParam().ack_response ==
                                                      AckResponse::kDefer);
    QuicFramerPeer::SetLastSerializedConnectionId(
        QuicConnectionPeer::GetFramer(&connection_), connection_id_);
    if (version().transport_version > QUIC_VERSION_43) {
      EXPECT_TRUE(QuicConnectionPeer::GetNoStopWaitingFrames(&connection_));
    } else {
      QuicConnectionPeer::SetNoStopWaitingFrames(&connection_,
                                                 GetParam().no_stop_waiting);
    }
    connection_.set_visitor(&visitor_);
    if (connection_.session_decides_what_to_write()) {
      connection_.SetSessionNotifier(&notifier_);
      connection_.set_notifier(&notifier_);
    }
    connection_.SetSendAlgorithm(send_algorithm_);
    connection_.SetLossAlgorithm(loss_algorithm_.get());
    EXPECT_CALL(*send_algorithm_, CanSend(_)).WillRepeatedly(Return(true));
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
        .Times(AnyNumber());
    EXPECT_CALL(*send_algorithm_, GetCongestionWindow())
        .WillRepeatedly(Return(kDefaultTCPMSS));
    EXPECT_CALL(*send_algorithm_, PacingRate(_))
        .WillRepeatedly(Return(QuicBandwidth::Zero()));
    EXPECT_CALL(*send_algorithm_, HasReliableBandwidthEstimate())
        .Times(AnyNumber());
    EXPECT_CALL(*send_algorithm_, BandwidthEstimate())
        .Times(AnyNumber())
        .WillRepeatedly(Return(QuicBandwidth::Zero()));
    EXPECT_CALL(*send_algorithm_, InSlowStart()).Times(AnyNumber());
    EXPECT_CALL(*send_algorithm_, InRecovery()).Times(AnyNumber());
    EXPECT_CALL(*send_algorithm_, OnApplicationLimited(_)).Times(AnyNumber());
    EXPECT_CALL(visitor_, WillingAndAbleToWrite()).Times(AnyNumber());
    EXPECT_CALL(visitor_, HasPendingHandshake()).Times(AnyNumber());
    if (connection_.session_decides_what_to_write()) {
      EXPECT_CALL(visitor_, OnCanWrite())
          .WillRepeatedly(
              Invoke(&notifier_, &SimpleSessionNotifier::OnCanWrite));
    } else {
      EXPECT_CALL(visitor_, OnCanWrite()).Times(AnyNumber());
    }
    EXPECT_CALL(visitor_, PostProcessAfterData()).Times(AnyNumber());
    EXPECT_CALL(visitor_, HasOpenDynamicStreams())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(visitor_, OnCongestionWindowChange(_)).Times(AnyNumber());
    EXPECT_CALL(visitor_, OnConnectivityProbeReceived(_, _)).Times(AnyNumber());
    EXPECT_CALL(visitor_, OnForwardProgressConfirmed()).Times(AnyNumber());

    EXPECT_CALL(*loss_algorithm_, GetLossTimeout())
        .WillRepeatedly(Return(QuicTime::Zero()));
    EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _))
        .Times(AnyNumber());
  }

  QuicConnectionTest(const QuicConnectionTest&) = delete;
  QuicConnectionTest& operator=(const QuicConnectionTest&) = delete;

  ParsedQuicVersion version() { return GetParam().version; }

  QuicAckFrame* outgoing_ack() {
    QuicFrame ack_frame = QuicConnectionPeer::GetUpdatedAckFrame(&connection_);
    ack_ = *ack_frame.ack_frame;
    return &ack_;
  }

  QuicStopWaitingFrame* stop_waiting() {
    QuicConnectionPeer::PopulateStopWaitingFrame(&connection_, &stop_waiting_);
    return &stop_waiting_;
  }

  QuicPacketNumber least_unacked() {
    if (writer_->stop_waiting_frames().empty()) {
      return 0;
    }
    return writer_->stop_waiting_frames()[0].least_unacked;
  }

  void use_tagging_decrypter() { writer_->use_tagging_decrypter(); }

  void ProcessPacket(QuicPacketNumber number) {
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
    ProcessDataPacket(number);
    if (connection_.GetSendAlarm()->IsSet()) {
      connection_.GetSendAlarm()->Fire();
    }
  }

  void ProcessReceivedPacket(const QuicSocketAddress& self_address,
                             const QuicSocketAddress& peer_address,
                             const QuicReceivedPacket& packet) {
    connection_.ProcessUdpPacket(self_address, peer_address, packet);
    if (connection_.GetSendAlarm()->IsSet()) {
      connection_.GetSendAlarm()->Fire();
    }
  }

  void ProcessFramePacket(QuicFrame frame) {
    ProcessFramePacketWithAddresses(frame, kSelfAddress, kPeerAddress);
  }

  void ProcessFramePacketWithAddresses(QuicFrame frame,
                                       QuicSocketAddress self_address,
                                       QuicSocketAddress peer_address) {
    QuicFrames frames;
    frames.push_back(QuicFrame(frame));
    QuicPacketCreatorPeer::SetSendVersionInPacket(
        &peer_creator_, connection_.perspective() == Perspective::IS_SERVER);
    if (QuicPacketCreatorPeer::GetEncryptionLevel(&peer_creator_) >
        ENCRYPTION_NONE) {
      // Set peer_framer_'s corresponding encrypter.
      peer_creator_.SetEncrypter(
          QuicPacketCreatorPeer::GetEncryptionLevel(&peer_creator_),
          QuicMakeUnique<NullEncrypter>(peer_framer_.perspective()));
    }

    char buffer[kMaxPacketSize];
    SerializedPacket serialized_packet =
        QuicPacketCreatorPeer::SerializeAllFrames(&peer_creator_, frames,
                                                  buffer, kMaxPacketSize);
    connection_.ProcessUdpPacket(
        self_address, peer_address,
        QuicReceivedPacket(serialized_packet.encrypted_buffer,
                           serialized_packet.encrypted_length, clock_.Now()));
    if (connection_.GetSendAlarm()->IsSet()) {
      connection_.GetSendAlarm()->Fire();
    }
  }

  // Bypassing the packet creator is unrealistic, but allows us to process
  // packets the QuicPacketCreator won't allow us to create.
  void ForceProcessFramePacket(QuicFrame frame) {
    QuicFrames frames;
    frames.push_back(QuicFrame(frame));
    QuicPacketCreatorPeer::SetSendVersionInPacket(
        &peer_creator_, connection_.perspective() == Perspective::IS_SERVER);
    QuicPacketHeader header;
    QuicPacketCreatorPeer::FillPacketHeader(&peer_creator_, &header);
    char encrypted_buffer[kMaxPacketSize];
    size_t length = peer_framer_.BuildDataPacket(
        header, frames, encrypted_buffer, kMaxPacketSize);
    DCHECK_GT(length, 0u);

    const size_t encrypted_length = peer_framer_.EncryptInPlace(
        ENCRYPTION_NONE, header.packet_number,
        GetStartOfEncryptedData(peer_framer_.version().transport_version,
                                header),
        length, kMaxPacketSize, encrypted_buffer);
    DCHECK_GT(encrypted_length, 0u);

    connection_.ProcessUdpPacket(
        kSelfAddress, kPeerAddress,
        QuicReceivedPacket(encrypted_buffer, encrypted_length, clock_.Now()));
  }

  size_t ProcessFramePacketAtLevel(QuicPacketNumber number,
                                   QuicFrame frame,
                                   EncryptionLevel level) {
    QuicPacketHeader header;
    header.destination_connection_id = connection_id_;
    header.packet_number_length = packet_number_length_;
    header.destination_connection_id_length = connection_id_length_;
    if (peer_framer_.transport_version() > QUIC_VERSION_43 &&
        peer_framer_.perspective() == Perspective::IS_SERVER) {
      header.destination_connection_id_length = PACKET_0BYTE_CONNECTION_ID;
    }
    header.packet_number = number;
    QuicFrames frames;
    frames.push_back(frame);
    std::unique_ptr<QuicPacket> packet(ConstructPacket(header, frames));

    char buffer[kMaxPacketSize];
    size_t encrypted_length =
        framer_.EncryptPayload(level, number, *packet, buffer, kMaxPacketSize);
    connection_.ProcessUdpPacket(
        kSelfAddress, kPeerAddress,
        QuicReceivedPacket(buffer, encrypted_length, QuicTime::Zero(), false));
    return encrypted_length;
  }

  size_t ProcessDataPacket(QuicPacketNumber number) {
    return ProcessDataPacketAtLevel(number, false, ENCRYPTION_NONE);
  }

  size_t ProcessDataPacketAtLevel(QuicPacketNumber number,
                                  bool has_stop_waiting,
                                  EncryptionLevel level) {
    std::unique_ptr<QuicPacket> packet(
        ConstructDataPacket(number, has_stop_waiting));
    char buffer[kMaxPacketSize];
    size_t encrypted_length = peer_framer_.EncryptPayload(
        level, number, *packet, buffer, kMaxPacketSize);
    connection_.ProcessUdpPacket(
        kSelfAddress, kPeerAddress,
        QuicReceivedPacket(buffer, encrypted_length, clock_.Now(), false));
    if (connection_.GetSendAlarm()->IsSet()) {
      connection_.GetSendAlarm()->Fire();
    }
    return encrypted_length;
  }

  void ProcessClosePacket(QuicPacketNumber number) {
    std::unique_ptr<QuicPacket> packet(ConstructClosePacket(number));
    char buffer[kMaxPacketSize];
    size_t encrypted_length = peer_framer_.EncryptPayload(
        ENCRYPTION_NONE, number, *packet, buffer, kMaxPacketSize);
    connection_.ProcessUdpPacket(
        kSelfAddress, kPeerAddress,
        QuicReceivedPacket(buffer, encrypted_length, QuicTime::Zero(), false));
  }

  QuicByteCount SendStreamDataToPeer(QuicStreamId id,
                                     QuicStringPiece data,
                                     QuicStreamOffset offset,
                                     StreamSendingState state,
                                     QuicPacketNumber* last_packet) {
    QuicByteCount packet_size;
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
        .WillOnce(SaveArg<3>(&packet_size));
    connection_.SendStreamDataWithString(id, data, offset, state);
    if (last_packet != nullptr) {
      *last_packet = creator_->packet_number();
    }
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
        .Times(AnyNumber());
    return packet_size;
  }

  void SendAckPacketToPeer() {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
    {
      QuicConnection::ScopedPacketFlusher flusher(&connection_,
                                                  QuicConnection::NO_ACK);
      connection_.SendAck();
    }
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
        .Times(AnyNumber());
  }

  void SendRstStream(QuicStreamId id,
                     QuicRstStreamErrorCode error,
                     QuicStreamOffset bytes_written) {
    if (connection_.session_decides_what_to_write()) {
      notifier_.WriteOrBufferRstStream(id, error, bytes_written);
      connection_.OnStreamReset(id, error);
      return;
    }
    std::unique_ptr<QuicRstStreamFrame> rst_stream =
        QuicMakeUnique<QuicRstStreamFrame>(1, id, error, bytes_written);
    if (connection_.SendControlFrame(QuicFrame(rst_stream.get()))) {
      rst_stream.release();
    }
    connection_.OnStreamReset(id, error);
  }

  void ProcessAckPacket(QuicPacketNumber packet_number, QuicAckFrame* frame) {
    QuicPacketCreatorPeer::SetPacketNumber(&peer_creator_, packet_number - 1);
    ProcessFramePacket(QuicFrame(frame));
  }

  void ProcessAckPacket(QuicAckFrame* frame) {
    ProcessFramePacket(QuicFrame(frame));
  }

  void ProcessStopWaitingPacket(QuicStopWaitingFrame* frame) {
    ProcessFramePacket(QuicFrame(frame));
  }

  size_t ProcessStopWaitingPacketAtLevel(QuicPacketNumber number,
                                         QuicStopWaitingFrame* frame,
                                         EncryptionLevel level) {
    return ProcessFramePacketAtLevel(number, QuicFrame(frame),
                                     ENCRYPTION_INITIAL);
  }

  void ProcessGoAwayPacket(QuicGoAwayFrame* frame) {
    ProcessFramePacket(QuicFrame(frame));
  }

  bool IsMissing(QuicPacketNumber number) {
    return IsAwaitingPacket(*outgoing_ack(), number, 0);
  }

  std::unique_ptr<QuicPacket> ConstructPacket(const QuicPacketHeader& header,
                                              const QuicFrames& frames) {
    auto packet = BuildUnsizedDataPacket(&peer_framer_, header, frames);
    EXPECT_NE(nullptr, packet.get());
    return packet;
  }

  std::unique_ptr<QuicPacket> ConstructDataPacket(QuicPacketNumber number,
                                                  bool has_stop_waiting) {
    QuicPacketHeader header;
    // Set connection_id to peer's in memory representation as this data packet
    // is created by peer_framer.
    header.destination_connection_id = connection_id_;
    header.packet_number_length = packet_number_length_;
    header.destination_connection_id_length = connection_id_length_;
    if (peer_framer_.transport_version() > QUIC_VERSION_43 &&
        peer_framer_.perspective() == Perspective::IS_SERVER) {
      header.destination_connection_id_length = PACKET_0BYTE_CONNECTION_ID;
    }
    header.packet_number = number;

    QuicFrames frames;
    frames.push_back(QuicFrame(frame1_));
    if (has_stop_waiting) {
      frames.push_back(QuicFrame(&stop_waiting_));
    }
    return ConstructPacket(header, frames);
  }

  OwningSerializedPacketPointer ConstructProbingPacket() {
    if (version().transport_version == QUIC_VERSION_99) {
      QuicPathFrameBuffer payload = {
          {0xde, 0xad, 0xbe, 0xef, 0xba, 0xdc, 0x0f, 0xfe}};
      return QuicPacketCreatorPeer::
          SerializePathChallengeConnectivityProbingPacket(&peer_creator_,
                                                          &payload);
    }
    return QuicPacketCreatorPeer::SerializeConnectivityProbingPacket(
        &peer_creator_);
  }

  std::unique_ptr<QuicPacket> ConstructClosePacket(QuicPacketNumber number) {
    QuicPacketHeader header;
    // Set connection_id to peer's in memory representation as this connection
    // close packet is created by peer_framer.
    header.destination_connection_id = connection_id_;
    header.packet_number = number;
    if (peer_framer_.transport_version() > QUIC_VERSION_43 &&
        peer_framer_.perspective() == Perspective::IS_SERVER) {
      header.destination_connection_id_length = PACKET_0BYTE_CONNECTION_ID;
    }

    QuicConnectionCloseFrame qccf;
    qccf.error_code = QUIC_PEER_GOING_AWAY;

    QuicFrames frames;
    frames.push_back(QuicFrame(&qccf));
    return ConstructPacket(header, frames);
  }

  QuicTime::Delta DefaultRetransmissionTime() {
    return QuicTime::Delta::FromMilliseconds(kDefaultRetransmissionTimeMs);
  }

  QuicTime::Delta DefaultDelayedAckTime() {
    return QuicTime::Delta::FromMilliseconds(kDefaultDelayedAckTimeMs);
  }

  const QuicStopWaitingFrame InitStopWaitingFrame(
      QuicPacketNumber least_unacked) {
    QuicStopWaitingFrame frame;
    frame.least_unacked = least_unacked;
    return frame;
  }

  // Construct a ack_frame that acks all packet numbers between 1 and
  // |largest_acked|, except |missing|.
  // REQUIRES: 1 <= |missing| < |largest_acked|
  QuicAckFrame ConstructAckFrame(QuicPacketNumber largest_acked,
                                 QuicPacketNumber missing) {
    if (missing == 1) {
      return InitAckFrame({{missing + 1, largest_acked + 1}});
    }
    return InitAckFrame({{1, missing}, {missing + 1, largest_acked + 1}});
  }

  // Undo nacking a packet within the frame.
  void AckPacket(QuicPacketNumber arrived, QuicAckFrame* frame) {
    EXPECT_FALSE(frame->packets.Contains(arrived));
    frame->packets.Add(arrived);
  }

  void TriggerConnectionClose() {
    // Send an erroneous packet to close the connection.
    EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_INVALID_PACKET_HEADER, _,
                                             ConnectionCloseSource::FROM_SELF));
    // Call ProcessDataPacket rather than ProcessPacket, as we should not get a
    // packet call to the visitor.
    if (GetQuicRestartFlag(quic_enable_accept_random_ipn)) {
      ProcessDataPacket(kMaxRandomInitialPacketNumber + 6000);
    } else {
      ProcessDataPacket(6000);
    }

    EXPECT_FALSE(QuicConnectionPeer::GetConnectionClosePacket(&connection_) ==
                 nullptr);
  }

  void BlockOnNextWrite() {
    writer_->BlockOnNextWrite();
    EXPECT_CALL(visitor_, OnWriteBlocked()).Times(AtLeast(1));
  }

  void SimulateNextPacketTooLarge() { writer_->SimulateNextPacketTooLarge(); }

  void AlwaysGetPacketTooLarge() { writer_->AlwaysGetPacketTooLarge(); }

  void SetWritePauseTimeDelta(QuicTime::Delta delta) {
    writer_->SetWritePauseTimeDelta(delta);
  }

  void CongestionBlockWrites() {
    EXPECT_CALL(*send_algorithm_, CanSend(_))
        .WillRepeatedly(testing::Return(false));
  }

  void CongestionUnblockWrites() {
    EXPECT_CALL(*send_algorithm_, CanSend(_))
        .WillRepeatedly(testing::Return(true));
  }

  void set_perspective(Perspective perspective) {
    connection_.set_perspective(perspective);
    if (perspective == Perspective::IS_SERVER) {
      connection_.set_can_truncate_connection_ids(true);
    }
    QuicFramerPeer::SetPerspective(&peer_framer_,
                                   InvertPerspective(perspective));
  }

  void set_packets_between_probes_base(
      const QuicPacketCount packets_between_probes_base) {
    QuicConnectionPeer::SetPacketsBetweenMtuProbes(&connection_,
                                                   packets_between_probes_base);
    QuicConnectionPeer::SetNextMtuProbeAt(&connection_,
                                          packets_between_probes_base);
  }

  bool IsDefaultTestConfiguration() {
    TestParams p = GetParam();
    return p.ack_response == AckResponse::kImmediate &&
           p.version == AllSupportedVersions()[0] && p.no_stop_waiting;
  }

  QuicConnectionId connection_id_;
  QuicFramer framer_;

  MockSendAlgorithm* send_algorithm_;
  std::unique_ptr<MockLossAlgorithm> loss_algorithm_;
  MockClock clock_;
  MockRandom random_generator_;
  SimpleBufferAllocator buffer_allocator_;
  std::unique_ptr<TestConnectionHelper> helper_;
  std::unique_ptr<TestAlarmFactory> alarm_factory_;
  QuicFramer peer_framer_;
  QuicPacketCreator peer_creator_;
  std::unique_ptr<TestPacketWriter> writer_;
  TestConnection connection_;
  QuicPacketCreator* creator_;
  QuicPacketGenerator* generator_;
  QuicSentPacketManager* manager_;
  StrictMock<MockQuicConnectionVisitor> visitor_;

  QuicStreamFrame frame1_;
  QuicStreamFrame frame2_;
  QuicAckFrame ack_;
  QuicStopWaitingFrame stop_waiting_;
  QuicPacketNumberLength packet_number_length_;
  QuicConnectionIdLength connection_id_length_;

  SimpleSessionNotifier notifier_;
};

// Run all end to end tests with all supported versions.
INSTANTIATE_TEST_CASE_P(SupportedVersion,
                        QuicConnectionTest,
                        ::testing::ValuesIn(GetTestParams()));

TEST_P(QuicConnectionTest, SelfAddressChangeAtClient) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  EXPECT_EQ(Perspective::IS_CLIENT, connection_.perspective());
  EXPECT_TRUE(connection_.connected());

  QuicStreamFrame stream_frame(
      QuicUtils::GetCryptoStreamId(connection_.transport_version()), false, 0u,
      QuicStringPiece());
  EXPECT_CALL(visitor_, OnStreamFrame(_));
  ProcessFramePacketWithAddresses(QuicFrame(stream_frame), kSelfAddress,
                                  kPeerAddress);
  // Cause change in self_address.
  QuicIpAddress host;
  host.FromString("1.1.1.1");
  QuicSocketAddress self_address(host, 123);
  EXPECT_CALL(visitor_, OnStreamFrame(_));
  ProcessFramePacketWithAddresses(QuicFrame(stream_frame), self_address,
                                  kPeerAddress);
  EXPECT_TRUE(connection_.connected());
}

TEST_P(QuicConnectionTest, SelfAddressChangeAtServer) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  set_perspective(Perspective::IS_SERVER);
  QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);

  EXPECT_EQ(Perspective::IS_SERVER, connection_.perspective());
  EXPECT_TRUE(connection_.connected());

  QuicStreamFrame stream_frame(
      QuicUtils::GetCryptoStreamId(connection_.transport_version()), false, 0u,
      QuicStringPiece());
  EXPECT_CALL(visitor_, OnStreamFrame(_));
  ProcessFramePacketWithAddresses(QuicFrame(stream_frame), kSelfAddress,
                                  kPeerAddress);
  // Cause change in self_address.
  QuicIpAddress host;
  host.FromString("1.1.1.1");
  QuicSocketAddress self_address(host, 123);
  EXPECT_CALL(visitor_, AllowSelfAddressChange()).WillOnce(Return(false));
  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_ERROR_MIGRATING_ADDRESS, _, _));
  ProcessFramePacketWithAddresses(QuicFrame(stream_frame), self_address,
                                  kPeerAddress);
  EXPECT_FALSE(connection_.connected());
}

TEST_P(QuicConnectionTest, AllowSelfAddressChangeToMappedIpv4AddressAtServer) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  set_perspective(Perspective::IS_SERVER);
  QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);

  EXPECT_EQ(Perspective::IS_SERVER, connection_.perspective());
  EXPECT_TRUE(connection_.connected());

  QuicStreamFrame stream_frame(
      QuicUtils::GetCryptoStreamId(connection_.transport_version()), false, 0u,
      QuicStringPiece());
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(3);
  QuicIpAddress host;
  host.FromString("1.1.1.1");
  QuicSocketAddress self_address1(host, 443);
  ProcessFramePacketWithAddresses(QuicFrame(stream_frame), self_address1,
                                  kPeerAddress);
  // Cause self_address change to mapped Ipv4 address.
  QuicIpAddress host2;
  host2.FromString(
      QuicStrCat("::ffff:", connection_.self_address().host().ToString()));
  QuicSocketAddress self_address2(host2, connection_.self_address().port());
  ProcessFramePacketWithAddresses(QuicFrame(stream_frame), self_address2,
                                  kPeerAddress);
  EXPECT_TRUE(connection_.connected());
  // self_address change back to Ipv4 address.
  ProcessFramePacketWithAddresses(QuicFrame(stream_frame), self_address1,
                                  kPeerAddress);
  EXPECT_TRUE(connection_.connected());
}

TEST_P(QuicConnectionTest, ClientAddressChangeAndPacketReordered) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  set_perspective(Perspective::IS_SERVER);
  QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);

  // Clear direct_peer_address.
  QuicConnectionPeer::SetDirectPeerAddress(&connection_, QuicSocketAddress());
  // Clear effective_peer_address, it is the same as direct_peer_address for
  // this test.
  QuicConnectionPeer::SetEffectivePeerAddress(&connection_,
                                              QuicSocketAddress());

  QuicPacketCreatorPeer::SetPacketNumber(&peer_creator_, 5);
  QuicStreamFrame stream_frame(
      QuicUtils::GetCryptoStreamId(connection_.transport_version()), false, 0u,
      QuicStringPiece());
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(AnyNumber());
  const QuicSocketAddress kNewPeerAddress =
      QuicSocketAddress(QuicIpAddress::Loopback6(),
                        /*port=*/23456);
  ProcessFramePacketWithAddresses(QuicFrame(stream_frame), kSelfAddress,
                                  kNewPeerAddress);
  EXPECT_EQ(kNewPeerAddress, connection_.peer_address());
  EXPECT_EQ(kNewPeerAddress, connection_.effective_peer_address());

  // Decrease packet number to simulate out-of-order packets.
  QuicPacketCreatorPeer::SetPacketNumber(&peer_creator_, 4);
  // This is an old packet, do not migrate.
  EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(0);
  ProcessFramePacketWithAddresses(QuicFrame(stream_frame), kSelfAddress,
                                  kPeerAddress);
  EXPECT_EQ(kNewPeerAddress, connection_.peer_address());
  EXPECT_EQ(kNewPeerAddress, connection_.effective_peer_address());
}

TEST_P(QuicConnectionTest, PeerAddressChangeAtServer) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  set_perspective(Perspective::IS_SERVER);
  QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);
  EXPECT_EQ(Perspective::IS_SERVER, connection_.perspective());

  // Clear direct_peer_address.
  QuicConnectionPeer::SetDirectPeerAddress(&connection_, QuicSocketAddress());
  // Clear effective_peer_address, it is the same as direct_peer_address for
  // this test.
  QuicConnectionPeer::SetEffectivePeerAddress(&connection_,
                                              QuicSocketAddress());
  EXPECT_FALSE(connection_.effective_peer_address().IsInitialized());

  QuicStreamFrame stream_frame(
      QuicUtils::GetCryptoStreamId(connection_.transport_version()), false, 0u,
      QuicStringPiece());
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(AnyNumber());
  ProcessFramePacketWithAddresses(QuicFrame(stream_frame), kSelfAddress,
                                  kPeerAddress);
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());

  // Process another packet with a different peer address on server side will
  // start connection migration.
  const QuicSocketAddress kNewPeerAddress =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/23456);
  EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(1);
  ProcessFramePacketWithAddresses(QuicFrame(stream_frame), kSelfAddress,
                                  kNewPeerAddress);
  EXPECT_EQ(kNewPeerAddress, connection_.peer_address());
  EXPECT_EQ(kNewPeerAddress, connection_.effective_peer_address());
}

TEST_P(QuicConnectionTest, EffectivePeerAddressChangeAtServer) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  set_perspective(Perspective::IS_SERVER);
  QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);
  EXPECT_EQ(Perspective::IS_SERVER, connection_.perspective());

  // Clear direct_peer_address.
  QuicConnectionPeer::SetDirectPeerAddress(&connection_, QuicSocketAddress());
  // Clear effective_peer_address, it is different from direct_peer_address for
  // this test.
  QuicConnectionPeer::SetEffectivePeerAddress(&connection_,
                                              QuicSocketAddress());
  const QuicSocketAddress kEffectivePeerAddress =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/43210);
  connection_.ReturnEffectivePeerAddressForNextPacket(kEffectivePeerAddress);

  QuicStreamFrame stream_frame(
      QuicUtils::GetCryptoStreamId(connection_.transport_version()), false, 0u,
      QuicStringPiece());
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(AnyNumber());
  ProcessFramePacketWithAddresses(QuicFrame(stream_frame), kSelfAddress,
                                  kPeerAddress);
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kEffectivePeerAddress, connection_.effective_peer_address());

  // Process another packet with the same direct peer address and different
  // effective peer address on server side will start connection migration.
  const QuicSocketAddress kNewEffectivePeerAddress =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/54321);
  connection_.ReturnEffectivePeerAddressForNextPacket(kNewEffectivePeerAddress);
  EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(1);
  ProcessFramePacketWithAddresses(QuicFrame(stream_frame), kSelfAddress,
                                  kPeerAddress);
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kNewEffectivePeerAddress, connection_.effective_peer_address());

  // Process another packet with a different direct peer address and the same
  // effective peer address on server side will not start connection migration.
  const QuicSocketAddress kNewPeerAddress =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/23456);
  connection_.ReturnEffectivePeerAddressForNextPacket(kNewEffectivePeerAddress);
  EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(0);
  // ack_frame is used to complete the migration started by the last packet, we
  // need to make sure a new migration does not start after the previous one is
  // completed.
  QuicAckFrame ack_frame = InitAckFrame(1);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(_, _, _, _, _));
  ProcessFramePacketWithAddresses(QuicFrame(&ack_frame), kSelfAddress,
                                  kNewPeerAddress);
  EXPECT_EQ(kNewPeerAddress, connection_.peer_address());
  EXPECT_EQ(kNewEffectivePeerAddress, connection_.effective_peer_address());

  // Process another packet with different direct peer address and different
  // effective peer address on server side will start connection migration.
  const QuicSocketAddress kNewerEffectivePeerAddress =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/65432);
  const QuicSocketAddress kFinalPeerAddress =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/34567);
  connection_.ReturnEffectivePeerAddressForNextPacket(
      kNewerEffectivePeerAddress);
  EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(1);
  ProcessFramePacketWithAddresses(QuicFrame(stream_frame), kSelfAddress,
                                  kFinalPeerAddress);
  EXPECT_EQ(kFinalPeerAddress, connection_.peer_address());
  EXPECT_EQ(kNewerEffectivePeerAddress, connection_.effective_peer_address());
  EXPECT_EQ(PORT_CHANGE, connection_.active_effective_peer_migration_type());

  // While the previous migration is ongoing, process another packet with the
  // same direct peer address and different effective peer address on server
  // side will start a new connection migration.
  const QuicSocketAddress kNewestEffectivePeerAddress =
      QuicSocketAddress(QuicIpAddress::Loopback4(), /*port=*/65430);
  connection_.ReturnEffectivePeerAddressForNextPacket(
      kNewestEffectivePeerAddress);
  EXPECT_CALL(visitor_, OnConnectionMigration(IPV6_TO_IPV4_CHANGE)).Times(1);
  EXPECT_CALL(*send_algorithm_, OnConnectionMigration()).Times(1);
  ProcessFramePacketWithAddresses(QuicFrame(stream_frame), kSelfAddress,
                                  kFinalPeerAddress);
  EXPECT_EQ(kFinalPeerAddress, connection_.peer_address());
  EXPECT_EQ(kNewestEffectivePeerAddress, connection_.effective_peer_address());
  EXPECT_EQ(IPV6_TO_IPV4_CHANGE,
            connection_.active_effective_peer_migration_type());
}

TEST_P(QuicConnectionTest, ReceivePaddedPingAtServer) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  set_perspective(Perspective::IS_SERVER);
  QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);
  EXPECT_EQ(Perspective::IS_SERVER, connection_.perspective());

  // Clear direct_peer_address.
  QuicConnectionPeer::SetDirectPeerAddress(&connection_, QuicSocketAddress());
  // Clear effective_peer_address, it is the same as direct_peer_address for
  // this test.
  QuicConnectionPeer::SetEffectivePeerAddress(&connection_,
                                              QuicSocketAddress());
  EXPECT_FALSE(connection_.effective_peer_address().IsInitialized());

  QuicStreamFrame stream_frame(
      QuicUtils::GetCryptoStreamId(connection_.transport_version()), false, 0u,
      QuicStringPiece());
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(AnyNumber());
  ProcessFramePacketWithAddresses(QuicFrame(stream_frame), kSelfAddress,
                                  kPeerAddress);
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());

  EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(0);
  EXPECT_CALL(visitor_, OnConnectivityProbeReceived(_, _)).Times(0);

  // Process a padded PING or PATH CHALLENGE packet with no peer address change
  // on server side will be ignored.
  OwningSerializedPacketPointer probing_packet;
  if (version().transport_version == QUIC_VERSION_99) {
    QuicPathFrameBuffer payload = {
        {0xde, 0xad, 0xbe, 0xef, 0xba, 0xdc, 0x0f, 0xfe}};
    probing_packet =
        QuicPacketCreatorPeer::SerializePathChallengeConnectivityProbingPacket(
            &peer_creator_, &payload);
  } else {
    probing_packet = QuicPacketCreatorPeer::SerializeConnectivityProbingPacket(
        &peer_creator_);
  }
  std::unique_ptr<QuicReceivedPacket> received(ConstructReceivedPacket(
      QuicEncryptedPacket(probing_packet->encrypted_buffer,
                          probing_packet->encrypted_length),
      clock_.Now()));

  ProcessReceivedPacket(kSelfAddress, kPeerAddress, *received);

  EXPECT_FALSE(connection_.IsCurrentPacketConnectivityProbing());
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());
}

TEST_P(QuicConnectionTest, WriteOutOfOrderQueuedPackets) {
  // EXPECT_QUIC_BUG tests are expensive so only run one instance of them.
  if (!IsDefaultTestConfiguration()) {
    return;
  }

  set_perspective(Perspective::IS_CLIENT);

  BlockOnNextWrite();

  QuicStreamId stream_id = 2;
  connection_.SendStreamDataWithString(stream_id, "foo", 0, NO_FIN);

  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  writer_->SetWritable();
  connection_.SendConnectivityProbingPacket(writer_.get(),
                                            connection_.peer_address());

  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_INTERNAL_ERROR,
                                           "Packet written out of order.",
                                           ConnectionCloseSource::FROM_SELF));
  EXPECT_QUIC_BUG(connection_.OnCanWrite(),
                  "Attempt to write packet:1 after:2");
  EXPECT_FALSE(connection_.connected());
}

TEST_P(QuicConnectionTest, DiscardQueuedPacketsAfterConnectionClose) {
  // Regression test for b/74073386.
  {
    InSequence seq;
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
    EXPECT_CALL(visitor_, OnConnectionClosed(_, _, _)).Times(1);
  }

  set_perspective(Perspective::IS_CLIENT);

  writer_->SimulateNextPacketTooLarge();

  // This packet write should fail, which should cause the connection to close
  // after sending a connection close packet, then the failed packet should be
  // queued.
  connection_.SendStreamDataWithString(/*id=*/2, "foo", 0, NO_FIN);

  EXPECT_FALSE(connection_.connected());
  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  EXPECT_EQ(0u, connection_.GetStats().packets_discarded);
  connection_.OnCanWrite();
  EXPECT_EQ(1u, connection_.GetStats().packets_discarded);
}

TEST_P(QuicConnectionTest, ReceiveConnectivityProbingAtServer) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  set_perspective(Perspective::IS_SERVER);
  QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);
  EXPECT_EQ(Perspective::IS_SERVER, connection_.perspective());

  // Clear direct_peer_address.
  QuicConnectionPeer::SetDirectPeerAddress(&connection_, QuicSocketAddress());
  // Clear effective_peer_address, it is the same as direct_peer_address for
  // this test.
  QuicConnectionPeer::SetEffectivePeerAddress(&connection_,
                                              QuicSocketAddress());
  EXPECT_FALSE(connection_.effective_peer_address().IsInitialized());

  QuicStreamFrame stream_frame(
      QuicUtils::GetCryptoStreamId(connection_.transport_version()), false, 0u,
      QuicStringPiece());
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(AnyNumber());
  ProcessFramePacketWithAddresses(QuicFrame(stream_frame), kSelfAddress,
                                  kPeerAddress);
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());

  EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(0);
  EXPECT_CALL(visitor_, OnConnectivityProbeReceived(_, _)).Times(1);

  // Process a padded PING packet from a new peer address on server side
  // is effectively receiving a connectivity probing.
  const QuicSocketAddress kNewPeerAddress =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/23456);

  OwningSerializedPacketPointer probing_packet = ConstructProbingPacket();
  std::unique_ptr<QuicReceivedPacket> received(ConstructReceivedPacket(
      QuicEncryptedPacket(probing_packet->encrypted_buffer,
                          probing_packet->encrypted_length),
      clock_.Now()));

  ProcessReceivedPacket(kSelfAddress, kNewPeerAddress, *received);

  EXPECT_TRUE(connection_.IsCurrentPacketConnectivityProbing());
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());

  // Process another packet with the old peer address on server side will not
  // start peer migration.
  EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(0);
  ProcessFramePacketWithAddresses(QuicFrame(stream_frame), kSelfAddress,
                                  kPeerAddress);
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());
}

TEST_P(QuicConnectionTest, ReceiveReorderedConnectivityProbingAtServer) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  set_perspective(Perspective::IS_SERVER);
  QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);
  EXPECT_EQ(Perspective::IS_SERVER, connection_.perspective());

  // Clear direct_peer_address.
  QuicConnectionPeer::SetDirectPeerAddress(&connection_, QuicSocketAddress());
  // Clear effective_peer_address, it is the same as direct_peer_address for
  // this test.
  QuicConnectionPeer::SetEffectivePeerAddress(&connection_,
                                              QuicSocketAddress());
  EXPECT_FALSE(connection_.effective_peer_address().IsInitialized());

  QuicPacketCreatorPeer::SetPacketNumber(&peer_creator_, 5);
  QuicStreamFrame stream_frame(
      QuicUtils::GetCryptoStreamId(connection_.transport_version()), false, 0u,
      QuicStringPiece());
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(AnyNumber());
  ProcessFramePacketWithAddresses(QuicFrame(stream_frame), kSelfAddress,
                                  kPeerAddress);
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());

  // Decrease packet number to simulate out-of-order packets.
  QuicPacketCreatorPeer::SetPacketNumber(&peer_creator_, 4);

  EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(0);
  EXPECT_CALL(visitor_, OnConnectivityProbeReceived(_, _)).Times(1);

  // Process a padded PING packet from a new peer address on server side
  // is effectively receiving a connectivity probing, even if a newer packet has
  // been received before this one.
  const QuicSocketAddress kNewPeerAddress =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/23456);

  OwningSerializedPacketPointer probing_packet = ConstructProbingPacket();
  std::unique_ptr<QuicReceivedPacket> received(ConstructReceivedPacket(
      QuicEncryptedPacket(probing_packet->encrypted_buffer,
                          probing_packet->encrypted_length),
      clock_.Now()));

  ProcessReceivedPacket(kSelfAddress, kNewPeerAddress, *received);

  EXPECT_TRUE(connection_.IsCurrentPacketConnectivityProbing());
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());
}

TEST_P(QuicConnectionTest, MigrateAfterProbingAtServer) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  set_perspective(Perspective::IS_SERVER);
  QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);
  EXPECT_EQ(Perspective::IS_SERVER, connection_.perspective());

  // Clear direct_peer_address.
  QuicConnectionPeer::SetDirectPeerAddress(&connection_, QuicSocketAddress());
  // Clear effective_peer_address, it is the same as direct_peer_address for
  // this test.
  QuicConnectionPeer::SetEffectivePeerAddress(&connection_,
                                              QuicSocketAddress());
  EXPECT_FALSE(connection_.effective_peer_address().IsInitialized());

  QuicStreamFrame stream_frame(
      QuicUtils::GetCryptoStreamId(connection_.transport_version()), false, 0u,
      QuicStringPiece());
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(AnyNumber());
  ProcessFramePacketWithAddresses(QuicFrame(stream_frame), kSelfAddress,
                                  kPeerAddress);
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());

  EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(0);
  EXPECT_CALL(visitor_, OnConnectivityProbeReceived(_, _)).Times(1);

  // Process a padded PING packet from a new peer address on server side
  // is effectively receiving a connectivity probing.
  const QuicSocketAddress kNewPeerAddress =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/23456);

  OwningSerializedPacketPointer probing_packet = ConstructProbingPacket();
  std::unique_ptr<QuicReceivedPacket> received(ConstructReceivedPacket(
      QuicEncryptedPacket(probing_packet->encrypted_buffer,
                          probing_packet->encrypted_length),
      clock_.Now()));
  ProcessReceivedPacket(kSelfAddress, kNewPeerAddress, *received);
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());

  // Process another non-probing packet with the new peer address on server
  // side will start peer migration.
  EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(1);

  ProcessFramePacketWithAddresses(QuicFrame(stream_frame), kSelfAddress,
                                  kNewPeerAddress);
  EXPECT_EQ(kNewPeerAddress, connection_.peer_address());
  EXPECT_EQ(kNewPeerAddress, connection_.effective_peer_address());
}

TEST_P(QuicConnectionTest, ReceivePaddedPingAtClient) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  set_perspective(Perspective::IS_CLIENT);
  EXPECT_EQ(Perspective::IS_CLIENT, connection_.perspective());

  // Clear direct_peer_address.
  QuicConnectionPeer::SetDirectPeerAddress(&connection_, QuicSocketAddress());
  // Clear effective_peer_address, it is the same as direct_peer_address for
  // this test.
  QuicConnectionPeer::SetEffectivePeerAddress(&connection_,
                                              QuicSocketAddress());
  EXPECT_FALSE(connection_.effective_peer_address().IsInitialized());

  QuicStreamFrame stream_frame(
      QuicUtils::GetCryptoStreamId(connection_.transport_version()), false, 0u,
      QuicStringPiece());
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(AnyNumber());
  ProcessFramePacketWithAddresses(QuicFrame(stream_frame), kSelfAddress,
                                  kPeerAddress);
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());

  // Client takes all padded PING packet as speculative connectivity
  // probing packet, and reports to visitor.
  EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(0);
  EXPECT_CALL(visitor_, OnConnectivityProbeReceived(_, _)).Times(1);

  OwningSerializedPacketPointer probing_packet = ConstructProbingPacket();
  std::unique_ptr<QuicReceivedPacket> received(ConstructReceivedPacket(
      QuicEncryptedPacket(probing_packet->encrypted_buffer,
                          probing_packet->encrypted_length),
      clock_.Now()));
  ProcessReceivedPacket(kSelfAddress, kPeerAddress, *received);

  EXPECT_FALSE(connection_.IsCurrentPacketConnectivityProbing());
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());
}

TEST_P(QuicConnectionTest, ReceiveConnectivityProbingAtClient) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  set_perspective(Perspective::IS_CLIENT);
  EXPECT_EQ(Perspective::IS_CLIENT, connection_.perspective());

  // Clear direct_peer_address.
  QuicConnectionPeer::SetDirectPeerAddress(&connection_, QuicSocketAddress());
  // Clear effective_peer_address, it is the same as direct_peer_address for
  // this test.
  QuicConnectionPeer::SetEffectivePeerAddress(&connection_,
                                              QuicSocketAddress());
  EXPECT_FALSE(connection_.effective_peer_address().IsInitialized());

  QuicStreamFrame stream_frame(
      QuicUtils::GetCryptoStreamId(connection_.transport_version()), false, 0u,
      QuicStringPiece());
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(AnyNumber());
  ProcessFramePacketWithAddresses(QuicFrame(stream_frame), kSelfAddress,
                                  kPeerAddress);
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());

  // Process a padded PING packet with a different self address on client side
  // is effectively receiving a connectivity probing.
  EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(0);
  EXPECT_CALL(visitor_, OnConnectivityProbeReceived(_, _)).Times(1);

  const QuicSocketAddress kNewSelfAddress =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/23456);

  OwningSerializedPacketPointer probing_packet = ConstructProbingPacket();
  std::unique_ptr<QuicReceivedPacket> received(ConstructReceivedPacket(
      QuicEncryptedPacket(probing_packet->encrypted_buffer,
                          probing_packet->encrypted_length),
      clock_.Now()));
  ProcessReceivedPacket(kNewSelfAddress, kPeerAddress, *received);

  EXPECT_TRUE(connection_.IsCurrentPacketConnectivityProbing());
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());
}

TEST_P(QuicConnectionTest, PeerAddressChangeAtClient) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  set_perspective(Perspective::IS_CLIENT);
  EXPECT_EQ(Perspective::IS_CLIENT, connection_.perspective());

  // Clear direct_peer_address.
  QuicConnectionPeer::SetDirectPeerAddress(&connection_, QuicSocketAddress());
  // Clear effective_peer_address, it is the same as direct_peer_address for
  // this test.
  QuicConnectionPeer::SetEffectivePeerAddress(&connection_,
                                              QuicSocketAddress());
  EXPECT_FALSE(connection_.effective_peer_address().IsInitialized());

  QuicStreamFrame stream_frame(
      QuicUtils::GetCryptoStreamId(connection_.transport_version()), false, 0u,
      QuicStringPiece());
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(AnyNumber());
  ProcessFramePacketWithAddresses(QuicFrame(stream_frame), kSelfAddress,
                                  kPeerAddress);
  EXPECT_EQ(kPeerAddress, connection_.peer_address());
  EXPECT_EQ(kPeerAddress, connection_.effective_peer_address());

  // Process another packet with a different peer address on client side will
  // only update peer address.
  const QuicSocketAddress kNewPeerAddress =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/23456);
  EXPECT_CALL(visitor_, OnConnectionMigration(PORT_CHANGE)).Times(0);
  ProcessFramePacketWithAddresses(QuicFrame(stream_frame), kSelfAddress,
                                  kNewPeerAddress);
  EXPECT_EQ(kNewPeerAddress, connection_.peer_address());
  EXPECT_EQ(kNewPeerAddress, connection_.effective_peer_address());
}

TEST_P(QuicConnectionTest, MaxPacketSize) {
  EXPECT_EQ(Perspective::IS_CLIENT, connection_.perspective());
  EXPECT_EQ(1350u, connection_.max_packet_length());
}

TEST_P(QuicConnectionTest, SmallerServerMaxPacketSize) {
  QuicConnectionId connection_id = 42;
  TestConnection connection(connection_id, kPeerAddress, helper_.get(),
                            alarm_factory_.get(), writer_.get(),
                            Perspective::IS_SERVER, version());
  EXPECT_EQ(Perspective::IS_SERVER, connection.perspective());
  EXPECT_EQ(1000u, connection.max_packet_length());
}

TEST_P(QuicConnectionTest, IncreaseServerMaxPacketSize) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  set_perspective(Perspective::IS_SERVER);
  connection_.SetMaxPacketLength(1000);

  QuicPacketHeader header;
  header.destination_connection_id = connection_id_;
  header.version_flag = true;
  header.packet_number = 1;

  QuicFrames frames;
  QuicPaddingFrame padding;
  frames.push_back(QuicFrame(frame1_));
  frames.push_back(QuicFrame(padding));
  std::unique_ptr<QuicPacket> packet(ConstructPacket(header, frames));
  char buffer[kMaxPacketSize];
  size_t encrypted_length = peer_framer_.EncryptPayload(
      ENCRYPTION_NONE, 12, *packet, buffer, kMaxPacketSize);
  EXPECT_EQ(kMaxPacketSize, encrypted_length);

  framer_.set_version(version());
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  connection_.ProcessUdpPacket(
      kSelfAddress, kPeerAddress,
      QuicReceivedPacket(buffer, encrypted_length, QuicTime::Zero(), false));

  EXPECT_EQ(kMaxPacketSize, connection_.max_packet_length());
}

TEST_P(QuicConnectionTest, IncreaseServerMaxPacketSizeWhileWriterLimited) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  const QuicByteCount lower_max_packet_size = 1240;
  writer_->set_max_packet_size(lower_max_packet_size);
  set_perspective(Perspective::IS_SERVER);
  connection_.SetMaxPacketLength(1000);
  EXPECT_EQ(1000u, connection_.max_packet_length());

  QuicPacketHeader header;
  header.destination_connection_id = connection_id_;
  header.version_flag = true;
  header.packet_number = 1;

  QuicFrames frames;
  QuicPaddingFrame padding;
  frames.push_back(QuicFrame(frame1_));
  frames.push_back(QuicFrame(padding));
  std::unique_ptr<QuicPacket> packet(ConstructPacket(header, frames));
  char buffer[kMaxPacketSize];
  size_t encrypted_length = peer_framer_.EncryptPayload(
      ENCRYPTION_NONE, 12, *packet, buffer, kMaxPacketSize);
  EXPECT_EQ(kMaxPacketSize, encrypted_length);

  framer_.set_version(version());
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  connection_.ProcessUdpPacket(
      kSelfAddress, kPeerAddress,
      QuicReceivedPacket(buffer, encrypted_length, QuicTime::Zero(), false));

  // Here, the limit imposed by the writer is lower than the size of the packet
  // received, so the writer max packet size is used.
  EXPECT_EQ(lower_max_packet_size, connection_.max_packet_length());
}

TEST_P(QuicConnectionTest, LimitMaxPacketSizeByWriter) {
  const QuicByteCount lower_max_packet_size = 1240;
  writer_->set_max_packet_size(lower_max_packet_size);

  static_assert(lower_max_packet_size < kDefaultMaxPacketSize,
                "Default maximum packet size is too low");
  connection_.SetMaxPacketLength(kDefaultMaxPacketSize);

  EXPECT_EQ(lower_max_packet_size, connection_.max_packet_length());
}

TEST_P(QuicConnectionTest, LimitMaxPacketSizeByWriterForNewConnection) {
  const QuicConnectionId connection_id = 17;
  const QuicByteCount lower_max_packet_size = 1240;
  writer_->set_max_packet_size(lower_max_packet_size);
  TestConnection connection(connection_id, kPeerAddress, helper_.get(),
                            alarm_factory_.get(), writer_.get(),
                            Perspective::IS_CLIENT, version());
  EXPECT_EQ(Perspective::IS_CLIENT, connection.perspective());
  EXPECT_EQ(lower_max_packet_size, connection.max_packet_length());
}

TEST_P(QuicConnectionTest, PacketsInOrder) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  ProcessPacket(1);
  EXPECT_EQ(1u, LargestAcked(*outgoing_ack()));
  EXPECT_EQ(1u, outgoing_ack()->packets.NumIntervals());

  ProcessPacket(2);
  EXPECT_EQ(2u, LargestAcked(*outgoing_ack()));
  EXPECT_EQ(1u, outgoing_ack()->packets.NumIntervals());

  ProcessPacket(3);
  EXPECT_EQ(3u, LargestAcked(*outgoing_ack()));
  EXPECT_EQ(1u, outgoing_ack()->packets.NumIntervals());
}

TEST_P(QuicConnectionTest, PacketsOutOfOrder) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  ProcessPacket(3);
  EXPECT_EQ(3u, LargestAcked(*outgoing_ack()));
  EXPECT_TRUE(IsMissing(2));
  EXPECT_TRUE(IsMissing(1));

  ProcessPacket(2);
  EXPECT_EQ(3u, LargestAcked(*outgoing_ack()));
  EXPECT_FALSE(IsMissing(2));
  EXPECT_TRUE(IsMissing(1));

  ProcessPacket(1);
  EXPECT_EQ(3u, LargestAcked(*outgoing_ack()));
  EXPECT_FALSE(IsMissing(2));
  EXPECT_FALSE(IsMissing(1));
}

TEST_P(QuicConnectionTest, DuplicatePacket) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  ProcessPacket(3);
  EXPECT_EQ(3u, LargestAcked(*outgoing_ack()));
  EXPECT_TRUE(IsMissing(2));
  EXPECT_TRUE(IsMissing(1));

  // Send packet 3 again, but do not set the expectation that
  // the visitor OnStreamFrame() will be called.
  ProcessDataPacket(3);
  EXPECT_EQ(3u, LargestAcked(*outgoing_ack()));
  EXPECT_TRUE(IsMissing(2));
  EXPECT_TRUE(IsMissing(1));
}

TEST_P(QuicConnectionTest, PacketsOutOfOrderWithAdditionsAndLeastAwaiting) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  ProcessPacket(3);
  EXPECT_EQ(3u, LargestAcked(*outgoing_ack()));
  EXPECT_TRUE(IsMissing(2));
  EXPECT_TRUE(IsMissing(1));

  ProcessPacket(2);
  EXPECT_EQ(3u, LargestAcked(*outgoing_ack()));
  EXPECT_TRUE(IsMissing(1));

  ProcessPacket(5);
  EXPECT_EQ(5u, LargestAcked(*outgoing_ack()));
  EXPECT_TRUE(IsMissing(1));
  EXPECT_TRUE(IsMissing(4));

  // Pretend at this point the client has gotten acks for 2 and 3 and 1 is a
  // packet the peer will not retransmit.  It indicates this by sending 'least
  // awaiting' is 4.  The connection should then realize 1 will not be
  // retransmitted, and will remove it from the missing list.
  QuicAckFrame frame = InitAckFrame(1);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(_, _, _, _, _));
  ProcessAckPacket(6, &frame);

  // Force an ack to be sent.
  SendAckPacketToPeer();
  EXPECT_TRUE(IsMissing(4));
}

TEST_P(QuicConnectionTest, RejectPacketTooFarOut) {
  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_INVALID_PACKET_HEADER, _,
                                           ConnectionCloseSource::FROM_SELF));

  // Call ProcessDataPacket rather than ProcessPacket, as we should not get a
  // packet call to the visitor.
  if (GetQuicRestartFlag(quic_enable_accept_random_ipn)) {
    ProcessDataPacket(kMaxRandomInitialPacketNumber + 6000);
  } else {
    ProcessDataPacket(6000);
  }
  EXPECT_FALSE(QuicConnectionPeer::GetConnectionClosePacket(&connection_) ==
               nullptr);
}

TEST_P(QuicConnectionTest, RejectUnencryptedStreamData) {
  // EXPECT_QUIC_BUG tests are expensive so only run one instance of them.
  if (!IsDefaultTestConfiguration()) {
    return;
  }

  // Process an unencrypted packet from the non-crypto stream.
  frame1_.stream_id = 3;
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_UNENCRYPTED_STREAM_DATA, _,
                                           ConnectionCloseSource::FROM_SELF));
  EXPECT_QUIC_BUG(ProcessDataPacket(1), "");
  EXPECT_FALSE(QuicConnectionPeer::GetConnectionClosePacket(&connection_) ==
               nullptr);
  const std::vector<QuicConnectionCloseFrame>& connection_close_frames =
      writer_->connection_close_frames();
  EXPECT_EQ(1u, connection_close_frames.size());
  EXPECT_EQ(QUIC_UNENCRYPTED_STREAM_DATA,
            connection_close_frames[0].error_code);
}

TEST_P(QuicConnectionTest, OutOfOrderReceiptCausesAckSend) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  ProcessPacket(3);
  // Should ack immediately since we have missing packets.
  EXPECT_EQ(1u, writer_->packets_write_attempts());

  ProcessPacket(2);
  // Should ack immediately since we have missing packets.
  EXPECT_EQ(2u, writer_->packets_write_attempts());

  ProcessPacket(1);
  // Should ack immediately, since this fills the last hole.
  EXPECT_EQ(3u, writer_->packets_write_attempts());

  ProcessPacket(4);
  // Should not cause an ack.
  EXPECT_EQ(3u, writer_->packets_write_attempts());
}

TEST_P(QuicConnectionTest, OutOfOrderAckReceiptCausesNoAck) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  SendStreamDataToPeer(1, "foo", 0, NO_FIN, nullptr);
  SendStreamDataToPeer(1, "bar", 3, NO_FIN, nullptr);
  EXPECT_EQ(2u, writer_->packets_write_attempts());

  QuicAckFrame ack1 = InitAckFrame(1);
  QuicAckFrame ack2 = InitAckFrame(2);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  ProcessAckPacket(2, &ack2);
  // Should ack immediately since we have missing packets.
  EXPECT_EQ(2u, writer_->packets_write_attempts());

  ProcessAckPacket(1, &ack1);
  // Should not ack an ack filling a missing packet.
  EXPECT_EQ(2u, writer_->packets_write_attempts());
}

TEST_P(QuicConnectionTest, AckReceiptCausesAckSend) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  QuicPacketNumber original, second;

  QuicByteCount packet_size =
      SendStreamDataToPeer(3, "foo", 0, NO_FIN, &original);  // 1st packet.
  SendStreamDataToPeer(3, "bar", 3, NO_FIN, &second);        // 2nd packet.

  QuicAckFrame frame = InitAckFrame({{second, second + 1}});
  // First nack triggers early retransmit.
  LostPacketVector lost_packets;
  lost_packets.push_back(LostPacket(original, kMaxPacketSize));
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _))
      .WillOnce(SetArgPointee<5>(lost_packets));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  QuicPacketNumber retransmission;
  // Packet 1 is short header for IETF QUIC because the encryption level
  // switched to ENCRYPTION_FORWARD_SECURE in SendStreamDataToPeer.
  EXPECT_CALL(
      *send_algorithm_,
      OnPacketSent(_, _, _,
                   GetParam().version.transport_version > QUIC_VERSION_43
                       ? packet_size
                       : packet_size - kQuicVersionSize,
                   _))
      .WillOnce(SaveArg<2>(&retransmission));

  ProcessAckPacket(&frame);

  QuicAckFrame frame2 = ConstructAckFrame(retransmission, original);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _));
  ProcessAckPacket(&frame2);

  // Now if the peer sends an ack which still reports the retransmitted packet
  // as missing, that will bundle an ack with data after two acks in a row
  // indicate the high water mark needs to be raised.
  EXPECT_CALL(*send_algorithm_,
              OnPacketSent(_, _, _, _, HAS_RETRANSMITTABLE_DATA));
  connection_.SendStreamDataWithString(3, "foo", 6, NO_FIN);
  // No ack sent.
  EXPECT_EQ(1u, writer_->frame_count());
  EXPECT_EQ(1u, writer_->stream_frames().size());

  // No more packet loss for the rest of the test.
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _))
      .Times(AnyNumber());
  ProcessAckPacket(&frame2);
  EXPECT_CALL(*send_algorithm_,
              OnPacketSent(_, _, _, _, HAS_RETRANSMITTABLE_DATA));
  connection_.SendStreamDataWithString(3, "foo", 9, NO_FIN);
  // Ack bundled.
  if (GetParam().no_stop_waiting) {
    EXPECT_EQ(2u, writer_->frame_count());
  } else {
    EXPECT_EQ(3u, writer_->frame_count());
  }
  EXPECT_EQ(1u, writer_->stream_frames().size());
  EXPECT_FALSE(writer_->ack_frames().empty());

  // But an ack with no missing packets will not send an ack.
  AckPacket(original, &frame2);
  ProcessAckPacket(&frame2);
  ProcessAckPacket(&frame2);
}

TEST_P(QuicConnectionTest, 20AcksCausesAckSend) {
  if (connection_.version().transport_version != QUIC_VERSION_35) {
    return;
  }
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  SendStreamDataToPeer(1, "foo", 0, NO_FIN, nullptr);

  QuicAlarm* ack_alarm = QuicConnectionPeer::GetAckAlarm(&connection_);
  // But an ack with no missing packets will not send an ack.
  QuicAckFrame frame = InitAckFrame(1);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  for (int i = 0; i < 19; ++i) {
    ProcessAckPacket(&frame);
    EXPECT_FALSE(ack_alarm->IsSet());
  }
  EXPECT_EQ(1u, writer_->packets_write_attempts());
  // The 20th ack packet will cause an ack to be sent.
  ProcessAckPacket(&frame);
  EXPECT_EQ(2u, writer_->packets_write_attempts());
}

TEST_P(QuicConnectionTest, AckNeedsRetransmittableFrames) {
  if (connection_.version().transport_version == QUIC_VERSION_35) {
    return;
  }

  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(99);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(19);
  // Receives packets 1 - 39.
  for (size_t i = 1; i <= 39; ++i) {
    ProcessDataPacket(i);
  }
  // Receiving Packet 40 causes 20th ack to send. Session is informed and adds
  // WINDOW_UPDATE.
  EXPECT_CALL(visitor_, OnAckNeedsRetransmittableFrame())
      .WillOnce(Invoke([this]() {
        connection_.SendControlFrame(
            QuicFrame(new QuicWindowUpdateFrame(1, 0, 0)));
      }));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  EXPECT_EQ(0u, writer_->window_update_frames().size());
  ProcessDataPacket(40);
  EXPECT_EQ(1u, writer_->window_update_frames().size());

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(9);
  // Receives packets 41 - 59.
  for (size_t i = 41; i <= 59; ++i) {
    ProcessDataPacket(i);
  }
  // Send a packet containing stream frame.
  SendStreamDataToPeer(1, "bar", 0, NO_FIN, nullptr);

  // Session will not be informed until receiving another 20 packets.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(19);
  for (size_t i = 60; i <= 98; ++i) {
    ProcessDataPacket(i);
    EXPECT_EQ(0u, writer_->window_update_frames().size());
  }
  // Session does not add a retransmittable frame.
  EXPECT_CALL(visitor_, OnAckNeedsRetransmittableFrame())
      .WillOnce(Invoke([this]() {
        connection_.SendControlFrame(QuicFrame(QuicPingFrame(1)));
      }));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  EXPECT_EQ(0u, writer_->ping_frames().size());
  ProcessDataPacket(99);
  EXPECT_EQ(0u, writer_->window_update_frames().size());
  // A ping frame will be added.
  EXPECT_EQ(1u, writer_->ping_frames().size());
}

TEST_P(QuicConnectionTest, LeastUnackedLower) {
  if (GetParam().version.transport_version > QUIC_VERSION_43) {
    return;
  }
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  SendStreamDataToPeer(1, "foo", 0, NO_FIN, nullptr);
  SendStreamDataToPeer(1, "bar", 3, NO_FIN, nullptr);
  SendStreamDataToPeer(1, "eep", 6, NO_FIN, nullptr);

  // Start out saying the least unacked is 2.
  QuicPacketCreatorPeer::SetPacketNumber(&peer_creator_, 5);
  QuicStopWaitingFrame frame = InitStopWaitingFrame(2);
  ProcessStopWaitingPacket(&frame);

  // Change it to 1, but lower the packet number to fake out-of-order packets.
  // This should be fine.
  QuicPacketCreatorPeer::SetPacketNumber(&peer_creator_, 1);
  // The scheduler will not process out of order acks, but all packet processing
  // causes the connection to try to write.
  if (!GetParam().no_stop_waiting) {
    EXPECT_CALL(visitor_, OnCanWrite());
  }
  QuicStopWaitingFrame frame2 = InitStopWaitingFrame(1);
  ProcessStopWaitingPacket(&frame2);

  // Now claim it's one, but set the ordering so it was sent "after" the first
  // one.  This should cause a connection error.
  QuicPacketCreatorPeer::SetPacketNumber(&peer_creator_, 7);
  if (!GetParam().no_stop_waiting) {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
    EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_INVALID_STOP_WAITING_DATA, _,
                                             ConnectionCloseSource::FROM_SELF));
  }
  QuicStopWaitingFrame frame3 = InitStopWaitingFrame(1);
  ProcessStopWaitingPacket(&frame3);
}

TEST_P(QuicConnectionTest, TooManySentPackets) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  QuicPacketCount max_tracked_packets = 50;
  QuicConnectionPeer::SetMaxTrackedPackets(&connection_, max_tracked_packets);

  const int num_packets = max_tracked_packets + 5;

  for (int i = 0; i < num_packets; ++i) {
    SendStreamDataToPeer(1, "foo", 3 * i, NO_FIN, nullptr);
  }

  // Ack packet 1, which leaves more than the limit outstanding.
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  EXPECT_CALL(visitor_,
              OnConnectionClosed(QUIC_TOO_MANY_OUTSTANDING_SENT_PACKETS, _,
                                 ConnectionCloseSource::FROM_SELF));

  // Nack the first packet and ack the rest, leaving a huge gap.
  QuicAckFrame frame1 = ConstructAckFrame(num_packets, 1);
  ProcessAckPacket(&frame1);
}

TEST_P(QuicConnectionTest, LargestObservedLower) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  SendStreamDataToPeer(1, "foo", 0, NO_FIN, nullptr);
  SendStreamDataToPeer(1, "bar", 3, NO_FIN, nullptr);
  SendStreamDataToPeer(1, "eep", 6, NO_FIN, nullptr);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));

  // Start out saying the largest observed is 2.
  QuicAckFrame frame1 = InitAckFrame(1);
  QuicAckFrame frame2 = InitAckFrame(2);
  ProcessAckPacket(&frame2);

  // Now change it to 1, and it should cause a connection error.
  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_INVALID_ACK_DATA, _,
                                           ConnectionCloseSource::FROM_SELF));
  EXPECT_CALL(visitor_, OnCanWrite()).Times(0);
  ProcessAckPacket(&frame1);
}

TEST_P(QuicConnectionTest, AckUnsentData) {
  // Ack a packet which has not been sent.
  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_INVALID_ACK_DATA, _,
                                           ConnectionCloseSource::FROM_SELF));
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
  QuicAckFrame frame = InitAckFrame(1);
  EXPECT_CALL(visitor_, OnCanWrite()).Times(0);
  ProcessAckPacket(&frame);
}

TEST_P(QuicConnectionTest, AckAll) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  ProcessPacket(1);

  QuicPacketCreatorPeer::SetPacketNumber(&peer_creator_, 1);
  QuicAckFrame frame1;
  ProcessAckPacket(&frame1);
}

TEST_P(QuicConnectionTest, BasicSending) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  QuicPacketNumber last_packet;
  SendStreamDataToPeer(1, "foo", 0, NO_FIN, &last_packet);  // Packet 1
  EXPECT_EQ(1u, last_packet);
  SendAckPacketToPeer();  // Packet 2

  if (GetParam().no_stop_waiting) {
    // Expect no stop waiting frame is sent.
    EXPECT_EQ(0u, least_unacked());
  } else {
    EXPECT_EQ(1u, least_unacked());
  }

  SendAckPacketToPeer();  // Packet 3
  if (GetParam().no_stop_waiting) {
    // Expect no stop waiting frame is sent.
    EXPECT_EQ(0u, least_unacked());
  } else {
    EXPECT_EQ(1u, least_unacked());
  }

  SendStreamDataToPeer(1, "bar", 3, NO_FIN, &last_packet);  // Packet 4
  EXPECT_EQ(4u, last_packet);
  SendAckPacketToPeer();  // Packet 5
  if (GetParam().no_stop_waiting) {
    // Expect no stop waiting frame is sent.
    EXPECT_EQ(0u, least_unacked());
  } else {
    EXPECT_EQ(1u, least_unacked());
  }

  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));

  // Peer acks up to packet 3.
  QuicAckFrame frame = InitAckFrame(3);
  ProcessAckPacket(&frame);
  SendAckPacketToPeer();  // Packet 6

  // As soon as we've acked one, we skip ack packets 2 and 3 and note lack of
  // ack for 4.
  if (GetParam().no_stop_waiting) {
    // Expect no stop waiting frame is sent.
    EXPECT_EQ(0u, least_unacked());
  } else {
    EXPECT_EQ(4u, least_unacked());
  }

  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));

  // Peer acks up to packet 4, the last packet.
  QuicAckFrame frame2 = InitAckFrame(6);
  ProcessAckPacket(&frame2);  // Acks don't instigate acks.

  // Verify that we did not send an ack.
  EXPECT_EQ(6u, writer_->header().packet_number);

  // So the last ack has not changed.
  if (GetParam().no_stop_waiting) {
    // Expect no stop waiting frame is sent.
    EXPECT_EQ(0u, least_unacked());
  } else {
    EXPECT_EQ(4u, least_unacked());
  }

  // If we force an ack, we shouldn't change our retransmit state.
  SendAckPacketToPeer();  // Packet 7
  if (GetParam().no_stop_waiting) {
    // Expect no stop waiting frame is sent.
    EXPECT_EQ(0u, least_unacked());
  } else {
    EXPECT_EQ(7u, least_unacked());
  }

  // But if we send more data it should.
  SendStreamDataToPeer(1, "eep", 6, NO_FIN, &last_packet);  // Packet 8
  EXPECT_EQ(8u, last_packet);
  SendAckPacketToPeer();  // Packet 9
  if (GetParam().no_stop_waiting) {
    // Expect no stop waiting frame is sent.
    EXPECT_EQ(0u, least_unacked());
  } else {
    EXPECT_EQ(7u, least_unacked());
  }
}

// QuicConnection should record the packet sent-time prior to sending the
// packet.
TEST_P(QuicConnectionTest, RecordSentTimeBeforePacketSent) {
  // We're using a MockClock for the tests, so we have complete control over the
  // time.
  // Our recorded timestamp for the last packet sent time will be passed in to
  // the send_algorithm.  Make sure that it is set to the correct value.
  QuicTime actual_recorded_send_time = QuicTime::Zero();
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .WillOnce(SaveArg<0>(&actual_recorded_send_time));

  // First send without any pause and check the result.
  QuicTime expected_recorded_send_time = clock_.Now();
  connection_.SendStreamDataWithString(1, "foo", 0, NO_FIN);
  EXPECT_EQ(expected_recorded_send_time, actual_recorded_send_time)
      << "Expected time = " << expected_recorded_send_time.ToDebuggingValue()
      << ".  Actual time = " << actual_recorded_send_time.ToDebuggingValue();

  // Now pause during the write, and check the results.
  actual_recorded_send_time = QuicTime::Zero();
  const QuicTime::Delta write_pause_time_delta =
      QuicTime::Delta::FromMilliseconds(5000);
  SetWritePauseTimeDelta(write_pause_time_delta);
  expected_recorded_send_time = clock_.Now();

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .WillOnce(SaveArg<0>(&actual_recorded_send_time));
  connection_.SendStreamDataWithString(2, "baz", 0, NO_FIN);
  EXPECT_EQ(expected_recorded_send_time, actual_recorded_send_time)
      << "Expected time = " << expected_recorded_send_time.ToDebuggingValue()
      << ".  Actual time = " << actual_recorded_send_time.ToDebuggingValue();
}

TEST_P(QuicConnectionTest, FramePacking) {
  // Send two stream frames in 1 packet by queueing them.
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  {
    QuicConnection::ScopedPacketFlusher flusher(&connection_,
                                                QuicConnection::SEND_ACK);
    connection_.SendStreamData3();
    connection_.SendStreamData5();
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  }
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  EXPECT_FALSE(connection_.HasQueuedData());

  // Parse the last packet and ensure it's an ack and two stream frames from
  // two different streams.
  if (GetParam().no_stop_waiting) {
    EXPECT_EQ(2u, writer_->frame_count());
    EXPECT_TRUE(writer_->stop_waiting_frames().empty());
  } else {
    EXPECT_EQ(2u, writer_->frame_count());
    EXPECT_TRUE(writer_->stop_waiting_frames().empty());
  }

  EXPECT_TRUE(writer_->ack_frames().empty());

  ASSERT_EQ(2u, writer_->stream_frames().size());
  EXPECT_EQ(GetNthClientInitiatedStreamId(1, connection_.transport_version()),
            writer_->stream_frames()[0]->stream_id);
  EXPECT_EQ(GetNthClientInitiatedStreamId(2, connection_.transport_version()),
            writer_->stream_frames()[1]->stream_id);
}

TEST_P(QuicConnectionTest, FramePackingNonCryptoThenCrypto) {
  // Send two stream frames (one non-crypto, then one crypto) in 2 packets by
  // queueing them.
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(2);
    QuicConnection::ScopedPacketFlusher flusher(&connection_,
                                                QuicConnection::SEND_ACK);
    connection_.SendStreamData3();
    connection_.SendCryptoStreamData();
  }
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  EXPECT_FALSE(connection_.HasQueuedData());

  // Parse the last packet and ensure it's the crypto stream frame.
  EXPECT_EQ(2u, writer_->frame_count());
  ASSERT_EQ(1u, writer_->stream_frames().size());
  ASSERT_EQ(1u, writer_->padding_frames().size());
  EXPECT_EQ(QuicUtils::GetCryptoStreamId(connection_.transport_version()),
            writer_->stream_frames()[0]->stream_id);
}

TEST_P(QuicConnectionTest, FramePackingCryptoThenNonCrypto) {
  // Send two stream frames (one crypto, then one non-crypto) in 2 packets by
  // queueing them.
  {
    connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(2);
    QuicConnection::ScopedPacketFlusher flusher(&connection_,
                                                QuicConnection::SEND_ACK);
    connection_.SendCryptoStreamData();
    connection_.SendStreamData3();
  }
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  EXPECT_FALSE(connection_.HasQueuedData());

  // Parse the last packet and ensure it's the stream frame from stream 3.
  EXPECT_EQ(1u, writer_->frame_count());
  ASSERT_EQ(1u, writer_->stream_frames().size());
  EXPECT_EQ(GetNthClientInitiatedStreamId(1, connection_.transport_version()),
            writer_->stream_frames()[0]->stream_id);
}

TEST_P(QuicConnectionTest, FramePackingAckResponse) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  // Process a data packet to queue up a pending ack.
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacket(1);

  EXPECT_CALL(visitor_, OnCanWrite())
      .WillOnce(DoAll(IgnoreResult(InvokeWithoutArgs(
                          &connection_, &TestConnection::SendStreamData3)),
                      IgnoreResult(InvokeWithoutArgs(
                          &connection_, &TestConnection::SendStreamData5))));

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);

  // Process an ack to cause the visitor's OnCanWrite to be invoked.
  QuicAckFrame ack_one;
  ProcessAckPacket(3, &ack_one);

  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  EXPECT_FALSE(connection_.HasQueuedData());

  // Parse the last packet and ensure it's an ack and two stream frames from
  // two different streams.
  if (GetParam().no_stop_waiting) {
    EXPECT_EQ(3u, writer_->frame_count());
    EXPECT_TRUE(writer_->stop_waiting_frames().empty());
  } else {
    EXPECT_EQ(4u, writer_->frame_count());
    EXPECT_FALSE(writer_->stop_waiting_frames().empty());
  }
  EXPECT_FALSE(writer_->ack_frames().empty());
  ASSERT_EQ(2u, writer_->stream_frames().size());
  EXPECT_EQ(GetNthClientInitiatedStreamId(1, connection_.transport_version()),
            writer_->stream_frames()[0]->stream_id);
  EXPECT_EQ(GetNthClientInitiatedStreamId(2, connection_.transport_version()),
            writer_->stream_frames()[1]->stream_id);
}

TEST_P(QuicConnectionTest, FramePackingSendv) {
  // Send data in 1 packet by writing multiple blocks in a single iovector
  // using writev.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));

  char data[] = "ABCDEF";
  struct iovec iov[2];
  iov[0].iov_base = data;
  iov[0].iov_len = 4;
  iov[1].iov_base = data + 4;
  iov[1].iov_len = 2;
  connection_.SaveAndSendStreamData(
      QuicUtils::GetCryptoStreamId(connection_.transport_version()), iov, 2, 6,
      0, NO_FIN);

  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  EXPECT_FALSE(connection_.HasQueuedData());

  // Parse the last packet and ensure multiple iovector blocks have
  // been packed into a single stream frame from one stream.
  EXPECT_EQ(2u, writer_->frame_count());
  EXPECT_EQ(1u, writer_->stream_frames().size());
  EXPECT_EQ(1u, writer_->padding_frames().size());
  QuicStreamFrame* frame = writer_->stream_frames()[0].get();
  EXPECT_EQ(QuicUtils::GetCryptoStreamId(connection_.transport_version()),
            frame->stream_id);
  EXPECT_EQ("ABCDEF", QuicStringPiece(frame->data_buffer, frame->data_length));
}

TEST_P(QuicConnectionTest, FramePackingSendvQueued) {
  // Try to send two stream frames in 1 packet by using writev.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));

  BlockOnNextWrite();
  char data[] = "ABCDEF";
  struct iovec iov[2];
  iov[0].iov_base = data;
  iov[0].iov_len = 4;
  iov[1].iov_base = data + 4;
  iov[1].iov_len = 2;
  connection_.SaveAndSendStreamData(
      QuicUtils::GetCryptoStreamId(connection_.transport_version()), iov, 2, 6,
      0, NO_FIN);

  EXPECT_EQ(1u, connection_.NumQueuedPackets());
  EXPECT_TRUE(connection_.HasQueuedData());

  // Unblock the writes and actually send.
  writer_->SetWritable();
  connection_.OnCanWrite();
  EXPECT_EQ(0u, connection_.NumQueuedPackets());

  // Parse the last packet and ensure it's one stream frame from one stream.
  EXPECT_EQ(2u, writer_->frame_count());
  EXPECT_EQ(1u, writer_->stream_frames().size());
  EXPECT_EQ(1u, writer_->padding_frames().size());
  EXPECT_EQ(QuicUtils::GetCryptoStreamId(connection_.transport_version()),
            writer_->stream_frames()[0]->stream_id);
}

TEST_P(QuicConnectionTest, SendingZeroBytes) {
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  // Send a zero byte write with a fin using writev.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
  connection_.SaveAndSendStreamData(
      QuicUtils::GetHeadersStreamId(connection_.transport_version()), nullptr,
      0, 0, 0, FIN);

  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  EXPECT_FALSE(connection_.HasQueuedData());

  // Parse the last packet and ensure it's one stream frame from one stream.
  EXPECT_EQ(1u, writer_->frame_count());
  EXPECT_EQ(1u, writer_->stream_frames().size());
  EXPECT_EQ(QuicUtils::GetHeadersStreamId(connection_.transport_version()),
            writer_->stream_frames()[0]->stream_id);
  EXPECT_TRUE(writer_->stream_frames()[0]->fin);
}

TEST_P(QuicConnectionTest, LargeSendWithPendingAck) {
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  // Set the ack alarm by processing a ping frame.
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  // Processs a PING frame.
  ProcessFramePacket(QuicFrame(QuicPingFrame()));
  // Ensure that this has caused the ACK alarm to be set.
  QuicAlarm* ack_alarm = QuicConnectionPeer::GetAckAlarm(&connection_);
  EXPECT_TRUE(ack_alarm->IsSet());

  // Send data and ensure the ack is bundled.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(8);
  size_t len = 10000;
  std::unique_ptr<char[]> data_array(new char[len]);
  memset(data_array.get(), '?', len);
  struct iovec iov;
  iov.iov_base = data_array.get();
  iov.iov_len = len;
  QuicConsumedData consumed = connection_.SaveAndSendStreamData(
      QuicUtils::GetHeadersStreamId(connection_.transport_version()), &iov, 1,
      len, 0, FIN);
  EXPECT_EQ(len, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  EXPECT_FALSE(connection_.HasQueuedData());

  // Parse the last packet and ensure it's one stream frame with a fin.
  EXPECT_EQ(1u, writer_->frame_count());
  EXPECT_EQ(1u, writer_->stream_frames().size());
  EXPECT_EQ(QuicUtils::GetHeadersStreamId(connection_.transport_version()),
            writer_->stream_frames()[0]->stream_id);
  EXPECT_TRUE(writer_->stream_frames()[0]->fin);
  // Ensure the ack alarm was cancelled when the ack was sent.
  EXPECT_FALSE(ack_alarm->IsSet());
}

TEST_P(QuicConnectionTest, OnCanWrite) {
  // Visitor's OnCanWrite will send data, but will have more pending writes.
  EXPECT_CALL(visitor_, OnCanWrite())
      .WillOnce(DoAll(IgnoreResult(InvokeWithoutArgs(
                          &connection_, &TestConnection::SendStreamData3)),
                      IgnoreResult(InvokeWithoutArgs(
                          &connection_, &TestConnection::SendStreamData5))));
  {
    InSequence seq;
    EXPECT_CALL(visitor_, WillingAndAbleToWrite()).WillOnce(Return(true));
    EXPECT_CALL(visitor_, WillingAndAbleToWrite())
        .WillRepeatedly(Return(false));
  }

  EXPECT_CALL(*send_algorithm_, CanSend(_))
      .WillRepeatedly(testing::Return(true));

  connection_.OnCanWrite();

  // Parse the last packet and ensure it's the two stream frames from
  // two different streams.
  EXPECT_EQ(2u, writer_->frame_count());
  EXPECT_EQ(2u, writer_->stream_frames().size());
  EXPECT_EQ(GetNthClientInitiatedStreamId(1, connection_.transport_version()),
            writer_->stream_frames()[0]->stream_id);
  EXPECT_EQ(GetNthClientInitiatedStreamId(2, connection_.transport_version()),
            writer_->stream_frames()[1]->stream_id);
}

TEST_P(QuicConnectionTest, RetransmitOnNack) {
  QuicPacketNumber last_packet;
  QuicByteCount second_packet_size;
  SendStreamDataToPeer(3, "foo", 0, NO_FIN, &last_packet);  // Packet 1
  second_packet_size =
      SendStreamDataToPeer(3, "foos", 3, NO_FIN, &last_packet);  // Packet 2
  SendStreamDataToPeer(3, "fooos", 7, NO_FIN, &last_packet);     // Packet 3

  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  // Don't lose a packet on an ack, and nothing is retransmitted.
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  QuicAckFrame ack_one = InitAckFrame(1);
  ProcessAckPacket(&ack_one);

  // Lose a packet and ensure it triggers retransmission.
  QuicAckFrame nack_two = ConstructAckFrame(3, 2);
  LostPacketVector lost_packets;
  lost_packets.push_back(LostPacket(2, kMaxPacketSize));
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _))
      .WillOnce(SetArgPointee<5>(lost_packets));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  EXPECT_FALSE(QuicPacketCreatorPeer::SendVersionInPacket(creator_));
  ProcessAckPacket(&nack_two);
}

TEST_P(QuicConnectionTest, DoNotSendQueuedPacketForResetStream) {
  // Block the connection to queue the packet.
  BlockOnNextWrite();

  QuicStreamId stream_id = 2;
  connection_.SendStreamDataWithString(stream_id, "foo", 0, NO_FIN);

  // Now that there is a queued packet, reset the stream.
  SendRstStream(stream_id, QUIC_ERROR_PROCESSING_STREAM, 3);

  // Unblock the connection and verify that only the RST_STREAM is sent.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  writer_->SetWritable();
  connection_.OnCanWrite();
  if (!connection_.session_decides_what_to_write()) {
    // OnCanWrite will cause RST_STREAM be sent again.
    connection_.SendControlFrame(QuicFrame(new QuicRstStreamFrame(
        1, stream_id, QUIC_ERROR_PROCESSING_STREAM, 14)));
  }
  EXPECT_EQ(1u, writer_->frame_count());
  EXPECT_EQ(1u, writer_->rst_stream_frames().size());
}

TEST_P(QuicConnectionTest, SendQueuedPacketForQuicRstStreamNoError) {
  // Block the connection to queue the packet.
  BlockOnNextWrite();

  QuicStreamId stream_id = 2;
  connection_.SendStreamDataWithString(stream_id, "foo", 0, NO_FIN);

  // Now that there is a queued packet, reset the stream.
  SendRstStream(stream_id, QUIC_STREAM_NO_ERROR, 3);

  // Unblock the connection and verify that the RST_STREAM is sent and the data
  // packet is sent.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(AtLeast(2));
  writer_->SetWritable();
  connection_.OnCanWrite();
  if (!connection_.session_decides_what_to_write()) {
    // OnCanWrite will cause RST_STREAM be sent again.
    connection_.SendControlFrame(QuicFrame(
        new QuicRstStreamFrame(1, stream_id, QUIC_STREAM_NO_ERROR, 14)));
  }
  EXPECT_EQ(1u, writer_->frame_count());
  EXPECT_EQ(1u, writer_->rst_stream_frames().size());
}

TEST_P(QuicConnectionTest, DoNotRetransmitForResetStreamOnNack) {
  QuicStreamId stream_id = 2;
  QuicPacketNumber last_packet;
  SendStreamDataToPeer(stream_id, "foo", 0, NO_FIN, &last_packet);
  SendStreamDataToPeer(stream_id, "foos", 3, NO_FIN, &last_packet);
  SendStreamDataToPeer(stream_id, "fooos", 7, NO_FIN, &last_packet);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  SendRstStream(stream_id, QUIC_ERROR_PROCESSING_STREAM, 12);

  // Lose a packet and ensure it does not trigger retransmission.
  QuicAckFrame nack_two = ConstructAckFrame(last_packet, last_packet - 1);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  ProcessAckPacket(&nack_two);
}

TEST_P(QuicConnectionTest, RetransmitForQuicRstStreamNoErrorOnNack) {
  QuicStreamId stream_id = 2;
  QuicPacketNumber last_packet;
  SendStreamDataToPeer(stream_id, "foo", 0, NO_FIN, &last_packet);
  SendStreamDataToPeer(stream_id, "foos", 3, NO_FIN, &last_packet);
  SendStreamDataToPeer(stream_id, "fooos", 7, NO_FIN, &last_packet);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  SendRstStream(stream_id, QUIC_STREAM_NO_ERROR, 12);

  // Lose a packet, ensure it triggers retransmission.
  QuicAckFrame nack_two = ConstructAckFrame(last_packet, last_packet - 1);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  LostPacketVector lost_packets;
  lost_packets.push_back(LostPacket(last_packet - 1, kMaxPacketSize));
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _))
      .WillOnce(SetArgPointee<5>(lost_packets));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(AtLeast(1));
  ProcessAckPacket(&nack_two);
}

TEST_P(QuicConnectionTest, DoNotRetransmitForResetStreamOnRTO) {
  QuicStreamId stream_id = 2;
  QuicPacketNumber last_packet;
  SendStreamDataToPeer(stream_id, "foo", 0, NO_FIN, &last_packet);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  SendRstStream(stream_id, QUIC_ERROR_PROCESSING_STREAM, 3);

  // Fire the RTO and verify that the RST_STREAM is resent, not stream data.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  clock_.AdvanceTime(DefaultRetransmissionTime());
  connection_.GetRetransmissionAlarm()->Fire();
  EXPECT_EQ(1u, writer_->frame_count());
  EXPECT_EQ(1u, writer_->rst_stream_frames().size());
  EXPECT_EQ(stream_id, writer_->rst_stream_frames().front().stream_id);
}

// Ensure that if the only data in flight is non-retransmittable, the
// retransmission alarm is not set.
TEST_P(QuicConnectionTest, CancelRetransmissionAlarmAfterResetStream) {
  QuicStreamId stream_id = 2;
  QuicPacketNumber last_data_packet;
  SendStreamDataToPeer(stream_id, "foo", 0, NO_FIN, &last_data_packet);

  // Cancel the stream.
  const QuicPacketNumber rst_packet = last_data_packet + 1;
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, rst_packet, _, _)).Times(1);
  SendRstStream(stream_id, QUIC_ERROR_PROCESSING_STREAM, 3);

  // Ack the RST_STREAM frame (since it's retransmittable), but not the data
  // packet, which is no longer retransmittable since the stream was cancelled.
  QuicAckFrame nack_stream_data =
      ConstructAckFrame(rst_packet, last_data_packet);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  ProcessAckPacket(&nack_stream_data);

  // Ensure that the data is still in flight, but the retransmission alarm is no
  // longer set.
  EXPECT_GT(QuicSentPacketManagerPeer::GetBytesInFlight(manager_), 0u);
  if (GetQuicReloadableFlag(quic_optimize_inflight_check)) {
    EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());
    // Firing the alarm should remove all bytes_in_flight.
    connection_.GetRetransmissionAlarm()->Fire();
    EXPECT_EQ(0u, QuicSentPacketManagerPeer::GetBytesInFlight(manager_));
  }
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, RetransmitForQuicRstStreamNoErrorOnRTO) {
  connection_.SetMaxTailLossProbes(0);

  QuicStreamId stream_id = 2;
  QuicPacketNumber last_packet;
  SendStreamDataToPeer(stream_id, "foo", 0, NO_FIN, &last_packet);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  SendRstStream(stream_id, QUIC_STREAM_NO_ERROR, 3);

  // Fire the RTO and verify that the RST_STREAM is resent, the stream data
  // is sent.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(AtLeast(2));
  clock_.AdvanceTime(DefaultRetransmissionTime());
  connection_.GetRetransmissionAlarm()->Fire();
  EXPECT_EQ(1u, writer_->frame_count());
  ASSERT_EQ(1u, writer_->rst_stream_frames().size());
  EXPECT_EQ(stream_id, writer_->rst_stream_frames().front().stream_id);
}

TEST_P(QuicConnectionTest, DoNotSendPendingRetransmissionForResetStream) {
  QuicStreamId stream_id = 2;
  QuicPacketNumber last_packet;
  SendStreamDataToPeer(stream_id, "foo", 0, NO_FIN, &last_packet);
  SendStreamDataToPeer(stream_id, "foos", 3, NO_FIN, &last_packet);
  BlockOnNextWrite();
  connection_.SendStreamDataWithString(stream_id, "fooos", 7, NO_FIN);

  // Lose a packet which will trigger a pending retransmission.
  QuicAckFrame ack = ConstructAckFrame(last_packet, last_packet - 1);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  ProcessAckPacket(&ack);

  SendRstStream(stream_id, QUIC_ERROR_PROCESSING_STREAM, 12);

  // Unblock the connection and verify that the RST_STREAM is sent but not the
  // second data packet nor a retransmit.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  writer_->SetWritable();
  connection_.OnCanWrite();
  if (!connection_.session_decides_what_to_write()) {
    // OnCanWrite will cause this RST_STREAM_FRAME be sent again.
    connection_.SendControlFrame(QuicFrame(new QuicRstStreamFrame(
        1, stream_id, QUIC_ERROR_PROCESSING_STREAM, 14)));
  }
  EXPECT_EQ(1u, writer_->frame_count());
  EXPECT_EQ(1u, writer_->rst_stream_frames().size());
  EXPECT_EQ(stream_id, writer_->rst_stream_frames().front().stream_id);
}

TEST_P(QuicConnectionTest, SendPendingRetransmissionForQuicRstStreamNoError) {
  QuicStreamId stream_id = 2;
  QuicPacketNumber last_packet;
  SendStreamDataToPeer(stream_id, "foo", 0, NO_FIN, &last_packet);
  SendStreamDataToPeer(stream_id, "foos", 3, NO_FIN, &last_packet);
  BlockOnNextWrite();
  connection_.SendStreamDataWithString(stream_id, "fooos", 7, NO_FIN);

  // Lose a packet which will trigger a pending retransmission.
  QuicAckFrame ack = ConstructAckFrame(last_packet, last_packet - 1);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  LostPacketVector lost_packets;
  lost_packets.push_back(LostPacket(last_packet - 1, kMaxPacketSize));
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _))
      .WillOnce(SetArgPointee<5>(lost_packets));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  ProcessAckPacket(&ack);

  SendRstStream(stream_id, QUIC_STREAM_NO_ERROR, 12);

  // Unblock the connection and verify that the RST_STREAM is sent and the
  // second data packet or a retransmit is sent.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(AtLeast(2));
  writer_->SetWritable();
  connection_.OnCanWrite();
  // The RST_STREAM_FRAME is sent after queued packets and pending
  // retransmission.
  connection_.SendControlFrame(QuicFrame(
      new QuicRstStreamFrame(1, stream_id, QUIC_STREAM_NO_ERROR, 14)));
  EXPECT_EQ(1u, writer_->frame_count());
  EXPECT_EQ(1u, writer_->rst_stream_frames().size());
}

TEST_P(QuicConnectionTest, RetransmitAckedPacket) {
  QuicPacketNumber last_packet;
  SendStreamDataToPeer(1, "foo", 0, NO_FIN, &last_packet);    // Packet 1
  SendStreamDataToPeer(1, "foos", 3, NO_FIN, &last_packet);   // Packet 2
  SendStreamDataToPeer(1, "fooos", 7, NO_FIN, &last_packet);  // Packet 3

  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  // Instigate a loss with an ack.
  QuicAckFrame nack_two = ConstructAckFrame(3, 2);
  // The first nack should trigger a fast retransmission, but we'll be
  // write blocked, so the packet will be queued.
  BlockOnNextWrite();

  LostPacketVector lost_packets;
  lost_packets.push_back(LostPacket(2, kMaxPacketSize));
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _))
      .WillOnce(SetArgPointee<5>(lost_packets));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  ProcessAckPacket(&nack_two);
  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  // Now, ack the previous transmission.
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(false, _, _, _, _));
  QuicAckFrame ack_all = InitAckFrame(3);
  ProcessAckPacket(&ack_all);

  // Unblock the socket and attempt to send the queued packets. We will always
  // send the retransmission.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, 4, _, _)).Times(1);

  writer_->SetWritable();
  connection_.OnCanWrite();

  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  // We do not store retransmittable frames of this retransmission.
  EXPECT_FALSE(QuicConnectionPeer::HasRetransmittableFrames(&connection_, 4));
}

TEST_P(QuicConnectionTest, RetransmitNackedLargestObserved) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  QuicPacketNumber original, second;

  QuicByteCount packet_size =
      SendStreamDataToPeer(3, "foo", 0, NO_FIN, &original);  // 1st packet.
  SendStreamDataToPeer(3, "bar", 3, NO_FIN, &second);        // 2nd packet.

  QuicAckFrame frame = InitAckFrame({{second, second + 1}});
  // The first nack should retransmit the largest observed packet.
  LostPacketVector lost_packets;
  lost_packets.push_back(LostPacket(original, kMaxPacketSize));
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _))
      .WillOnce(SetArgPointee<5>(lost_packets));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  // Packet 1 is short header for IETF QUIC because the encryption level
  // switched to ENCRYPTION_FORWARD_SECURE in SendStreamDataToPeer.
  EXPECT_CALL(
      *send_algorithm_,
      OnPacketSent(_, _, _,
                   GetParam().version.transport_version > QUIC_VERSION_43
                       ? packet_size
                       : packet_size - kQuicVersionSize,
                   _));
  ProcessAckPacket(&frame);
}

TEST_P(QuicConnectionTest, QueueAfterTwoRTOs) {
  connection_.SetMaxTailLossProbes(0);

  for (int i = 0; i < 10; ++i) {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
    connection_.SendStreamDataWithString(3, "foo", i * 3, NO_FIN);
  }

  // Block the writer and ensure they're queued.
  BlockOnNextWrite();
  clock_.AdvanceTime(DefaultRetransmissionTime());
  // Only one packet should be retransmitted.
  connection_.GetRetransmissionAlarm()->Fire();
  EXPECT_TRUE(connection_.HasQueuedData());

  // Unblock the writer.
  writer_->SetWritable();
  clock_.AdvanceTime(QuicTime::Delta::FromMicroseconds(
      2 * DefaultRetransmissionTime().ToMicroseconds()));
  // Retransmit already retransmitted packets event though the packet number
  // greater than the largest observed.
  if (connection_.session_decides_what_to_write()) {
    // 2 RTOs + 1 TLP.
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(3);
  } else {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(2);
  }
  connection_.GetRetransmissionAlarm()->Fire();
  connection_.OnCanWrite();
}

TEST_P(QuicConnectionTest, WriteBlockedBufferedThenSent) {
  BlockOnNextWrite();
  writer_->set_is_write_blocked_data_buffered(true);
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  connection_.SendStreamDataWithString(1, "foo", 0, NO_FIN);
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());

  writer_->SetWritable();
  connection_.OnCanWrite();
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, WriteBlockedThenSent) {
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  BlockOnNextWrite();
  connection_.SendStreamDataWithString(1, "foo", 0, NO_FIN);
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());
  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  // The second packet should also be queued, in order to ensure packets are
  // never sent out of order.
  writer_->SetWritable();
  connection_.SendStreamDataWithString(1, "foo", 0, NO_FIN);
  EXPECT_EQ(2u, connection_.NumQueuedPackets());

  // Now both are sent in order when we unblock.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(2);
  connection_.OnCanWrite();
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, RetransmitWriteBlockedAckedOriginalThenSent) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  connection_.SendStreamDataWithString(3, "foo", 0, NO_FIN);
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());

  BlockOnNextWrite();
  writer_->set_is_write_blocked_data_buffered(true);
  // Simulate the retransmission alarm firing.
  clock_.AdvanceTime(DefaultRetransmissionTime());
  connection_.GetRetransmissionAlarm()->Fire();

  // Ack the sent packet before the callback returns, which happens in
  // rare circumstances with write blocked sockets.
  QuicAckFrame ack = InitAckFrame(1);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  ProcessAckPacket(&ack);

  writer_->SetWritable();
  connection_.OnCanWrite();
  // There is now a pending packet, but with no retransmittable frames.
  if (GetQuicReloadableFlag(quic_optimize_inflight_check)) {
    EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());
    // Firing the alarm should remove all bytes_in_flight.
    connection_.GetRetransmissionAlarm()->Fire();
    EXPECT_EQ(0u, QuicSentPacketManagerPeer::GetBytesInFlight(manager_));
  }
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());
  EXPECT_FALSE(QuicConnectionPeer::HasRetransmittableFrames(&connection_, 2));
}

TEST_P(QuicConnectionTest, AlarmsWhenWriteBlocked) {
  // Block the connection.
  BlockOnNextWrite();
  connection_.SendStreamDataWithString(3, "foo", 0, NO_FIN);
  EXPECT_EQ(1u, writer_->packets_write_attempts());
  EXPECT_TRUE(writer_->IsWriteBlocked());

  // Set the send alarm. Fire the alarm and ensure it doesn't attempt to write.
  connection_.GetSendAlarm()->Set(clock_.ApproximateNow());
  connection_.GetSendAlarm()->Fire();
  EXPECT_TRUE(writer_->IsWriteBlocked());
  EXPECT_EQ(1u, writer_->packets_write_attempts());
}

TEST_P(QuicConnectionTest, NoSendAlarmAfterProcessPacketWhenWriteBlocked) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  // Block the connection.
  BlockOnNextWrite();
  connection_.SendStreamDataWithString(3, "foo", 0, NO_FIN);
  EXPECT_TRUE(writer_->IsWriteBlocked());
  EXPECT_EQ(1u, connection_.NumQueuedPackets());
  EXPECT_FALSE(connection_.GetSendAlarm()->IsSet());

  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  // Process packet number 1. Can not call ProcessPacket or ProcessDataPacket
  // here, because they will fire the alarm after QuicConnection::ProcessPacket
  // is returned.
  const QuicPacketNumber received_packet_num = 1;
  const bool has_stop_waiting = false;
  const EncryptionLevel level = ENCRYPTION_NONE;
  std::unique_ptr<QuicPacket> packet(
      ConstructDataPacket(received_packet_num, has_stop_waiting));
  char buffer[kMaxPacketSize];
  size_t encrypted_length = peer_framer_.EncryptPayload(
      level, received_packet_num, *packet, buffer, kMaxPacketSize);
  connection_.ProcessUdpPacket(
      kSelfAddress, kPeerAddress,
      QuicReceivedPacket(buffer, encrypted_length, clock_.Now(), false));

  EXPECT_TRUE(writer_->IsWriteBlocked());
  EXPECT_FALSE(connection_.GetSendAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, AddToWriteBlockedListIfWriterBlockedWhenProcessing) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  SendStreamDataToPeer(1, "foo", 0, NO_FIN, nullptr);

  // Simulate the case where a shared writer gets blocked by another connection.
  writer_->SetWriteBlocked();

  // Process an ACK, make sure the connection calls visitor_->OnWriteBlocked().
  QuicAckFrame ack1 = InitAckFrame(1);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(_, _, _, _, _));
  EXPECT_CALL(visitor_, OnWriteBlocked()).Times(1);
  ProcessAckPacket(1, &ack1);
}

TEST_P(QuicConnectionTest, NoLimitPacketsPerNack) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  int offset = 0;
  // Send packets 1 to 15.
  for (int i = 0; i < 15; ++i) {
    SendStreamDataToPeer(1, "foo", offset, NO_FIN, nullptr);
    offset += 3;
  }

  // Ack 15, nack 1-14.

  QuicAckFrame nack = InitAckFrame({{15, 16}});

  // 14 packets have been NACK'd and lost.
  LostPacketVector lost_packets;
  for (int i = 1; i < 15; ++i) {
    lost_packets.push_back(LostPacket(i, kMaxPacketSize));
  }
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _))
      .WillOnce(SetArgPointee<5>(lost_packets));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  if (connection_.session_decides_what_to_write()) {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  } else {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(14);
  }
  ProcessAckPacket(&nack);
}

// Test sending multiple acks from the connection to the session.
TEST_P(QuicConnectionTest, MultipleAcks) {
  QuicPacketNumber last_packet;
  SendStreamDataToPeer(1, "foo", 0, NO_FIN, &last_packet);  // Packet 1
  EXPECT_EQ(1u, last_packet);
  SendStreamDataToPeer(3, "foo", 0, NO_FIN, &last_packet);  // Packet 2
  EXPECT_EQ(2u, last_packet);
  SendAckPacketToPeer();                                    // Packet 3
  SendStreamDataToPeer(5, "foo", 0, NO_FIN, &last_packet);  // Packet 4
  EXPECT_EQ(4u, last_packet);
  SendStreamDataToPeer(1, "foo", 3, NO_FIN, &last_packet);  // Packet 5
  EXPECT_EQ(5u, last_packet);
  SendStreamDataToPeer(3, "foo", 3, NO_FIN, &last_packet);  // Packet 6
  EXPECT_EQ(6u, last_packet);

  // Client will ack packets 1, 2, [!3], 4, 5.
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  QuicAckFrame frame1 = ConstructAckFrame(5, 3);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  ProcessAckPacket(&frame1);

  // Now the client implicitly acks 3, and explicitly acks 6.
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  QuicAckFrame frame2 = InitAckFrame(6);
  ProcessAckPacket(&frame2);
}

TEST_P(QuicConnectionTest, DontLatchUnackedPacket) {
  SendStreamDataToPeer(1, "foo", 0, NO_FIN, nullptr);  // Packet 1;
  // From now on, we send acks, so the send algorithm won't mark them pending.
  SendAckPacketToPeer();  // Packet 2

  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  QuicAckFrame frame = InitAckFrame(1);
  ProcessAckPacket(&frame);

  // Verify that our internal state has least-unacked as 2, because we're still
  // waiting for a potential ack for 2.

  EXPECT_EQ(2u, stop_waiting()->least_unacked);

  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  frame = InitAckFrame(2);
  ProcessAckPacket(&frame);
  EXPECT_EQ(3u, stop_waiting()->least_unacked);

  // When we send an ack, we make sure our least-unacked makes sense.  In this
  // case since we're not waiting on an ack for 2 and all packets are acked, we
  // set it to 3.
  SendAckPacketToPeer();  // Packet 3
  // Least_unacked remains at 3 until another ack is received.
  EXPECT_EQ(3u, stop_waiting()->least_unacked);
  if (GetParam().no_stop_waiting) {
    // Expect no stop waiting frame is sent.
    EXPECT_EQ(0u, least_unacked());
  } else {
    // Check that the outgoing ack had its packet number as least_unacked.
    EXPECT_EQ(3u, least_unacked());
  }

  // Ack the ack, which updates the rtt and raises the least unacked.
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  frame = InitAckFrame(3);
  ProcessAckPacket(&frame);

  SendStreamDataToPeer(1, "bar", 3, NO_FIN, nullptr);  // Packet 4
  EXPECT_EQ(4u, stop_waiting()->least_unacked);
  SendAckPacketToPeer();  // Packet 5
  if (GetParam().no_stop_waiting) {
    // Expect no stop waiting frame is sent.
    EXPECT_EQ(0u, least_unacked());
  } else {
    EXPECT_EQ(4u, least_unacked());
  }

  // Send two data packets at the end, and ensure if the last one is acked,
  // the least unacked is raised above the ack packets.
  SendStreamDataToPeer(1, "bar", 6, NO_FIN, nullptr);  // Packet 6
  SendStreamDataToPeer(1, "bar", 9, NO_FIN, nullptr);  // Packet 7

  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  frame = InitAckFrame({{1, 5}, {7, 8}});
  ProcessAckPacket(&frame);

  EXPECT_EQ(6u, stop_waiting()->least_unacked);
}

TEST_P(QuicConnectionTest, TLP) {
  connection_.SetMaxTailLossProbes(1);

  SendStreamDataToPeer(3, "foo", 0, NO_FIN, nullptr);
  EXPECT_EQ(1u, stop_waiting()->least_unacked);
  QuicTime retransmission_time =
      connection_.GetRetransmissionAlarm()->deadline();
  EXPECT_NE(QuicTime::Zero(), retransmission_time);

  EXPECT_EQ(1u, writer_->header().packet_number);
  // Simulate the retransmission alarm firing and sending a tlp,
  // so send algorithm's OnRetransmissionTimeout is not called.
  clock_.AdvanceTime(retransmission_time - clock_.Now());
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, 2u, _, _));
  connection_.GetRetransmissionAlarm()->Fire();
  EXPECT_EQ(2u, writer_->header().packet_number);
  // We do not raise the high water mark yet.
  EXPECT_EQ(1u, stop_waiting()->least_unacked);
}

TEST_P(QuicConnectionTest, RTO) {
  connection_.SetMaxTailLossProbes(0);

  QuicTime default_retransmission_time =
      clock_.ApproximateNow() + DefaultRetransmissionTime();
  SendStreamDataToPeer(3, "foo", 0, NO_FIN, nullptr);
  EXPECT_EQ(1u, stop_waiting()->least_unacked);

  EXPECT_EQ(1u, writer_->header().packet_number);
  EXPECT_EQ(default_retransmission_time,
            connection_.GetRetransmissionAlarm()->deadline());
  // Simulate the retransmission alarm firing.
  clock_.AdvanceTime(DefaultRetransmissionTime());
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, 2u, _, _));
  connection_.GetRetransmissionAlarm()->Fire();
  EXPECT_EQ(2u, writer_->header().packet_number);
  // We do not raise the high water mark yet.
  EXPECT_EQ(1u, stop_waiting()->least_unacked);
}

TEST_P(QuicConnectionTest, RetransmitWithSameEncryptionLevel) {
  use_tagging_decrypter();

  // A TaggingEncrypter puts kTagSize copies of the given byte (0x01 here) at
  // the end of the packet. We can test this to check which encrypter was used.
  connection_.SetEncrypter(ENCRYPTION_NONE,
                           QuicMakeUnique<TaggingEncrypter>(0x01));
  SendStreamDataToPeer(
      QuicUtils::GetCryptoStreamId(connection_.transport_version()), "foo", 0,
      NO_FIN, nullptr);
  EXPECT_EQ(0x01010101u, writer_->final_bytes_of_last_packet());

  connection_.SetEncrypter(ENCRYPTION_INITIAL,
                           QuicMakeUnique<TaggingEncrypter>(0x02));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
  SendStreamDataToPeer(3, "foo", 0, NO_FIN, nullptr);
  EXPECT_EQ(0x02020202u, writer_->final_bytes_of_last_packet());

  {
    InSequence s;
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, 3, _, _));
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, 4, _, _));
  }

  // Manually mark both packets for retransmission.
  connection_.RetransmitUnackedPackets(ALL_UNACKED_RETRANSMISSION);

  // Packet should have been sent with ENCRYPTION_NONE.
  EXPECT_EQ(0x01010101u, writer_->final_bytes_of_previous_packet());

  // Packet should have been sent with ENCRYPTION_INITIAL.
  EXPECT_EQ(0x02020202u, writer_->final_bytes_of_last_packet());
}

TEST_P(QuicConnectionTest, SendHandshakeMessages) {
  use_tagging_decrypter();
  // A TaggingEncrypter puts kTagSize copies of the given byte (0x01 here) at
  // the end of the packet. We can test this to check which encrypter was used.
  connection_.SetEncrypter(ENCRYPTION_NONE,
                           QuicMakeUnique<TaggingEncrypter>(0x01));

  // Attempt to send a handshake message and have the socket block.
  EXPECT_CALL(*send_algorithm_, CanSend(_)).WillRepeatedly(Return(true));
  BlockOnNextWrite();
  connection_.SendStreamDataWithString(
      QuicUtils::GetCryptoStreamId(connection_.transport_version()), "foo", 0,
      NO_FIN);
  // The packet should be serialized, but not queued.
  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  // Switch to the new encrypter.
  connection_.SetEncrypter(ENCRYPTION_INITIAL,
                           QuicMakeUnique<TaggingEncrypter>(0x02));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);

  // Now become writeable and flush the packets.
  writer_->SetWritable();
  EXPECT_CALL(visitor_, OnCanWrite());
  connection_.OnCanWrite();
  EXPECT_EQ(0u, connection_.NumQueuedPackets());

  // Verify that the handshake packet went out at the null encryption.
  EXPECT_EQ(0x01010101u, writer_->final_bytes_of_last_packet());
}

TEST_P(QuicConnectionTest,
       DropRetransmitsForNullEncryptedPacketAfterForwardSecure) {
  use_tagging_decrypter();
  connection_.SetEncrypter(ENCRYPTION_NONE,
                           QuicMakeUnique<TaggingEncrypter>(0x01));
  QuicPacketNumber packet_number;
  SendStreamDataToPeer(
      QuicUtils::GetCryptoStreamId(connection_.transport_version()), "foo", 0,
      NO_FIN, &packet_number);

  // Simulate the retransmission alarm firing and the socket blocking.
  BlockOnNextWrite();
  clock_.AdvanceTime(DefaultRetransmissionTime());
  connection_.GetRetransmissionAlarm()->Fire();

  // Go forward secure.
  connection_.SetEncrypter(ENCRYPTION_FORWARD_SECURE,
                           QuicMakeUnique<TaggingEncrypter>(0x02));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  notifier_.NeuterUnencryptedData();
  connection_.NeuterUnencryptedPackets();

  EXPECT_EQ(QuicTime::Zero(), connection_.GetRetransmissionAlarm()->deadline());
  // Unblock the socket and ensure that no packets are sent.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  writer_->SetWritable();
  connection_.OnCanWrite();
}

TEST_P(QuicConnectionTest, RetransmitPacketsWithInitialEncryption) {
  use_tagging_decrypter();
  connection_.SetEncrypter(ENCRYPTION_NONE,
                           QuicMakeUnique<TaggingEncrypter>(0x01));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_NONE);

  SendStreamDataToPeer(
      QuicUtils::GetCryptoStreamId(connection_.transport_version()), "foo", 0,
      NO_FIN, nullptr);

  connection_.SetEncrypter(ENCRYPTION_INITIAL,
                           QuicMakeUnique<TaggingEncrypter>(0x02));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);

  SendStreamDataToPeer(2, "bar", 0, NO_FIN, nullptr);
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);

  connection_.RetransmitUnackedPackets(ALL_INITIAL_RETRANSMISSION);
}

TEST_P(QuicConnectionTest, BufferNonDecryptablePackets) {
  // SetFromConfig is always called after construction from InitializeSession.
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  QuicConfig config;
  connection_.SetFromConfig(config);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  use_tagging_decrypter();

  const uint8_t tag = 0x07;
  peer_framer_.SetEncrypter(ENCRYPTION_INITIAL,
                            QuicMakeUnique<TaggingEncrypter>(tag));

  // Process an encrypted packet which can not yet be decrypted which should
  // result in the packet being buffered.
  ProcessDataPacketAtLevel(1, !kHasStopWaiting, ENCRYPTION_INITIAL);

  // Transition to the new encryption state and process another encrypted packet
  // which should result in the original packet being processed.
  connection_.SetDecrypter(ENCRYPTION_INITIAL,
                           QuicMakeUnique<StrictTaggingDecrypter>(tag));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
  connection_.SetEncrypter(ENCRYPTION_INITIAL,
                           QuicMakeUnique<TaggingEncrypter>(tag));
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(2);
  ProcessDataPacketAtLevel(2, !kHasStopWaiting, ENCRYPTION_INITIAL);

  // Finally, process a third packet and note that we do not reprocess the
  // buffered packet.
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(3, !kHasStopWaiting, ENCRYPTION_INITIAL);
}

TEST_P(QuicConnectionTest, Buffer100NonDecryptablePackets) {
  if (GetQuicReloadableFlag(quic_decrypt_packets_on_key_change)) {
    return;
  }

  // SetFromConfig is always called after construction from InitializeSession.
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  QuicConfig config;
  config.set_max_undecryptable_packets(100);
  connection_.SetFromConfig(config);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  use_tagging_decrypter();

  const uint8_t tag = 0x07;
  peer_framer_.SetEncrypter(ENCRYPTION_INITIAL,
                            QuicMakeUnique<TaggingEncrypter>(tag));

  // Process an encrypted packet which can not yet be decrypted which should
  // result in the packet being buffered.
  for (QuicPacketNumber i = 1; i <= 100; ++i) {
    ProcessDataPacketAtLevel(i, !kHasStopWaiting, ENCRYPTION_INITIAL);
  }

  // Transition to the new encryption state and process another encrypted packet
  // which should result in the original packets being processed.
  connection_.SetDecrypter(ENCRYPTION_INITIAL,
                           QuicMakeUnique<StrictTaggingDecrypter>(tag));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
  connection_.SetEncrypter(ENCRYPTION_INITIAL,
                           QuicMakeUnique<TaggingEncrypter>(tag));
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(101);
  ProcessDataPacketAtLevel(101, !kHasStopWaiting, ENCRYPTION_INITIAL);

  // Finally, process a third packet and note that we do not reprocess the
  // buffered packet.
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(102, !kHasStopWaiting, ENCRYPTION_INITIAL);
}

TEST_P(QuicConnectionTest, TestRetransmitOrder) {
  connection_.SetMaxTailLossProbes(0);

  QuicByteCount first_packet_size;
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .WillOnce(SaveArg<3>(&first_packet_size));

  connection_.SendStreamDataWithString(3, "first_packet", 0, NO_FIN);
  QuicByteCount second_packet_size;
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .WillOnce(SaveArg<3>(&second_packet_size));
  connection_.SendStreamDataWithString(3, "second_packet", 12, NO_FIN);
  EXPECT_NE(first_packet_size, second_packet_size);
  // Advance the clock by huge time to make sure packets will be retransmitted.
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(10));
  {
    InSequence s;
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, first_packet_size, _));
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, second_packet_size, _));
  }
  connection_.GetRetransmissionAlarm()->Fire();

  // Advance again and expect the packets to be sent again in the same order.
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(20));
  {
    InSequence s;
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, first_packet_size, _));
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, second_packet_size, _));
  }
  connection_.GetRetransmissionAlarm()->Fire();
}

TEST_P(QuicConnectionTest, Buffer100NonDecryptablePacketsThenKeyChange) {
  if (!GetQuicReloadableFlag(quic_decrypt_packets_on_key_change)) {
    return;
  }

  // SetFromConfig is always called after construction from InitializeSession.
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  QuicConfig config;
  config.set_max_undecryptable_packets(100);
  connection_.SetFromConfig(config);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  use_tagging_decrypter();

  const uint8_t tag = 0x07;
  peer_framer_.SetEncrypter(ENCRYPTION_INITIAL,
                            QuicMakeUnique<TaggingEncrypter>(tag));

  // Process an encrypted packet which can not yet be decrypted which should
  // result in the packet being buffered.
  for (QuicPacketNumber i = 1; i <= 100; ++i) {
    ProcessDataPacketAtLevel(i, !kHasStopWaiting, ENCRYPTION_INITIAL);
  }

  // Transition to the new encryption state and process another encrypted packet
  // which should result in the original packets being processed.
  EXPECT_FALSE(connection_.GetProcessUndecryptablePacketsAlarm()->IsSet());
  connection_.SetDecrypter(ENCRYPTION_INITIAL,
                           QuicMakeUnique<StrictTaggingDecrypter>(tag));
  EXPECT_TRUE(connection_.GetProcessUndecryptablePacketsAlarm()->IsSet());
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
  connection_.SetEncrypter(ENCRYPTION_INITIAL,
                           QuicMakeUnique<TaggingEncrypter>(tag));

  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(100);
  connection_.GetProcessUndecryptablePacketsAlarm()->Fire();

  // Finally, process a third packet and note that we do not reprocess the
  // buffered packet.
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(102, !kHasStopWaiting, ENCRYPTION_INITIAL);
}

TEST_P(QuicConnectionTest, SetRTOAfterWritingToSocket) {
  BlockOnNextWrite();
  connection_.SendStreamDataWithString(1, "foo", 0, NO_FIN);
  // Make sure that RTO is not started when the packet is queued.
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());

  // Test that RTO is started once we write to the socket.
  writer_->SetWritable();
  connection_.OnCanWrite();
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, DelayRTOWithAckReceipt) {
  connection_.SetMaxTailLossProbes(0);

  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(2);
  connection_.SendStreamDataWithString(2, "foo", 0, NO_FIN);
  connection_.SendStreamDataWithString(3, "bar", 0, NO_FIN);
  QuicAlarm* retransmission_alarm = connection_.GetRetransmissionAlarm();
  EXPECT_TRUE(retransmission_alarm->IsSet());
  EXPECT_EQ(clock_.Now() + DefaultRetransmissionTime(),
            retransmission_alarm->deadline());

  // Advance the time right before the RTO, then receive an ack for the first
  // packet to delay the RTO.
  clock_.AdvanceTime(DefaultRetransmissionTime());
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  QuicAckFrame ack = InitAckFrame(1);
  ProcessAckPacket(&ack);
  // Now we have an RTT sample of DefaultRetransmissionTime(500ms),
  // so the RTO has increased to 2 * SRTT.
  EXPECT_TRUE(retransmission_alarm->IsSet());
  EXPECT_EQ(retransmission_alarm->deadline(),
            clock_.Now() + 2 * DefaultRetransmissionTime());

  // Move forward past the original RTO and ensure the RTO is still pending.
  clock_.AdvanceTime(2 * DefaultRetransmissionTime());

  // Ensure the second packet gets retransmitted when it finally fires.
  EXPECT_TRUE(retransmission_alarm->IsSet());
  EXPECT_EQ(retransmission_alarm->deadline(), clock_.ApproximateNow());
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
  // Manually cancel the alarm to simulate a real test.
  connection_.GetRetransmissionAlarm()->Fire();

  // The new retransmitted packet number should set the RTO to a larger value
  // than previously.
  EXPECT_TRUE(retransmission_alarm->IsSet());
  QuicTime next_rto_time = retransmission_alarm->deadline();
  QuicTime expected_rto_time =
      connection_.sent_packet_manager().GetRetransmissionTime();
  EXPECT_EQ(next_rto_time, expected_rto_time);
}

TEST_P(QuicConnectionTest, TestQueued) {
  connection_.SetMaxTailLossProbes(0);

  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  BlockOnNextWrite();
  connection_.SendStreamDataWithString(1, "foo", 0, NO_FIN);
  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  // Unblock the writes and actually send.
  writer_->SetWritable();
  connection_.OnCanWrite();
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
}

TEST_P(QuicConnectionTest, InitialTimeout) {
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(AnyNumber());
  EXPECT_FALSE(connection_.GetTimeoutAlarm()->IsSet());

  // SetFromConfig sets the initial timeouts before negotiation.
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  QuicConfig config;
  connection_.SetFromConfig(config);
  // Subtract a second from the idle timeout on the client side.
  QuicTime default_timeout =
      clock_.ApproximateNow() +
      QuicTime::Delta::FromSeconds(kInitialIdleTimeoutSecs - 1);
  EXPECT_EQ(default_timeout, connection_.GetTimeoutAlarm()->deadline());

  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_NETWORK_IDLE_TIMEOUT, _,
                                           ConnectionCloseSource::FROM_SELF));
  // Simulate the timeout alarm firing.
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(kInitialIdleTimeoutSecs - 1));
  connection_.GetTimeoutAlarm()->Fire();

  EXPECT_FALSE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_FALSE(connection_.connected());

  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetPingAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetSendAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetRetransmittableOnWireAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, IdleTimeoutAfterFirstSentPacket) {
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(AnyNumber());
  EXPECT_FALSE(connection_.GetTimeoutAlarm()->IsSet());

  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  QuicConfig config;
  connection_.SetFromConfig(config);
  EXPECT_TRUE(connection_.GetTimeoutAlarm()->IsSet());
  QuicTime initial_ddl =
      clock_.ApproximateNow() +
      QuicTime::Delta::FromSeconds(kInitialIdleTimeoutSecs - 1);
  EXPECT_EQ(initial_ddl, connection_.GetTimeoutAlarm()->deadline());
  EXPECT_TRUE(connection_.connected());

  // Advance the time and send the first packet to the peer.
  clock_.AdvanceTime(QuicTime::Delta::FromMicroseconds(20));
  QuicPacketNumber last_packet;
  SendStreamDataToPeer(1, "foo", 0, NO_FIN, &last_packet);
  EXPECT_EQ(1u, last_packet);
  // This will be the updated deadline for the connection to idle time out.
  QuicTime new_ddl = clock_.ApproximateNow() +
                     QuicTime::Delta::FromSeconds(kInitialIdleTimeoutSecs - 1);

  // Simulate the timeout alarm firing, the connection should not be closed as
  // a new packet has been sent.
  EXPECT_CALL(visitor_, OnConnectionClosed(_, _, _)).Times(0);
  QuicTime::Delta delay = initial_ddl - clock_.ApproximateNow();
  clock_.AdvanceTime(delay);
  connection_.GetTimeoutAlarm()->Fire();
  // Verify the timeout alarm deadline is updated.
  EXPECT_TRUE(connection_.connected());
  EXPECT_TRUE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_EQ(new_ddl, connection_.GetTimeoutAlarm()->deadline());

  // Simulate the timeout alarm firing again, the connection now should be
  // closed.
  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_NETWORK_IDLE_TIMEOUT, _,
                                           ConnectionCloseSource::FROM_SELF));
  clock_.AdvanceTime(new_ddl - clock_.ApproximateNow());
  connection_.GetTimeoutAlarm()->Fire();
  EXPECT_FALSE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_FALSE(connection_.connected());

  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetPingAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetSendAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetRetransmittableOnWireAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, IdleTimeoutAfterSendTwoPackets) {
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(AnyNumber());
  EXPECT_FALSE(connection_.GetTimeoutAlarm()->IsSet());

  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  QuicConfig config;
  connection_.SetFromConfig(config);
  EXPECT_TRUE(connection_.GetTimeoutAlarm()->IsSet());
  QuicTime initial_ddl =
      clock_.ApproximateNow() +
      QuicTime::Delta::FromSeconds(kInitialIdleTimeoutSecs - 1);
  EXPECT_EQ(initial_ddl, connection_.GetTimeoutAlarm()->deadline());
  EXPECT_TRUE(connection_.connected());

  // Immediately send the first packet, this is a rare case but test code will
  // hit this issue often as MockClock used for tests doesn't move with code
  // execution until manually adjusted.
  QuicPacketNumber last_packet;
  SendStreamDataToPeer(1, "foo", 0, NO_FIN, &last_packet);
  EXPECT_EQ(1u, last_packet);

  // Advance the time and send the second packet to the peer.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(20));
  SendStreamDataToPeer(1, "foo", 0, NO_FIN, &last_packet);
  EXPECT_EQ(2u, last_packet);

  if (GetQuicReloadableFlag(
          quic_fix_time_of_first_packet_sent_after_receiving)) {
    // Simulate the timeout alarm firing, the connection will be closed.
    EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_NETWORK_IDLE_TIMEOUT, _,
                                             ConnectionCloseSource::FROM_SELF));
    clock_.AdvanceTime(initial_ddl - clock_.ApproximateNow());
    connection_.GetTimeoutAlarm()->Fire();
  } else {
    // Simulate the timeout alarm firing, the connection will not be closed.
    EXPECT_CALL(visitor_, OnConnectionClosed(_, _, _)).Times(0);
    clock_.AdvanceTime(initial_ddl - clock_.ApproximateNow());
    connection_.GetTimeoutAlarm()->Fire();
    EXPECT_TRUE(connection_.GetTimeoutAlarm()->IsSet());
    EXPECT_TRUE(connection_.connected());

    // Advance another 20ms, and fire the alarm again. The connection will be
    // closed.
    EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_NETWORK_IDLE_TIMEOUT, _,
                                             ConnectionCloseSource::FROM_SELF));
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(20));
    connection_.GetTimeoutAlarm()->Fire();
  }

  EXPECT_FALSE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_FALSE(connection_.connected());

  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetPingAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetSendAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetRetransmittableOnWireAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, HandshakeTimeout) {
  // Use a shorter handshake timeout than idle timeout for this test.
  const QuicTime::Delta timeout = QuicTime::Delta::FromSeconds(5);
  connection_.SetNetworkTimeouts(timeout, timeout);
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(AnyNumber());

  QuicTime handshake_timeout =
      clock_.ApproximateNow() + timeout - QuicTime::Delta::FromSeconds(1);
  EXPECT_EQ(handshake_timeout, connection_.GetTimeoutAlarm()->deadline());
  EXPECT_TRUE(connection_.connected());

  // Send and ack new data 3 seconds later to lengthen the idle timeout.
  SendStreamDataToPeer(
      QuicUtils::GetHeadersStreamId(connection_.transport_version()), "GET /",
      0, FIN, nullptr);
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(3));
  QuicAckFrame frame = InitAckFrame(1);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  ProcessAckPacket(&frame);

  // Fire early to verify it wouldn't timeout yet.
  connection_.GetTimeoutAlarm()->Fire();
  EXPECT_TRUE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_TRUE(connection_.connected());

  clock_.AdvanceTime(timeout - QuicTime::Delta::FromSeconds(2));

  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_HANDSHAKE_TIMEOUT, _,
                                           ConnectionCloseSource::FROM_SELF));
  // Simulate the timeout alarm firing.
  connection_.GetTimeoutAlarm()->Fire();

  EXPECT_FALSE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_FALSE(connection_.connected());

  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetPingAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());
  EXPECT_FALSE(connection_.GetSendAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, PingAfterSend) {
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(visitor_, HasOpenDynamicStreams()).WillRepeatedly(Return(true));
  EXPECT_FALSE(connection_.GetPingAlarm()->IsSet());

  // Advance to 5ms, and send a packet to the peer, which will set
  // the ping alarm.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());
  SendStreamDataToPeer(
      QuicUtils::GetHeadersStreamId(connection_.transport_version()), "GET /",
      0, FIN, nullptr);
  EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
  EXPECT_EQ(clock_.ApproximateNow() + QuicTime::Delta::FromSeconds(15),
            connection_.GetPingAlarm()->deadline());

  // Now recevie an ACK of the previous packet, which will move the
  // ping alarm forward.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  QuicAckFrame frame = InitAckFrame(1);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  ProcessAckPacket(&frame);
  EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
  // The ping timer is set slightly less than 15 seconds in the future, because
  // of the 1s ping timer alarm granularity.
  EXPECT_EQ(clock_.ApproximateNow() + QuicTime::Delta::FromSeconds(15) -
                QuicTime::Delta::FromMilliseconds(5),
            connection_.GetPingAlarm()->deadline());

  writer_->Reset();
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(15));
  EXPECT_CALL(visitor_, SendPing()).WillOnce(Invoke([this]() {
    connection_.SendControlFrame(QuicFrame(QuicPingFrame(1)));
  }));
  connection_.GetPingAlarm()->Fire();
  EXPECT_EQ(1u, writer_->frame_count());
  ASSERT_EQ(1u, writer_->ping_frames().size());
  writer_->Reset();

  EXPECT_CALL(visitor_, HasOpenDynamicStreams()).WillRepeatedly(Return(false));
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  SendAckPacketToPeer();

  EXPECT_FALSE(connection_.GetPingAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, ReducedPingTimeout) {
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(visitor_, HasOpenDynamicStreams()).WillRepeatedly(Return(true));
  EXPECT_FALSE(connection_.GetPingAlarm()->IsSet());

  // Use a reduced ping timeout for this connection.
  connection_.set_ping_timeout(QuicTime::Delta::FromSeconds(10));

  // Advance to 5ms, and send a packet to the peer, which will set
  // the ping alarm.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());
  SendStreamDataToPeer(
      QuicUtils::GetHeadersStreamId(connection_.transport_version()), "GET /",
      0, FIN, nullptr);
  EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
  EXPECT_EQ(clock_.ApproximateNow() + QuicTime::Delta::FromSeconds(10),
            connection_.GetPingAlarm()->deadline());

  // Now recevie an ACK of the previous packet, which will move the
  // ping alarm forward.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  QuicAckFrame frame = InitAckFrame(1);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  ProcessAckPacket(&frame);
  EXPECT_TRUE(connection_.GetPingAlarm()->IsSet());
  // The ping timer is set slightly less than 10 seconds in the future, because
  // of the 1s ping timer alarm granularity.
  EXPECT_EQ(clock_.ApproximateNow() + QuicTime::Delta::FromSeconds(10) -
                QuicTime::Delta::FromMilliseconds(5),
            connection_.GetPingAlarm()->deadline());

  writer_->Reset();
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(10));
  EXPECT_CALL(visitor_, SendPing()).WillOnce(Invoke([this]() {
    connection_.SendControlFrame(QuicFrame(QuicPingFrame(1)));
  }));
  connection_.GetPingAlarm()->Fire();
  EXPECT_EQ(1u, writer_->frame_count());
  ASSERT_EQ(1u, writer_->ping_frames().size());
  writer_->Reset();

  EXPECT_CALL(visitor_, HasOpenDynamicStreams()).WillRepeatedly(Return(false));
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  SendAckPacketToPeer();

  EXPECT_FALSE(connection_.GetPingAlarm()->IsSet());
}

// Tests whether sending an MTU discovery packet to peer successfully causes the
// maximum packet size to increase.
TEST_P(QuicConnectionTest, SendMtuDiscoveryPacket) {
  EXPECT_TRUE(connection_.connected());

  // Send an MTU probe.
  const size_t new_mtu = kDefaultMaxPacketSize + 100;
  QuicByteCount mtu_probe_size;
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .WillOnce(SaveArg<3>(&mtu_probe_size));
  connection_.SendMtuDiscoveryPacket(new_mtu);
  EXPECT_EQ(new_mtu, mtu_probe_size);
  EXPECT_EQ(1u, creator_->packet_number());

  // Send more than MTU worth of data.  No acknowledgement was received so far,
  // so the MTU should be at its old value.
  const QuicString data(kDefaultMaxPacketSize + 1, '.');
  QuicByteCount size_before_mtu_change;
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .Times(2)
      .WillOnce(SaveArg<3>(&size_before_mtu_change))
      .WillOnce(Return());
  connection_.SendStreamDataWithString(3, data, 0, FIN);
  EXPECT_EQ(3u, creator_->packet_number());
  EXPECT_EQ(kDefaultMaxPacketSize, size_before_mtu_change);

  // Acknowledge all packets so far.
  QuicAckFrame probe_ack = InitAckFrame(3);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  ProcessAckPacket(&probe_ack);
  EXPECT_EQ(new_mtu, connection_.max_packet_length());

  // Send the same data again.  Check that it fits into a single packet now.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  connection_.SendStreamDataWithString(3, data, 0, FIN);
  EXPECT_EQ(4u, creator_->packet_number());
}

// Tests whether MTU discovery does not happen when it is not explicitly enabled
// by the connection options.
TEST_P(QuicConnectionTest, MtuDiscoveryDisabled) {
  EXPECT_TRUE(connection_.connected());

  const QuicPacketCount packets_between_probes_base = 10;
  set_packets_between_probes_base(packets_between_probes_base);

  const QuicPacketCount number_of_packets = packets_between_probes_base * 2;
  for (QuicPacketCount i = 0; i < number_of_packets; i++) {
    SendStreamDataToPeer(3, ".", i, NO_FIN, nullptr);
    EXPECT_FALSE(connection_.GetMtuDiscoveryAlarm()->IsSet());
    EXPECT_EQ(0u, connection_.mtu_probe_count());
  }
}

// Tests whether MTU discovery works when the probe gets acknowledged on the
// first try.
TEST_P(QuicConnectionTest, MtuDiscoveryEnabled) {
  EXPECT_TRUE(connection_.connected());

  connection_.EnablePathMtuDiscovery(send_algorithm_);

  const QuicPacketCount packets_between_probes_base = 5;
  set_packets_between_probes_base(packets_between_probes_base);

  // Send enough packets so that the next one triggers path MTU discovery.
  for (QuicPacketCount i = 0; i < packets_between_probes_base - 1; i++) {
    SendStreamDataToPeer(3, ".", i, NO_FIN, nullptr);
    ASSERT_FALSE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  }

  // Trigger the probe.
  SendStreamDataToPeer(3, "!", packets_between_probes_base - 1, NO_FIN,
                       nullptr);
  ASSERT_TRUE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  QuicByteCount probe_size;
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .WillOnce(SaveArg<3>(&probe_size));
  connection_.GetMtuDiscoveryAlarm()->Fire();
  EXPECT_EQ(kMtuDiscoveryTargetPacketSizeHigh, probe_size);

  const QuicPacketCount probe_packet_number = packets_between_probes_base + 1;
  ASSERT_EQ(probe_packet_number, creator_->packet_number());

  // Acknowledge all packets sent so far.
  QuicAckFrame probe_ack = InitAckFrame(probe_packet_number);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  ProcessAckPacket(&probe_ack);
  EXPECT_EQ(kMtuDiscoveryTargetPacketSizeHigh, connection_.max_packet_length());
  EXPECT_EQ(0u, connection_.GetBytesInFlight());

  // Send more packets, and ensure that none of them sets the alarm.
  for (QuicPacketCount i = 0; i < 4 * packets_between_probes_base; i++) {
    SendStreamDataToPeer(3, ".", packets_between_probes_base + i, NO_FIN,
                         nullptr);
    ASSERT_FALSE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  }

  EXPECT_EQ(1u, connection_.mtu_probe_count());
}

// Tests whether MTU discovery works correctly when the probes never get
// acknowledged.
TEST_P(QuicConnectionTest, MtuDiscoveryFailed) {
  EXPECT_TRUE(connection_.connected());

  connection_.EnablePathMtuDiscovery(send_algorithm_);

  const QuicTime::Delta rtt = QuicTime::Delta::FromMilliseconds(100);

  EXPECT_EQ(kPacketsBetweenMtuProbesBase,
            QuicConnectionPeer::GetPacketsBetweenMtuProbes(&connection_));
  // Lower the number of probes between packets in order to make the test go
  // much faster.
  const QuicPacketCount packets_between_probes_base = 5;
  set_packets_between_probes_base(packets_between_probes_base);

  // This tests sends more packets than strictly necessary to make sure that if
  // the connection was to send more discovery packets than needed, those would
  // get caught as well.
  const QuicPacketCount number_of_packets =
      packets_between_probes_base * (1 << (kMtuDiscoveryAttempts + 1));
  std::vector<QuicPacketNumber> mtu_discovery_packets;
  // Called by the first ack.
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  // Called on many acks.
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _))
      .Times(AnyNumber());
  for (QuicPacketCount i = 0; i < number_of_packets; i++) {
    SendStreamDataToPeer(3, "!", i, NO_FIN, nullptr);
    clock_.AdvanceTime(rtt);

    // Receive an ACK, which marks all data packets as received, and all MTU
    // discovery packets as missing.

    QuicAckFrame ack;

    if (!mtu_discovery_packets.empty()) {
      QuicPacketNumber min_packet = *min_element(mtu_discovery_packets.begin(),
                                                 mtu_discovery_packets.end());
      QuicPacketNumber max_packet = *max_element(mtu_discovery_packets.begin(),
                                                 mtu_discovery_packets.end());
      ack.packets.AddRange(1, min_packet);
      ack.packets.AddRange(max_packet + 1, creator_->packet_number() + 1);
      ack.largest_acked = creator_->packet_number();

    } else {
      ack.packets.AddRange(1, creator_->packet_number() + 1);
      ack.largest_acked = creator_->packet_number();
    }

    ProcessAckPacket(&ack);

    // Trigger MTU probe if it would be scheduled now.
    if (!connection_.GetMtuDiscoveryAlarm()->IsSet()) {
      continue;
    }

    // Fire the alarm.  The alarm should cause a packet to be sent.
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
    connection_.GetMtuDiscoveryAlarm()->Fire();
    // Record the packet number of the MTU discovery packet in order to
    // mark it as NACK'd.
    mtu_discovery_packets.push_back(creator_->packet_number());
  }

  // Ensure the number of packets between probes grows exponentially by checking
  // it against the closed-form expression for the packet number.
  ASSERT_EQ(kMtuDiscoveryAttempts, mtu_discovery_packets.size());
  for (QuicPacketNumber i = 0; i < kMtuDiscoveryAttempts; i++) {
    // 2^0 + 2^1 + 2^2 + ... + 2^n = 2^(n + 1) - 1
    const QuicPacketCount packets_between_probes =
        packets_between_probes_base * ((1 << (i + 1)) - 1);
    EXPECT_EQ(packets_between_probes + (i + 1), mtu_discovery_packets[i]);
  }

  EXPECT_FALSE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  EXPECT_EQ(kDefaultMaxPacketSize, connection_.max_packet_length());
  EXPECT_EQ(kMtuDiscoveryAttempts, connection_.mtu_probe_count());
}

// Tests whether MTU discovery works when the writer has a limit on how large a
// packet can be.
TEST_P(QuicConnectionTest, MtuDiscoveryWriterLimited) {
  EXPECT_TRUE(connection_.connected());

  const QuicByteCount mtu_limit = kMtuDiscoveryTargetPacketSizeHigh - 1;
  writer_->set_max_packet_size(mtu_limit);
  connection_.EnablePathMtuDiscovery(send_algorithm_);

  const QuicPacketCount packets_between_probes_base = 5;
  set_packets_between_probes_base(packets_between_probes_base);

  // Send enough packets so that the next one triggers path MTU discovery.
  for (QuicPacketCount i = 0; i < packets_between_probes_base - 1; i++) {
    SendStreamDataToPeer(3, ".", i, NO_FIN, nullptr);
    ASSERT_FALSE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  }

  // Trigger the probe.
  SendStreamDataToPeer(3, "!", packets_between_probes_base - 1, NO_FIN,
                       nullptr);
  ASSERT_TRUE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  QuicByteCount probe_size;
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .WillOnce(SaveArg<3>(&probe_size));
  connection_.GetMtuDiscoveryAlarm()->Fire();
  EXPECT_EQ(mtu_limit, probe_size);

  const QuicPacketCount probe_sequence_number = packets_between_probes_base + 1;
  ASSERT_EQ(probe_sequence_number, creator_->packet_number());

  // Acknowledge all packets sent so far.
  QuicAckFrame probe_ack = InitAckFrame(probe_sequence_number);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  ProcessAckPacket(&probe_ack);
  EXPECT_EQ(mtu_limit, connection_.max_packet_length());
  EXPECT_EQ(0u, connection_.GetBytesInFlight());

  // Send more packets, and ensure that none of them sets the alarm.
  for (QuicPacketCount i = 0; i < 4 * packets_between_probes_base; i++) {
    SendStreamDataToPeer(3, ".", packets_between_probes_base + i, NO_FIN,
                         nullptr);
    ASSERT_FALSE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  }

  EXPECT_EQ(1u, connection_.mtu_probe_count());
}

// Tests whether MTU discovery works when the writer returns an error despite
// advertising higher packet length.
TEST_P(QuicConnectionTest, MtuDiscoveryWriterFailed) {
  EXPECT_TRUE(connection_.connected());

  const QuicByteCount mtu_limit = kMtuDiscoveryTargetPacketSizeHigh - 1;
  const QuicByteCount initial_mtu = connection_.max_packet_length();
  EXPECT_LT(initial_mtu, mtu_limit);
  writer_->set_max_packet_size(mtu_limit);
  connection_.EnablePathMtuDiscovery(send_algorithm_);

  const QuicPacketCount packets_between_probes_base = 5;
  set_packets_between_probes_base(packets_between_probes_base);

  // Send enough packets so that the next one triggers path MTU discovery.
  for (QuicPacketCount i = 0; i < packets_between_probes_base - 1; i++) {
    SendStreamDataToPeer(3, ".", i, NO_FIN, nullptr);
    ASSERT_FALSE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  }

  // Trigger the probe.
  SendStreamDataToPeer(3, "!", packets_between_probes_base - 1, NO_FIN,
                       nullptr);
  ASSERT_TRUE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  writer_->SimulateNextPacketTooLarge();
  connection_.GetMtuDiscoveryAlarm()->Fire();
  ASSERT_TRUE(connection_.connected());

  // Send more data.
  QuicPacketNumber probe_number = creator_->packet_number();
  QuicPacketCount extra_packets = packets_between_probes_base * 3;
  for (QuicPacketCount i = 0; i < extra_packets; i++) {
    connection_.EnsureWritableAndSendStreamData5();
    ASSERT_FALSE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  }

  // Acknowledge all packets sent so far, except for the lost probe.
  QuicAckFrame probe_ack =
      ConstructAckFrame(creator_->packet_number(), probe_number);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  ProcessAckPacket(&probe_ack);
  EXPECT_EQ(initial_mtu, connection_.max_packet_length());

  // Send more packets, and ensure that none of them sets the alarm.
  for (QuicPacketCount i = 0; i < 4 * packets_between_probes_base; i++) {
    connection_.EnsureWritableAndSendStreamData5();
    ASSERT_FALSE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  }

  EXPECT_EQ(initial_mtu, connection_.max_packet_length());
  EXPECT_EQ(1u, connection_.mtu_probe_count());
}

TEST_P(QuicConnectionTest, NoMtuDiscoveryAfterConnectionClosed) {
  EXPECT_TRUE(connection_.connected());

  connection_.EnablePathMtuDiscovery(send_algorithm_);

  const QuicPacketCount packets_between_probes_base = 10;
  set_packets_between_probes_base(packets_between_probes_base);

  // Send enough packets so that the next one triggers path MTU discovery.
  for (QuicPacketCount i = 0; i < packets_between_probes_base - 1; i++) {
    SendStreamDataToPeer(3, ".", i, NO_FIN, nullptr);
    ASSERT_FALSE(connection_.GetMtuDiscoveryAlarm()->IsSet());
  }

  SendStreamDataToPeer(3, "!", packets_between_probes_base - 1, NO_FIN,
                       nullptr);
  EXPECT_TRUE(connection_.GetMtuDiscoveryAlarm()->IsSet());

  EXPECT_CALL(visitor_, OnConnectionClosed(_, _, _));
  connection_.CloseConnection(QUIC_PEER_GOING_AWAY, "no reason",
                              ConnectionCloseBehavior::SILENT_CLOSE);
  EXPECT_FALSE(connection_.GetMtuDiscoveryAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, TimeoutAfterSend) {
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  QuicConfig config;
  connection_.SetFromConfig(config);
  EXPECT_FALSE(QuicConnectionPeer::IsSilentCloseEnabled(&connection_));

  const QuicTime::Delta initial_idle_timeout =
      QuicTime::Delta::FromSeconds(kInitialIdleTimeoutSecs - 1);
  const QuicTime::Delta five_ms = QuicTime::Delta::FromMilliseconds(5);
  QuicTime default_timeout = clock_.ApproximateNow() + initial_idle_timeout;

  // When we send a packet, the timeout will change to 5ms +
  // kInitialIdleTimeoutSecs.
  clock_.AdvanceTime(five_ms);
  SendStreamDataToPeer(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "foo",
      0, FIN, nullptr);
  EXPECT_EQ(default_timeout, connection_.GetTimeoutAlarm()->deadline());

  // Now send more data. This will not move the timeout because
  // no data has been received since the previous write.
  clock_.AdvanceTime(five_ms);
  SendStreamDataToPeer(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "foo",
      3, FIN, nullptr);
  EXPECT_EQ(default_timeout, connection_.GetTimeoutAlarm()->deadline());

  // The original alarm will fire.  We should not time out because we had a
  // network event at t=5ms.  The alarm will reregister.
  clock_.AdvanceTime(initial_idle_timeout - five_ms - five_ms);
  EXPECT_EQ(default_timeout, clock_.ApproximateNow());
  connection_.GetTimeoutAlarm()->Fire();
  EXPECT_TRUE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_TRUE(connection_.connected());
  EXPECT_EQ(default_timeout + five_ms,
            connection_.GetTimeoutAlarm()->deadline());

  // This time, we should time out.
  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_NETWORK_IDLE_TIMEOUT, _,
                                           ConnectionCloseSource::FROM_SELF));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
  clock_.AdvanceTime(five_ms);
  EXPECT_EQ(default_timeout + five_ms, clock_.ApproximateNow());
  connection_.GetTimeoutAlarm()->Fire();
  EXPECT_FALSE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_FALSE(connection_.connected());
}

TEST_P(QuicConnectionTest, TimeoutAfterRetransmission) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  QuicConfig config;
  connection_.SetFromConfig(config);
  EXPECT_FALSE(QuicConnectionPeer::IsSilentCloseEnabled(&connection_));

  const QuicTime start_time = clock_.Now();
  const QuicTime::Delta initial_idle_timeout =
      QuicTime::Delta::FromSeconds(kInitialIdleTimeoutSecs - 1);
  QuicTime default_timeout = clock_.Now() + initial_idle_timeout;

  connection_.SetMaxTailLossProbes(0);
  const QuicTime default_retransmission_time =
      start_time + DefaultRetransmissionTime();

  ASSERT_LT(default_retransmission_time, default_timeout);

  // When we send a packet, the timeout will change to 5 ms +
  // kInitialIdleTimeoutSecs (but it will not reschedule the alarm).
  const QuicTime::Delta five_ms = QuicTime::Delta::FromMilliseconds(5);
  const QuicTime send_time = start_time + five_ms;
  clock_.AdvanceTime(five_ms);
  ASSERT_EQ(send_time, clock_.Now());
  SendStreamDataToPeer(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "foo",
      0, FIN, nullptr);
  EXPECT_EQ(default_timeout, connection_.GetTimeoutAlarm()->deadline());

  // Move forward 5 ms and receive a packet, which will move the timeout
  // forward 5 ms more (but will not reschedule the alarm).
  const QuicTime receive_time = send_time + five_ms;
  clock_.AdvanceTime(receive_time - clock_.Now());
  ASSERT_EQ(receive_time, clock_.Now());
  ProcessPacket(1);

  // Now move forward to the retransmission time and retransmit the
  // packet, which should move the timeout forward again (but will not
  // reschedule the alarm).
  EXPECT_EQ(default_retransmission_time + five_ms,
            connection_.GetRetransmissionAlarm()->deadline());
  // Simulate the retransmission alarm firing.
  const QuicTime rto_time = send_time + DefaultRetransmissionTime();
  const QuicTime final_timeout = rto_time + initial_idle_timeout;
  clock_.AdvanceTime(rto_time - clock_.Now());
  ASSERT_EQ(rto_time, clock_.Now());
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, 2u, _, _));
  connection_.GetRetransmissionAlarm()->Fire();

  // Advance to the original timeout and fire the alarm. The connection should
  // timeout, and the alarm should be registered based on the time of the
  // retransmission.
  clock_.AdvanceTime(default_timeout - clock_.Now());
  ASSERT_EQ(default_timeout.ToDebuggingValue(),
            clock_.Now().ToDebuggingValue());
  EXPECT_EQ(default_timeout, clock_.Now());
  connection_.GetTimeoutAlarm()->Fire();
  EXPECT_TRUE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_TRUE(connection_.connected());
  ASSERT_EQ(final_timeout.ToDebuggingValue(),
            connection_.GetTimeoutAlarm()->deadline().ToDebuggingValue());

  // This time, we should time out.
  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_NETWORK_IDLE_TIMEOUT, _,
                                           ConnectionCloseSource::FROM_SELF));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
  clock_.AdvanceTime(final_timeout - clock_.Now());
  EXPECT_EQ(connection_.GetTimeoutAlarm()->deadline(), clock_.Now());
  EXPECT_EQ(final_timeout, clock_.Now());
  connection_.GetTimeoutAlarm()->Fire();
  EXPECT_FALSE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_FALSE(connection_.connected());
}

TEST_P(QuicConnectionTest, NewTimeoutAfterSendSilentClose) {
  // Same test as above, but complete a handshake which enables silent close,
  // causing no connection close packet to be sent.
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  QuicConfig config;

  // Create a handshake message that also enables silent close.
  CryptoHandshakeMessage msg;
  QuicString error_details;
  QuicConfig client_config;
  client_config.SetInitialStreamFlowControlWindowToSend(
      kInitialStreamFlowControlWindowForTest);
  client_config.SetInitialSessionFlowControlWindowToSend(
      kInitialSessionFlowControlWindowForTest);
  client_config.SetIdleNetworkTimeout(
      QuicTime::Delta::FromSeconds(kDefaultIdleTimeoutSecs),
      QuicTime::Delta::FromSeconds(kDefaultIdleTimeoutSecs));
  client_config.ToHandshakeMessage(&msg);
  const QuicErrorCode error =
      config.ProcessPeerHello(msg, CLIENT, &error_details);
  EXPECT_EQ(QUIC_NO_ERROR, error);

  connection_.SetFromConfig(config);
  EXPECT_TRUE(QuicConnectionPeer::IsSilentCloseEnabled(&connection_));

  const QuicTime::Delta default_idle_timeout =
      QuicTime::Delta::FromSeconds(kDefaultIdleTimeoutSecs - 1);
  const QuicTime::Delta five_ms = QuicTime::Delta::FromMilliseconds(5);
  QuicTime default_timeout = clock_.ApproximateNow() + default_idle_timeout;

  // When we send a packet, the timeout will change to 5ms +
  // kInitialIdleTimeoutSecs.
  clock_.AdvanceTime(five_ms);
  SendStreamDataToPeer(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "foo",
      0, FIN, nullptr);
  EXPECT_EQ(default_timeout, connection_.GetTimeoutAlarm()->deadline());

  // Now send more data. This will not move the timeout because
  // no data has been received since the previous write.
  clock_.AdvanceTime(five_ms);
  SendStreamDataToPeer(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "foo",
      3, FIN, nullptr);
  EXPECT_EQ(default_timeout, connection_.GetTimeoutAlarm()->deadline());

  // The original alarm will fire.  We should not time out because we had a
  // network event at t=5ms.  The alarm will reregister.
  clock_.AdvanceTime(default_idle_timeout - five_ms - five_ms);
  EXPECT_EQ(default_timeout, clock_.ApproximateNow());
  connection_.GetTimeoutAlarm()->Fire();
  EXPECT_TRUE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_TRUE(connection_.connected());
  EXPECT_EQ(default_timeout + five_ms,
            connection_.GetTimeoutAlarm()->deadline());

  // This time, we should time out.
  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_NETWORK_IDLE_TIMEOUT, _,
                                           ConnectionCloseSource::FROM_SELF));
  clock_.AdvanceTime(five_ms);
  EXPECT_EQ(default_timeout + five_ms, clock_.ApproximateNow());
  connection_.GetTimeoutAlarm()->Fire();
  EXPECT_FALSE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_FALSE(connection_.connected());
}

TEST_P(QuicConnectionTest, TimeoutAfterSendSilentCloseAndTLP) {
  // Same test as above, but complete a handshake which enables silent close,
  // but sending TLPs causes the connection close to be sent.
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  QuicConfig config;

  // Create a handshake message that also enables silent close.
  CryptoHandshakeMessage msg;
  QuicString error_details;
  QuicConfig client_config;
  client_config.SetInitialStreamFlowControlWindowToSend(
      kInitialStreamFlowControlWindowForTest);
  client_config.SetInitialSessionFlowControlWindowToSend(
      kInitialSessionFlowControlWindowForTest);
  client_config.SetIdleNetworkTimeout(
      QuicTime::Delta::FromSeconds(kDefaultIdleTimeoutSecs),
      QuicTime::Delta::FromSeconds(kDefaultIdleTimeoutSecs));
  client_config.ToHandshakeMessage(&msg);
  const QuicErrorCode error =
      config.ProcessPeerHello(msg, CLIENT, &error_details);
  EXPECT_EQ(QUIC_NO_ERROR, error);

  connection_.SetFromConfig(config);
  EXPECT_TRUE(QuicConnectionPeer::IsSilentCloseEnabled(&connection_));

  const QuicTime::Delta default_idle_timeout =
      QuicTime::Delta::FromSeconds(kDefaultIdleTimeoutSecs - 1);
  const QuicTime::Delta five_ms = QuicTime::Delta::FromMilliseconds(5);
  QuicTime default_timeout = clock_.ApproximateNow() + default_idle_timeout;

  // When we send a packet, the timeout will change to 5ms +
  // kInitialIdleTimeoutSecs.
  clock_.AdvanceTime(five_ms);
  SendStreamDataToPeer(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "foo",
      0, FIN, nullptr);
  EXPECT_EQ(default_timeout, connection_.GetTimeoutAlarm()->deadline());

  // Retransmit the packet via tail loss probe.
  clock_.AdvanceTime(connection_.GetRetransmissionAlarm()->deadline() -
                     clock_.Now());
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, 2u, _, _));
  connection_.GetRetransmissionAlarm()->Fire();

  // This time, we should time out and send a connection close due to the TLP.
  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_NETWORK_IDLE_TIMEOUT, _,
                                           ConnectionCloseSource::FROM_SELF));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
  clock_.AdvanceTime(connection_.GetTimeoutAlarm()->deadline() -
                     clock_.ApproximateNow() + five_ms);
  connection_.GetTimeoutAlarm()->Fire();
  EXPECT_FALSE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_FALSE(connection_.connected());
}

TEST_P(QuicConnectionTest, TimeoutAfterSendSilentCloseWithOpenStreams) {
  // Same test as above, but complete a handshake which enables silent close,
  // but having open streams causes the connection close to be sent.
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  QuicConfig config;

  // Create a handshake message that also enables silent close.
  CryptoHandshakeMessage msg;
  QuicString error_details;
  QuicConfig client_config;
  client_config.SetInitialStreamFlowControlWindowToSend(
      kInitialStreamFlowControlWindowForTest);
  client_config.SetInitialSessionFlowControlWindowToSend(
      kInitialSessionFlowControlWindowForTest);
  client_config.SetIdleNetworkTimeout(
      QuicTime::Delta::FromSeconds(kDefaultIdleTimeoutSecs),
      QuicTime::Delta::FromSeconds(kDefaultIdleTimeoutSecs));
  client_config.ToHandshakeMessage(&msg);
  const QuicErrorCode error =
      config.ProcessPeerHello(msg, CLIENT, &error_details);
  EXPECT_EQ(QUIC_NO_ERROR, error);

  connection_.SetFromConfig(config);
  EXPECT_TRUE(QuicConnectionPeer::IsSilentCloseEnabled(&connection_));

  const QuicTime::Delta default_idle_timeout =
      QuicTime::Delta::FromSeconds(kDefaultIdleTimeoutSecs - 1);
  const QuicTime::Delta five_ms = QuicTime::Delta::FromMilliseconds(5);
  QuicTime default_timeout = clock_.ApproximateNow() + default_idle_timeout;

  // When we send a packet, the timeout will change to 5ms +
  // kInitialIdleTimeoutSecs.
  clock_.AdvanceTime(five_ms);
  SendStreamDataToPeer(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "foo",
      0, FIN, nullptr);
  EXPECT_EQ(default_timeout, connection_.GetTimeoutAlarm()->deadline());

  // Indicate streams are still open.
  EXPECT_CALL(visitor_, HasOpenDynamicStreams()).WillRepeatedly(Return(true));

  // This time, we should time out and send a connection close due to the TLP.
  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_NETWORK_IDLE_TIMEOUT, _,
                                           ConnectionCloseSource::FROM_SELF));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
  clock_.AdvanceTime(connection_.GetTimeoutAlarm()->deadline() -
                     clock_.ApproximateNow() + five_ms);
  connection_.GetTimeoutAlarm()->Fire();
  EXPECT_FALSE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_FALSE(connection_.connected());
}

TEST_P(QuicConnectionTest, TimeoutAfterReceive) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  QuicConfig config;
  connection_.SetFromConfig(config);
  EXPECT_FALSE(QuicConnectionPeer::IsSilentCloseEnabled(&connection_));

  const QuicTime::Delta initial_idle_timeout =
      QuicTime::Delta::FromSeconds(kInitialIdleTimeoutSecs - 1);
  const QuicTime::Delta five_ms = QuicTime::Delta::FromMilliseconds(5);
  QuicTime default_timeout = clock_.ApproximateNow() + initial_idle_timeout;

  connection_.SendStreamDataWithString(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "foo",
      0, NO_FIN);
  connection_.SendStreamDataWithString(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "foo",
      3, NO_FIN);

  EXPECT_EQ(default_timeout, connection_.GetTimeoutAlarm()->deadline());
  clock_.AdvanceTime(five_ms);

  // When we receive a packet, the timeout will change to 5ms +
  // kInitialIdleTimeoutSecs.
  QuicAckFrame ack = InitAckFrame(2);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  ProcessAckPacket(&ack);

  // The original alarm will fire.  We should not time out because we had a
  // network event at t=5ms.  The alarm will reregister.
  clock_.AdvanceTime(initial_idle_timeout - five_ms);
  EXPECT_EQ(default_timeout, clock_.ApproximateNow());
  connection_.GetTimeoutAlarm()->Fire();
  EXPECT_TRUE(connection_.connected());
  EXPECT_TRUE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_EQ(default_timeout + five_ms,
            connection_.GetTimeoutAlarm()->deadline());

  // This time, we should time out.
  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_NETWORK_IDLE_TIMEOUT, _,
                                           ConnectionCloseSource::FROM_SELF));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
  clock_.AdvanceTime(five_ms);
  EXPECT_EQ(default_timeout + five_ms, clock_.ApproximateNow());
  connection_.GetTimeoutAlarm()->Fire();
  EXPECT_FALSE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_FALSE(connection_.connected());
}

TEST_P(QuicConnectionTest, TimeoutAfterReceiveNotSendWhenUnacked) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  QuicConfig config;
  connection_.SetFromConfig(config);
  EXPECT_FALSE(QuicConnectionPeer::IsSilentCloseEnabled(&connection_));

  const QuicTime::Delta initial_idle_timeout =
      QuicTime::Delta::FromSeconds(kInitialIdleTimeoutSecs - 1);
  connection_.SetNetworkTimeouts(
      QuicTime::Delta::Infinite(),
      initial_idle_timeout + QuicTime::Delta::FromSeconds(1));
  const QuicTime::Delta five_ms = QuicTime::Delta::FromMilliseconds(5);
  QuicTime default_timeout = clock_.ApproximateNow() + initial_idle_timeout;

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
  connection_.SendStreamDataWithString(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "foo",
      0, NO_FIN);
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
  connection_.SendStreamDataWithString(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "foo",
      3, NO_FIN);

  EXPECT_EQ(default_timeout, connection_.GetTimeoutAlarm()->deadline());

  clock_.AdvanceTime(five_ms);

  // When we receive a packet, the timeout will change to 5ms +
  // kInitialIdleTimeoutSecs.
  QuicAckFrame ack = InitAckFrame(2);
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  ProcessAckPacket(&ack);

  // The original alarm will fire.  We should not time out because we had a
  // network event at t=5ms.  The alarm will reregister.
  clock_.AdvanceTime(initial_idle_timeout - five_ms);
  EXPECT_EQ(default_timeout, clock_.ApproximateNow());
  connection_.GetTimeoutAlarm()->Fire();
  EXPECT_TRUE(connection_.connected());
  EXPECT_TRUE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_EQ(default_timeout + five_ms,
            connection_.GetTimeoutAlarm()->deadline());

  // Now, send packets while advancing the time and verify that the connection
  // eventually times out.
  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_NETWORK_IDLE_TIMEOUT, _,
                                           ConnectionCloseSource::FROM_SELF));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(AnyNumber());
  for (int i = 0; i < 100 && connection_.connected(); ++i) {
    QUIC_LOG(INFO) << "sending data packet";
    connection_.SendStreamDataWithString(
        GetNthClientInitiatedStreamId(1, connection_.transport_version()),
        "foo", 0, NO_FIN);
    connection_.GetTimeoutAlarm()->Fire();
    clock_.AdvanceTime(QuicTime::Delta::FromSeconds(1));
  }
  EXPECT_FALSE(connection_.connected());
  EXPECT_FALSE(connection_.GetTimeoutAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, TimeoutAfter5ClientRTOs) {
  connection_.SetMaxTailLossProbes(2);
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  QuicConfig config;
  QuicTagVector connection_options;
  connection_options.push_back(k5RTO);
  config.SetConnectionOptionsToSend(connection_options);
  connection_.SetFromConfig(config);

  // Send stream data.
  SendStreamDataToPeer(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "foo",
      0, FIN, nullptr);

  // Fire the retransmission alarm 6 times, twice for TLP and 4 times for RTO.
  for (int i = 0; i < 6; ++i) {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
    connection_.GetRetransmissionAlarm()->Fire();
    EXPECT_TRUE(connection_.GetTimeoutAlarm()->IsSet());
    EXPECT_TRUE(connection_.connected());
  }

  EXPECT_EQ(2u, connection_.sent_packet_manager().GetConsecutiveTlpCount());
  EXPECT_EQ(4u, connection_.sent_packet_manager().GetConsecutiveRtoCount());
  // This time, we should time out.
  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_TOO_MANY_RTOS, _,
                                           ConnectionCloseSource::FROM_SELF));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
  connection_.GetRetransmissionAlarm()->Fire();
  EXPECT_FALSE(connection_.GetTimeoutAlarm()->IsSet());
  EXPECT_FALSE(connection_.connected());
}

TEST_P(QuicConnectionTest, SendScheduler) {
  // Test that if we send a packet without delay, it is not queued.
  QuicFramerPeer::SetPerspective(&peer_framer_, Perspective::IS_CLIENT);
  std::unique_ptr<QuicPacket> packet = ConstructDataPacket(1, !kHasStopWaiting);
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
  connection_.SendPacket(ENCRYPTION_NONE, 1, std::move(packet),
                         HAS_RETRANSMITTABLE_DATA, false, false);
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
}

TEST_P(QuicConnectionTest, FailToSendFirstPacket) {
  // Test that the connection does not crash when it fails to send the first
  // packet at which point self_address_ might be uninitialized.
  QuicFramerPeer::SetPerspective(&peer_framer_, Perspective::IS_CLIENT);
  EXPECT_CALL(visitor_, OnConnectionClosed(_, _, _)).Times(1);
  std::unique_ptr<QuicPacket> packet = ConstructDataPacket(1, !kHasStopWaiting);
  writer_->SetShouldWriteFail();
  connection_.SendPacket(ENCRYPTION_NONE, 1, std::move(packet),
                         HAS_RETRANSMITTABLE_DATA, false, false);
}

TEST_P(QuicConnectionTest, SendSchedulerEAGAIN) {
  QuicFramerPeer::SetPerspective(&peer_framer_, Perspective::IS_CLIENT);
  std::unique_ptr<QuicPacket> packet = ConstructDataPacket(1, !kHasStopWaiting);
  BlockOnNextWrite();
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, 1, _, _)).Times(0);
  connection_.SendPacket(ENCRYPTION_NONE, 1, std::move(packet),
                         HAS_RETRANSMITTABLE_DATA, false, false);
  EXPECT_EQ(1u, connection_.NumQueuedPackets());
}

TEST_P(QuicConnectionTest, TestQueueLimitsOnSendStreamData) {
  // All packets carry version info till version is negotiated.
  size_t payload_length;
  size_t length = GetPacketLengthForOneStream(
      connection_.version().transport_version, kIncludeVersion,
      !kIncludeDiversificationNonce, PACKET_8BYTE_CONNECTION_ID,
      PACKET_0BYTE_CONNECTION_ID,
      QuicPacketCreatorPeer::GetPacketNumberLength(creator_), &payload_length);
  connection_.SetMaxPacketLength(length);

  // Queue the first packet.
  EXPECT_CALL(*send_algorithm_, CanSend(_)).WillOnce(testing::Return(false));
  const QuicString payload(payload_length, 'a');
  EXPECT_EQ(0u, connection_.SendStreamDataWithString(3, payload, 0, NO_FIN)
                    .bytes_consumed);
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
}

TEST_P(QuicConnectionTest, LoopThroughSendingPackets) {
  // All packets carry version info till version is negotiated.
  size_t payload_length;

  // Number of packets this test generates. The goal is to have
  // kPacketCount packets, each the same size (overhead and payload).
  // The payload will vary depending on the overhead (which in turn
  // varies per the QUIC packet encoding rules).
  const int kPacketCount = 7;

  // Get the basic packet size. This assumes, among other things, a
  // stream offset of 0.
  size_t length = GetPacketLengthForOneStream(
      connection_.version().transport_version, kIncludeVersion,
      !kIncludeDiversificationNonce, PACKET_8BYTE_CONNECTION_ID,
      PACKET_0BYTE_CONNECTION_ID,
      QuicPacketCreatorPeer::GetPacketNumberLength(creator_), &payload_length);
  // GetPacketLengthForOneStream() assumes a stream offset of 0 in determining
  // packet length. The size of the offset field in a stream frame is
  // 0 for offset 0, and 2 for non-zero offsets up through 16K (for
  // versions other than 99) and 1 for non-zero offsets through 16K
  // for version 99. Increase the length by 1 or 2, as apporpriate, so
  // that subsequent packets containing subsequent stream frames with
  // non-zero offsets will fit within the packet length.
  if (connection_.version().transport_version == QUIC_VERSION_99) {
    length = length + 1;
  } else {
    length = length + 2;
  }

  connection_.SetMaxPacketLength(length);

  // Queue the first packet.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
      .Times(kPacketCount);

  size_t total_payload_length = payload_length * kPacketCount;
  // The first frame of the stream is at offset 0. When the offset is
  // 0, it is not included in the stream frame. Increase the total
  // payload so that the "missing" offset byte in the first packet is
  // occupied by a payload byte. The net result is that each of the N
  // packets of the test will contain a single stream frame, each of
  // which will be the same size (overhead + data).
  if (connection_.version().transport_version == QUIC_VERSION_99) {
    // Version 99 encodes the offset in 1 byte for the scope of this test.
    total_payload_length = total_payload_length + 1;
  } else {
    // Versions other than 99 encode the offset in 2 bytes for the
    // scope of this test.
    total_payload_length = total_payload_length + 2;
  }
  const QuicString payload(total_payload_length, 'a');

  EXPECT_EQ(payload.size(),
            connection_
                .SendStreamDataWithString(QuicUtils::GetCryptoStreamId(
                                              connection_.transport_version()),
                                          payload, 0, NO_FIN)
                .bytes_consumed);
}

TEST_P(QuicConnectionTest, LoopThroughSendingPacketsWithTruncation) {
  set_perspective(Perspective::IS_SERVER);
  if (GetParam().version.transport_version <= QUIC_VERSION_43) {
    // For IETF QUIC, encryption level will be switched to FORWARD_SECURE in
    // SendStreamDataWithString.
    QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);
  }
  // Set up a larger payload than will fit in one packet.
  const QuicString payload(connection_.max_packet_length(), 'a');
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _)).Times(AnyNumber());

  // Now send some packets with no truncation.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(2);
  EXPECT_EQ(payload.size(),
            connection_.SendStreamDataWithString(3, payload, 0, NO_FIN)
                .bytes_consumed);
  // Track the size of the second packet here.  The overhead will be the largest
  // we see in this test, due to the non-truncated connection id.
  size_t non_truncated_packet_size = writer_->last_packet_size();

  // Change to a 0 byte connection id.
  QuicConfig config;
  QuicConfigPeer::SetReceivedBytesForConnectionId(&config, 0);
  connection_.SetFromConfig(config);
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(2);
  EXPECT_EQ(payload.size(),
            connection_.SendStreamDataWithString(3, payload, 1350, NO_FIN)
                .bytes_consumed);
  if (connection_.transport_version() > QUIC_VERSION_43) {
    // Short header packets sent from server omit connection ID already, and
    // stream offset size increases from 0 to 2.
    EXPECT_EQ(non_truncated_packet_size, writer_->last_packet_size() - 2);
  } else {
    // Just like above, we save 8 bytes on payload, and 8 on truncation. -2
    // because stream offset size is 2 instead of 0.
    EXPECT_EQ(non_truncated_packet_size,
              writer_->last_packet_size() + 8 * 2 - 2);
  }
}

TEST_P(QuicConnectionTest, SendDelayedAck) {
  QuicTime ack_time = clock_.ApproximateNow() + DefaultDelayedAckTime();
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
  const uint8_t tag = 0x07;
  connection_.SetDecrypter(ENCRYPTION_INITIAL,
                           QuicMakeUnique<StrictTaggingDecrypter>(tag));
  peer_framer_.SetEncrypter(ENCRYPTION_INITIAL,
                            QuicMakeUnique<TaggingEncrypter>(tag));
  // Process a packet from the non-crypto stream.
  frame1_.stream_id = 3;

  // The same as ProcessPacket(1) except that ENCRYPTION_INITIAL is used
  // instead of ENCRYPTION_NONE.
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(1, !kHasStopWaiting, ENCRYPTION_INITIAL);

  // Check if delayed ack timer is running for the expected interval.
  EXPECT_TRUE(connection_.GetAckAlarm()->IsSet());
  EXPECT_EQ(ack_time, connection_.GetAckAlarm()->deadline());
  // Simulate delayed ack alarm firing.
  connection_.GetAckAlarm()->Fire();
  // Check that ack is sent and that delayed ack alarm is reset.
  if (GetParam().no_stop_waiting) {
    EXPECT_EQ(1u, writer_->frame_count());
    EXPECT_TRUE(writer_->stop_waiting_frames().empty());
  } else {
    EXPECT_EQ(2u, writer_->frame_count());
    EXPECT_FALSE(writer_->stop_waiting_frames().empty());
  }
  EXPECT_FALSE(writer_->ack_frames().empty());
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, SendDelayedAfterQuiescence) {
  QuicConnectionPeer::SetFastAckAfterQuiescence(&connection_, true);

  // The beginning of the connection counts as quiescence.
  QuicTime ack_time =
      clock_.ApproximateNow() + QuicTime::Delta::FromMilliseconds(1);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
  const uint8_t tag = 0x07;
  connection_.SetDecrypter(ENCRYPTION_INITIAL,
                           QuicMakeUnique<StrictTaggingDecrypter>(tag));
  peer_framer_.SetEncrypter(ENCRYPTION_INITIAL,
                            QuicMakeUnique<TaggingEncrypter>(tag));
  // Process a packet from the non-crypto stream.
  frame1_.stream_id = 3;

  // The same as ProcessPacket(1) except that ENCRYPTION_INITIAL is used
  // instead of ENCRYPTION_NONE.
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(1, !kHasStopWaiting, ENCRYPTION_INITIAL);

  // Check if delayed ack timer is running for the expected interval.
  EXPECT_TRUE(connection_.GetAckAlarm()->IsSet());
  EXPECT_EQ(ack_time, connection_.GetAckAlarm()->deadline());
  // Simulate delayed ack alarm firing.
  connection_.GetAckAlarm()->Fire();
  // Check that ack is sent and that delayed ack alarm is reset.
  if (GetParam().no_stop_waiting) {
    EXPECT_EQ(1u, writer_->frame_count());
    EXPECT_TRUE(writer_->stop_waiting_frames().empty());
  } else {
    EXPECT_EQ(2u, writer_->frame_count());
    EXPECT_FALSE(writer_->stop_waiting_frames().empty());
  }
  EXPECT_FALSE(writer_->ack_frames().empty());
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());

  // Process another packet immedately after sending the ack and expect the
  // ack alarm to be set delayed ack time in the future.
  ack_time = clock_.ApproximateNow() + DefaultDelayedAckTime();
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(2, !kHasStopWaiting, ENCRYPTION_INITIAL);

  // Check if delayed ack timer is running for the expected interval.
  EXPECT_TRUE(connection_.GetAckAlarm()->IsSet());
  EXPECT_EQ(ack_time, connection_.GetAckAlarm()->deadline());
  // Simulate delayed ack alarm firing.
  connection_.GetAckAlarm()->Fire();
  // Check that ack is sent and that delayed ack alarm is reset.
  if (GetParam().no_stop_waiting) {
    EXPECT_EQ(1u, writer_->frame_count());
    EXPECT_TRUE(writer_->stop_waiting_frames().empty());
  } else {
    EXPECT_EQ(2u, writer_->frame_count());
    EXPECT_FALSE(writer_->stop_waiting_frames().empty());
  }
  EXPECT_FALSE(writer_->ack_frames().empty());
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());

  // Wait 1 second and enesure the ack alarm is set to 1ms in the future.
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(1));
  ack_time = clock_.ApproximateNow() + QuicTime::Delta::FromMilliseconds(1);
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(3, !kHasStopWaiting, ENCRYPTION_INITIAL);

  // Check if delayed ack timer is running for the expected interval.
  EXPECT_TRUE(connection_.GetAckAlarm()->IsSet());
  EXPECT_EQ(ack_time, connection_.GetAckAlarm()->deadline());
}

TEST_P(QuicConnectionTest, SendDelayedAckDecimation) {
  EXPECT_CALL(visitor_, OnAckNeedsRetransmittableFrame()).Times(AnyNumber());
  QuicConnectionPeer::SetAckMode(&connection_, QuicConnection::ACK_DECIMATION);

  const size_t kMinRttMs = 40;
  RttStats* rtt_stats = const_cast<RttStats*>(manager_->GetRttStats());
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(kMinRttMs),
                       QuicTime::Delta::Zero(), QuicTime::Zero());
  // The ack time should be based on min_rtt/4, since it's less than the
  // default delayed ack time.
  QuicTime ack_time = clock_.ApproximateNow() +
                      QuicTime::Delta::FromMilliseconds(kMinRttMs / 4);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
  const uint8_t tag = 0x07;
  connection_.SetDecrypter(ENCRYPTION_INITIAL,
                           QuicMakeUnique<StrictTaggingDecrypter>(tag));
  peer_framer_.SetEncrypter(ENCRYPTION_INITIAL,
                            QuicMakeUnique<TaggingEncrypter>(tag));
  // Process a packet from the non-crypto stream.
  frame1_.stream_id = 3;

  // Process all the initial packets in order so there aren't missing packets.
  QuicPacketNumber kFirstDecimatedPacket = 101;
  for (unsigned int i = 0; i < kFirstDecimatedPacket - 1; ++i) {
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
    ProcessDataPacketAtLevel(1 + i, !kHasStopWaiting, ENCRYPTION_INITIAL);
  }
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
  // The same as ProcessPacket(1) except that ENCRYPTION_INITIAL is used
  // instead of ENCRYPTION_NONE.
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(kFirstDecimatedPacket, !kHasStopWaiting,
                           ENCRYPTION_INITIAL);

  // Check if delayed ack timer is running for the expected interval.
  EXPECT_TRUE(connection_.GetAckAlarm()->IsSet());
  EXPECT_EQ(ack_time, connection_.GetAckAlarm()->deadline());

  // The 10th received packet causes an ack to be sent.
  for (int i = 0; i < 9; ++i) {
    EXPECT_TRUE(connection_.GetAckAlarm()->IsSet());
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
    ProcessDataPacketAtLevel(kFirstDecimatedPacket + 1 + i, !kHasStopWaiting,
                             ENCRYPTION_INITIAL);
  }
  // Check that ack is sent and that delayed ack alarm is reset.
  if (GetParam().no_stop_waiting) {
    EXPECT_EQ(1u, writer_->frame_count());
    EXPECT_TRUE(writer_->stop_waiting_frames().empty());
  } else {
    EXPECT_EQ(2u, writer_->frame_count());
    EXPECT_FALSE(writer_->stop_waiting_frames().empty());
  }
  EXPECT_FALSE(writer_->ack_frames().empty());
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, SendDelayedAckAckDecimationAfterQuiescence) {
  EXPECT_CALL(visitor_, OnAckNeedsRetransmittableFrame()).Times(AnyNumber());
  QuicConnectionPeer::SetAckMode(&connection_, QuicConnection::ACK_DECIMATION);
  QuicConnectionPeer::SetFastAckAfterQuiescence(&connection_, true);

  const size_t kMinRttMs = 40;
  RttStats* rtt_stats = const_cast<RttStats*>(manager_->GetRttStats());
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(kMinRttMs),
                       QuicTime::Delta::Zero(), QuicTime::Zero());

  // The beginning of the connection counts as quiescence.
  QuicTime ack_time =
      clock_.ApproximateNow() + QuicTime::Delta::FromMilliseconds(1);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
  const uint8_t tag = 0x07;
  connection_.SetDecrypter(ENCRYPTION_INITIAL,
                           QuicMakeUnique<StrictTaggingDecrypter>(tag));
  peer_framer_.SetEncrypter(ENCRYPTION_INITIAL,
                            QuicMakeUnique<TaggingEncrypter>(tag));
  // Process a packet from the non-crypto stream.
  frame1_.stream_id = 3;

  // The same as ProcessPacket(1) except that ENCRYPTION_INITIAL is used
  // instead of ENCRYPTION_NONE.
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(1, !kHasStopWaiting, ENCRYPTION_INITIAL);

  // Check if delayed ack timer is running for the expected interval.
  EXPECT_TRUE(connection_.GetAckAlarm()->IsSet());
  EXPECT_EQ(ack_time, connection_.GetAckAlarm()->deadline());
  // Simulate delayed ack alarm firing.
  connection_.GetAckAlarm()->Fire();
  // Check that ack is sent and that delayed ack alarm is reset.
  if (GetParam().no_stop_waiting) {
    EXPECT_EQ(1u, writer_->frame_count());
    EXPECT_TRUE(writer_->stop_waiting_frames().empty());
  } else {
    EXPECT_EQ(2u, writer_->frame_count());
    EXPECT_FALSE(writer_->stop_waiting_frames().empty());
  }
  EXPECT_FALSE(writer_->ack_frames().empty());
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());

  // Process another packet immedately after sending the ack and expect the
  // ack alarm to be set delayed ack time in the future.
  ack_time = clock_.ApproximateNow() + DefaultDelayedAckTime();
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(2, !kHasStopWaiting, ENCRYPTION_INITIAL);

  // Check if delayed ack timer is running for the expected interval.
  EXPECT_TRUE(connection_.GetAckAlarm()->IsSet());
  EXPECT_EQ(ack_time, connection_.GetAckAlarm()->deadline());
  // Simulate delayed ack alarm firing.
  connection_.GetAckAlarm()->Fire();
  // Check that ack is sent and that delayed ack alarm is reset.
  if (GetParam().no_stop_waiting) {
    EXPECT_EQ(1u, writer_->frame_count());
    EXPECT_TRUE(writer_->stop_waiting_frames().empty());
  } else {
    EXPECT_EQ(2u, writer_->frame_count());
    EXPECT_FALSE(writer_->stop_waiting_frames().empty());
  }
  EXPECT_FALSE(writer_->ack_frames().empty());
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());

  // Wait 1 second and enesure the ack alarm is set to 1ms in the future.
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(1));
  ack_time = clock_.ApproximateNow() + QuicTime::Delta::FromMilliseconds(1);
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(3, !kHasStopWaiting, ENCRYPTION_INITIAL);

  // Check if delayed ack timer is running for the expected interval.
  EXPECT_TRUE(connection_.GetAckAlarm()->IsSet());
  EXPECT_EQ(ack_time, connection_.GetAckAlarm()->deadline());

  // Process enough packets to get into ack decimation behavior.
  // The ack time should be based on min_rtt/4, since it's less than the
  // default delayed ack time.
  ack_time = clock_.ApproximateNow() +
             QuicTime::Delta::FromMilliseconds(kMinRttMs / 4);
  QuicPacketNumber kFirstDecimatedPacket = 101;
  for (unsigned int i = 0; i < kFirstDecimatedPacket - 4; ++i) {
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
    ProcessDataPacketAtLevel(4 + i, !kHasStopWaiting, ENCRYPTION_INITIAL);
  }
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
  // The same as ProcessPacket(1) except that ENCRYPTION_INITIAL is used
  // instead of ENCRYPTION_NONE.
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(kFirstDecimatedPacket, !kHasStopWaiting,
                           ENCRYPTION_INITIAL);

  // Check if delayed ack timer is running for the expected interval.
  EXPECT_TRUE(connection_.GetAckAlarm()->IsSet());
  EXPECT_EQ(ack_time, connection_.GetAckAlarm()->deadline());

  // The 10th received packet causes an ack to be sent.
  for (int i = 0; i < 9; ++i) {
    EXPECT_TRUE(connection_.GetAckAlarm()->IsSet());
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
    ProcessDataPacketAtLevel(kFirstDecimatedPacket + 1 + i, !kHasStopWaiting,
                             ENCRYPTION_INITIAL);
  }
  // Check that ack is sent and that delayed ack alarm is reset.
  if (GetParam().no_stop_waiting) {
    EXPECT_EQ(1u, writer_->frame_count());
    EXPECT_TRUE(writer_->stop_waiting_frames().empty());
  } else {
    EXPECT_EQ(2u, writer_->frame_count());
    EXPECT_FALSE(writer_->stop_waiting_frames().empty());
  }
  EXPECT_FALSE(writer_->ack_frames().empty());
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());

  // Wait 1 second and enesure the ack alarm is set to 1ms in the future.
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(1));
  ack_time = clock_.ApproximateNow() + QuicTime::Delta::FromMilliseconds(1);
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(kFirstDecimatedPacket + 10, !kHasStopWaiting,
                           ENCRYPTION_INITIAL);

  // Check if delayed ack timer is running for the expected interval.
  EXPECT_TRUE(connection_.GetAckAlarm()->IsSet());
  EXPECT_EQ(ack_time, connection_.GetAckAlarm()->deadline());
}

TEST_P(QuicConnectionTest, SendDelayedAckDecimationUnlimitedAggregation) {
  EXPECT_CALL(visitor_, OnAckNeedsRetransmittableFrame()).Times(AnyNumber());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  QuicConfig config;
  QuicTagVector connection_options;
  connection_options.push_back(kACKD);
  // No limit on the number of packets received before sending an ack.
  connection_options.push_back(kAKDU);
  config.SetConnectionOptionsToSend(connection_options);
  connection_.SetFromConfig(config);

  const size_t kMinRttMs = 40;
  RttStats* rtt_stats = const_cast<RttStats*>(manager_->GetRttStats());
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(kMinRttMs),
                       QuicTime::Delta::Zero(), QuicTime::Zero());
  // The ack time should be based on min_rtt/4, since it's less than the
  // default delayed ack time.
  QuicTime ack_time = clock_.ApproximateNow() +
                      QuicTime::Delta::FromMilliseconds(kMinRttMs / 4);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
  const uint8_t tag = 0x07;
  connection_.SetDecrypter(ENCRYPTION_INITIAL,
                           QuicMakeUnique<StrictTaggingDecrypter>(tag));
  peer_framer_.SetEncrypter(ENCRYPTION_INITIAL,
                            QuicMakeUnique<TaggingEncrypter>(tag));
  // Process a packet from the non-crypto stream.
  frame1_.stream_id = 3;

  // Process all the initial packets in order so there aren't missing packets.
  QuicPacketNumber kFirstDecimatedPacket = 101;
  for (unsigned int i = 0; i < kFirstDecimatedPacket - 1; ++i) {
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
    ProcessDataPacketAtLevel(1 + i, !kHasStopWaiting, ENCRYPTION_INITIAL);
  }
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
  // The same as ProcessPacket(1) except that ENCRYPTION_INITIAL is used
  // instead of ENCRYPTION_NONE.
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(kFirstDecimatedPacket, !kHasStopWaiting,
                           ENCRYPTION_INITIAL);

  // Check if delayed ack timer is running for the expected interval.
  EXPECT_TRUE(connection_.GetAckAlarm()->IsSet());
  EXPECT_EQ(ack_time, connection_.GetAckAlarm()->deadline());

  // 18 packets will not cause an ack to be sent.  19 will because when
  // stop waiting frames are in use, we ack every 20 packets no matter what.
  for (int i = 0; i < 18; ++i) {
    EXPECT_TRUE(connection_.GetAckAlarm()->IsSet());
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
    ProcessDataPacketAtLevel(kFirstDecimatedPacket + 1 + i, !kHasStopWaiting,
                             ENCRYPTION_INITIAL);
  }
  // The delayed ack timer should still be set to the expected deadline.
  EXPECT_TRUE(connection_.GetAckAlarm()->IsSet());
  EXPECT_EQ(ack_time, connection_.GetAckAlarm()->deadline());
}

TEST_P(QuicConnectionTest, SendDelayedAckDecimationEighthRtt) {
  EXPECT_CALL(visitor_, OnAckNeedsRetransmittableFrame()).Times(AnyNumber());
  QuicConnectionPeer::SetAckMode(&connection_, QuicConnection::ACK_DECIMATION);
  QuicConnectionPeer::SetAckDecimationDelay(&connection_, 0.125);

  const size_t kMinRttMs = 40;
  RttStats* rtt_stats = const_cast<RttStats*>(manager_->GetRttStats());
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(kMinRttMs),
                       QuicTime::Delta::Zero(), QuicTime::Zero());
  // The ack time should be based on min_rtt/8, since it's less than the
  // default delayed ack time.
  QuicTime ack_time = clock_.ApproximateNow() +
                      QuicTime::Delta::FromMilliseconds(kMinRttMs / 8);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
  const uint8_t tag = 0x07;
  connection_.SetDecrypter(ENCRYPTION_INITIAL,
                           QuicMakeUnique<StrictTaggingDecrypter>(tag));
  peer_framer_.SetEncrypter(ENCRYPTION_INITIAL,
                            QuicMakeUnique<TaggingEncrypter>(tag));
  // Process a packet from the non-crypto stream.
  frame1_.stream_id = 3;

  // Process all the initial packets in order so there aren't missing packets.
  QuicPacketNumber kFirstDecimatedPacket = 101;
  for (unsigned int i = 0; i < kFirstDecimatedPacket - 1; ++i) {
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
    ProcessDataPacketAtLevel(1 + i, !kHasStopWaiting, ENCRYPTION_INITIAL);
  }
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
  // The same as ProcessPacket(1) except that ENCRYPTION_INITIAL is used
  // instead of ENCRYPTION_NONE.
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(kFirstDecimatedPacket, !kHasStopWaiting,
                           ENCRYPTION_INITIAL);

  // Check if delayed ack timer is running for the expected interval.
  EXPECT_TRUE(connection_.GetAckAlarm()->IsSet());
  EXPECT_EQ(ack_time, connection_.GetAckAlarm()->deadline());

  // The 10th received packet causes an ack to be sent.
  for (int i = 0; i < 9; ++i) {
    EXPECT_TRUE(connection_.GetAckAlarm()->IsSet());
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
    ProcessDataPacketAtLevel(kFirstDecimatedPacket + 1 + i, !kHasStopWaiting,
                             ENCRYPTION_INITIAL);
  }
  // Check that ack is sent and that delayed ack alarm is reset.
  if (GetParam().no_stop_waiting) {
    EXPECT_EQ(1u, writer_->frame_count());
    EXPECT_TRUE(writer_->stop_waiting_frames().empty());
  } else {
    EXPECT_EQ(2u, writer_->frame_count());
    EXPECT_FALSE(writer_->stop_waiting_frames().empty());
  }
  EXPECT_FALSE(writer_->ack_frames().empty());
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, SendDelayedAckDecimationWithReordering) {
  if (GetQuicReloadableFlag(quic_enable_ack_decimation)) {
    return;
  }
  EXPECT_CALL(visitor_, OnAckNeedsRetransmittableFrame()).Times(AnyNumber());
  QuicConnectionPeer::SetAckMode(
      &connection_, QuicConnection::ACK_DECIMATION_WITH_REORDERING);

  const size_t kMinRttMs = 40;
  RttStats* rtt_stats = const_cast<RttStats*>(manager_->GetRttStats());
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(kMinRttMs),
                       QuicTime::Delta::Zero(), QuicTime::Zero());
  // The ack time should be based on min_rtt/4, since it's less than the
  // default delayed ack time.
  QuicTime ack_time = clock_.ApproximateNow() +
                      QuicTime::Delta::FromMilliseconds(kMinRttMs / 4);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
  const uint8_t tag = 0x07;
  connection_.SetDecrypter(ENCRYPTION_INITIAL,
                           QuicMakeUnique<StrictTaggingDecrypter>(tag));
  peer_framer_.SetEncrypter(ENCRYPTION_INITIAL,
                            QuicMakeUnique<TaggingEncrypter>(tag));
  // Process a packet from the non-crypto stream.
  frame1_.stream_id = 3;

  // Process all the initial packets in order so there aren't missing packets.
  QuicPacketNumber kFirstDecimatedPacket = 101;
  for (unsigned int i = 0; i < kFirstDecimatedPacket - 1; ++i) {
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
    ProcessDataPacketAtLevel(1 + i, !kHasStopWaiting, ENCRYPTION_INITIAL);
  }
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());

  // Receive one packet out of order and then the rest in order.
  // The loop leaves a one packet gap between acks sent to simulate some loss.
  for (int j = 0; j < 3; ++j) {
    // Process packet 10 first and ensure the alarm is one eighth min_rtt.
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
    ProcessDataPacketAtLevel(kFirstDecimatedPacket + 9 + (j * 11),
                             !kHasStopWaiting, ENCRYPTION_INITIAL);
    ack_time = clock_.ApproximateNow() + QuicTime::Delta::FromMilliseconds(5);
    EXPECT_TRUE(connection_.GetAckAlarm()->IsSet());
    EXPECT_EQ(ack_time, connection_.GetAckAlarm()->deadline());

    // The 10th received packet causes an ack to be sent.
    writer_->Reset();
    for (int i = 0; i < 9; ++i) {
      EXPECT_TRUE(connection_.GetAckAlarm()->IsSet());
      EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
      // The ACK shouldn't be sent until the 10th packet is processed.
      EXPECT_TRUE(writer_->ack_frames().empty());
      ProcessDataPacketAtLevel(kFirstDecimatedPacket + i + (j * 11),
                               !kHasStopWaiting, ENCRYPTION_INITIAL);
    }
    // Check that ack is sent and that delayed ack alarm is reset.
    if (GetParam().no_stop_waiting) {
      EXPECT_EQ(1u, writer_->frame_count());
      EXPECT_TRUE(writer_->stop_waiting_frames().empty());
    } else {
      EXPECT_EQ(2u, writer_->frame_count());
      EXPECT_FALSE(writer_->stop_waiting_frames().empty());
    }
    EXPECT_FALSE(writer_->ack_frames().empty());
    EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
  }
}

TEST_P(QuicConnectionTest, SendDelayedAckDecimationWithLargeReordering) {
  if (GetQuicReloadableFlag(quic_enable_ack_decimation)) {
    return;
  }
  EXPECT_CALL(visitor_, OnAckNeedsRetransmittableFrame()).Times(AnyNumber());
  QuicConnectionPeer::SetAckMode(
      &connection_, QuicConnection::ACK_DECIMATION_WITH_REORDERING);

  const size_t kMinRttMs = 40;
  RttStats* rtt_stats = const_cast<RttStats*>(manager_->GetRttStats());
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(kMinRttMs),
                       QuicTime::Delta::Zero(), QuicTime::Zero());
  // The ack time should be based on min_rtt/4, since it's less than the
  // default delayed ack time.
  QuicTime ack_time = clock_.ApproximateNow() +
                      QuicTime::Delta::FromMilliseconds(kMinRttMs / 4);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
  const uint8_t tag = 0x07;
  connection_.SetDecrypter(ENCRYPTION_INITIAL,
                           QuicMakeUnique<StrictTaggingDecrypter>(tag));
  peer_framer_.SetEncrypter(ENCRYPTION_INITIAL,
                            QuicMakeUnique<TaggingEncrypter>(tag));
  // Process a packet from the non-crypto stream.
  frame1_.stream_id = 3;

  // Process all the initial packets in order so there aren't missing packets.
  QuicPacketNumber kFirstDecimatedPacket = 101;
  for (unsigned int i = 0; i < kFirstDecimatedPacket - 1; ++i) {
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
    ProcessDataPacketAtLevel(1 + i, !kHasStopWaiting, ENCRYPTION_INITIAL);
  }
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
  // The same as ProcessPacket(1) except that ENCRYPTION_INITIAL is used
  // instead of ENCRYPTION_NONE.
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(kFirstDecimatedPacket, !kHasStopWaiting,
                           ENCRYPTION_INITIAL);

  // Check if delayed ack timer is running for the expected interval.
  EXPECT_TRUE(connection_.GetAckAlarm()->IsSet());
  EXPECT_EQ(ack_time, connection_.GetAckAlarm()->deadline());

  // Process packet 10 first and ensure the alarm is one eighth min_rtt.
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(kFirstDecimatedPacket + 19, !kHasStopWaiting,
                           ENCRYPTION_INITIAL);
  ack_time = clock_.ApproximateNow() + QuicTime::Delta::FromMilliseconds(5);
  EXPECT_TRUE(connection_.GetAckAlarm()->IsSet());
  EXPECT_EQ(ack_time, connection_.GetAckAlarm()->deadline());

  // The 10th received packet causes an ack to be sent.
  for (int i = 0; i < 8; ++i) {
    EXPECT_TRUE(connection_.GetAckAlarm()->IsSet());
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
    ProcessDataPacketAtLevel(kFirstDecimatedPacket + 1 + i, !kHasStopWaiting,
                             ENCRYPTION_INITIAL);
  }
  // Check that ack is sent and that delayed ack alarm is reset.
  if (GetParam().no_stop_waiting) {
    EXPECT_EQ(1u, writer_->frame_count());
    EXPECT_TRUE(writer_->stop_waiting_frames().empty());
  } else {
    EXPECT_EQ(2u, writer_->frame_count());
    EXPECT_FALSE(writer_->stop_waiting_frames().empty());
  }
  EXPECT_FALSE(writer_->ack_frames().empty());
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());

  // The next packet received in order will cause an immediate ack,
  // because it fills a hole.
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(kFirstDecimatedPacket + 10, !kHasStopWaiting,
                           ENCRYPTION_INITIAL);
  // Check that ack is sent and that delayed ack alarm is reset.
  if (GetParam().no_stop_waiting) {
    EXPECT_EQ(1u, writer_->frame_count());
    EXPECT_TRUE(writer_->stop_waiting_frames().empty());
  } else {
    EXPECT_EQ(2u, writer_->frame_count());
    EXPECT_FALSE(writer_->stop_waiting_frames().empty());
  }
  EXPECT_FALSE(writer_->ack_frames().empty());
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, SendDelayedAckDecimationWithReorderingEighthRtt) {
  if (GetQuicReloadableFlag(quic_enable_ack_decimation)) {
    return;
  }
  EXPECT_CALL(visitor_, OnAckNeedsRetransmittableFrame()).Times(AnyNumber());
  QuicConnectionPeer::SetAckMode(
      &connection_, QuicConnection::ACK_DECIMATION_WITH_REORDERING);
  QuicConnectionPeer::SetAckDecimationDelay(&connection_, 0.125);

  const size_t kMinRttMs = 40;
  RttStats* rtt_stats = const_cast<RttStats*>(manager_->GetRttStats());
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(kMinRttMs),
                       QuicTime::Delta::Zero(), QuicTime::Zero());
  // The ack time should be based on min_rtt/8, since it's less than the
  // default delayed ack time.
  QuicTime ack_time = clock_.ApproximateNow() +
                      QuicTime::Delta::FromMilliseconds(kMinRttMs / 8);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
  const uint8_t tag = 0x07;
  connection_.SetDecrypter(ENCRYPTION_INITIAL,
                           QuicMakeUnique<StrictTaggingDecrypter>(tag));
  peer_framer_.SetEncrypter(ENCRYPTION_INITIAL,
                            QuicMakeUnique<TaggingEncrypter>(tag));
  // Process a packet from the non-crypto stream.
  frame1_.stream_id = 3;

  // Process all the initial packets in order so there aren't missing packets.
  QuicPacketNumber kFirstDecimatedPacket = 101;
  for (unsigned int i = 0; i < kFirstDecimatedPacket - 1; ++i) {
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
    ProcessDataPacketAtLevel(1 + i, !kHasStopWaiting, ENCRYPTION_INITIAL);
  }
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
  // The same as ProcessPacket(1) except that ENCRYPTION_INITIAL is used
  // instead of ENCRYPTION_NONE.
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(kFirstDecimatedPacket, !kHasStopWaiting,
                           ENCRYPTION_INITIAL);

  // Check if delayed ack timer is running for the expected interval.
  EXPECT_TRUE(connection_.GetAckAlarm()->IsSet());
  EXPECT_EQ(ack_time, connection_.GetAckAlarm()->deadline());

  // Process packet 10 first and ensure the alarm is one eighth min_rtt.
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(kFirstDecimatedPacket + 9, !kHasStopWaiting,
                           ENCRYPTION_INITIAL);
  ack_time = clock_.ApproximateNow() + QuicTime::Delta::FromMilliseconds(5);
  EXPECT_TRUE(connection_.GetAckAlarm()->IsSet());
  EXPECT_EQ(ack_time, connection_.GetAckAlarm()->deadline());

  // The 10th received packet causes an ack to be sent.
  for (int i = 0; i < 8; ++i) {
    EXPECT_TRUE(connection_.GetAckAlarm()->IsSet());
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
    ProcessDataPacketAtLevel(kFirstDecimatedPacket + 1 + i, !kHasStopWaiting,
                             ENCRYPTION_INITIAL);
  }
  // Check that ack is sent and that delayed ack alarm is reset.
  if (GetParam().no_stop_waiting) {
    EXPECT_EQ(1u, writer_->frame_count());
    EXPECT_TRUE(writer_->stop_waiting_frames().empty());
  } else {
    EXPECT_EQ(2u, writer_->frame_count());
    EXPECT_FALSE(writer_->stop_waiting_frames().empty());
  }
  EXPECT_FALSE(writer_->ack_frames().empty());
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
}

TEST_P(QuicConnectionTest,
       SendDelayedAckDecimationWithLargeReorderingEighthRtt) {
  if (GetQuicReloadableFlag(quic_enable_ack_decimation)) {
    return;
  }
  EXPECT_CALL(visitor_, OnAckNeedsRetransmittableFrame()).Times(AnyNumber());
  QuicConnectionPeer::SetAckMode(
      &connection_, QuicConnection::ACK_DECIMATION_WITH_REORDERING);
  QuicConnectionPeer::SetAckDecimationDelay(&connection_, 0.125);

  const size_t kMinRttMs = 40;
  RttStats* rtt_stats = const_cast<RttStats*>(manager_->GetRttStats());
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(kMinRttMs),
                       QuicTime::Delta::Zero(), QuicTime::Zero());
  // The ack time should be based on min_rtt/8, since it's less than the
  // default delayed ack time.
  QuicTime ack_time = clock_.ApproximateNow() +
                      QuicTime::Delta::FromMilliseconds(kMinRttMs / 8);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
  const uint8_t tag = 0x07;
  connection_.SetDecrypter(ENCRYPTION_INITIAL,
                           QuicMakeUnique<StrictTaggingDecrypter>(tag));
  peer_framer_.SetEncrypter(ENCRYPTION_INITIAL,
                            QuicMakeUnique<TaggingEncrypter>(tag));
  // Process a packet from the non-crypto stream.
  frame1_.stream_id = 3;

  // Process all the initial packets in order so there aren't missing packets.
  QuicPacketNumber kFirstDecimatedPacket = 101;
  for (unsigned int i = 0; i < kFirstDecimatedPacket - 1; ++i) {
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
    ProcessDataPacketAtLevel(1 + i, !kHasStopWaiting, ENCRYPTION_INITIAL);
  }
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
  // The same as ProcessPacket(1) except that ENCRYPTION_INITIAL is used
  // instead of ENCRYPTION_NONE.
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(kFirstDecimatedPacket, !kHasStopWaiting,
                           ENCRYPTION_INITIAL);

  // Check if delayed ack timer is running for the expected interval.
  EXPECT_TRUE(connection_.GetAckAlarm()->IsSet());
  EXPECT_EQ(ack_time, connection_.GetAckAlarm()->deadline());

  // Process packet 10 first and ensure the alarm is one eighth min_rtt.
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(kFirstDecimatedPacket + 19, !kHasStopWaiting,
                           ENCRYPTION_INITIAL);
  ack_time = clock_.ApproximateNow() + QuicTime::Delta::FromMilliseconds(5);
  EXPECT_TRUE(connection_.GetAckAlarm()->IsSet());
  EXPECT_EQ(ack_time, connection_.GetAckAlarm()->deadline());

  // The 10th received packet causes an ack to be sent.
  for (int i = 0; i < 8; ++i) {
    EXPECT_TRUE(connection_.GetAckAlarm()->IsSet());
    EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
    ProcessDataPacketAtLevel(kFirstDecimatedPacket + 1 + i, !kHasStopWaiting,
                             ENCRYPTION_INITIAL);
  }
  // Check that ack is sent and that delayed ack alarm is reset.
  if (GetParam().no_stop_waiting) {
    EXPECT_EQ(1u, writer_->frame_count());
    EXPECT_TRUE(writer_->stop_waiting_frames().empty());
  } else {
    EXPECT_EQ(2u, writer_->frame_count());
    EXPECT_FALSE(writer_->stop_waiting_frames().empty());
  }
  EXPECT_FALSE(writer_->ack_frames().empty());
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());

  // The next packet received in order will cause an immediate ack,
  // because it fills a hole.
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  ProcessDataPacketAtLevel(kFirstDecimatedPacket + 10, !kHasStopWaiting,
                           ENCRYPTION_INITIAL);
  // Check that ack is sent and that delayed ack alarm is reset.
  if (GetParam().no_stop_waiting) {
    EXPECT_EQ(1u, writer_->frame_count());
    EXPECT_TRUE(writer_->stop_waiting_frames().empty());
  } else {
    EXPECT_EQ(2u, writer_->frame_count());
    EXPECT_FALSE(writer_->stop_waiting_frames().empty());
  }
  EXPECT_FALSE(writer_->ack_frames().empty());
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, SendDelayedAckOnHandshakeConfirmed) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  ProcessPacket(1);
  // Check that ack is sent and that delayed ack alarm is set.
  EXPECT_TRUE(connection_.GetAckAlarm()->IsSet());
  QuicTime ack_time = clock_.ApproximateNow() + DefaultDelayedAckTime();
  EXPECT_EQ(ack_time, connection_.GetAckAlarm()->deadline());

  // Completing the handshake as the server does nothing.
  QuicConnectionPeer::SetPerspective(&connection_, Perspective::IS_SERVER);
  connection_.OnHandshakeComplete();
  EXPECT_TRUE(connection_.GetAckAlarm()->IsSet());
  EXPECT_EQ(ack_time, connection_.GetAckAlarm()->deadline());

  // Complete the handshake as the client decreases the delayed ack time to 0ms.
  QuicConnectionPeer::SetPerspective(&connection_, Perspective::IS_CLIENT);
  connection_.OnHandshakeComplete();
  EXPECT_TRUE(connection_.GetAckAlarm()->IsSet());
  EXPECT_EQ(clock_.ApproximateNow(), connection_.GetAckAlarm()->deadline());
}

TEST_P(QuicConnectionTest, SendDelayedAckOnSecondPacket) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  ProcessPacket(1);
  ProcessPacket(2);
  // Check that ack is sent and that delayed ack alarm is reset.
  if (GetParam().no_stop_waiting) {
    EXPECT_EQ(1u, writer_->frame_count());
    EXPECT_TRUE(writer_->stop_waiting_frames().empty());
  } else {
    EXPECT_EQ(2u, writer_->frame_count());
    EXPECT_FALSE(writer_->stop_waiting_frames().empty());
  }
  EXPECT_FALSE(writer_->ack_frames().empty());
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, NoAckOnOldNacks) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  // Drop one packet, triggering a sequence of acks.
  ProcessPacket(2);
  size_t frames_per_ack = GetParam().no_stop_waiting ? 1 : 2;
  EXPECT_EQ(frames_per_ack, writer_->frame_count());
  EXPECT_FALSE(writer_->ack_frames().empty());
  writer_->Reset();
  ProcessPacket(3);
  EXPECT_EQ(frames_per_ack, writer_->frame_count());
  EXPECT_FALSE(writer_->ack_frames().empty());
  writer_->Reset();
  ProcessPacket(4);
  EXPECT_EQ(frames_per_ack, writer_->frame_count());
  EXPECT_FALSE(writer_->ack_frames().empty());
  writer_->Reset();
  ProcessPacket(5);
  EXPECT_EQ(frames_per_ack, writer_->frame_count());
  EXPECT_FALSE(writer_->ack_frames().empty());
  writer_->Reset();
  // Now only set the timer on the 6th packet, instead of sending another ack.
  ProcessPacket(6);
  EXPECT_EQ(0u, writer_->frame_count());
  EXPECT_TRUE(connection_.GetAckAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, SendDelayedAckOnOutgoingPacket) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  ProcessPacket(1);
  connection_.SendStreamDataWithString(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "foo",
      0, NO_FIN);
  // Check that ack is bundled with outgoing data and that delayed ack
  // alarm is reset.
  if (GetParam().no_stop_waiting) {
    EXPECT_EQ(2u, writer_->frame_count());
    EXPECT_TRUE(writer_->stop_waiting_frames().empty());
  } else {
    EXPECT_EQ(3u, writer_->frame_count());
    EXPECT_FALSE(writer_->stop_waiting_frames().empty());
  }
  EXPECT_FALSE(writer_->ack_frames().empty());
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, SendDelayedAckOnOutgoingCryptoPacket) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  ProcessPacket(1);
  connection_.SendStreamDataWithString(
      QuicUtils::GetCryptoStreamId(connection_.transport_version()), "foo", 0,
      NO_FIN);
  // Check that ack is bundled with outgoing crypto data.
  if (GetParam().no_stop_waiting) {
    EXPECT_EQ(3u, writer_->frame_count());
    EXPECT_TRUE(writer_->stop_waiting_frames().empty());
  } else {
    EXPECT_EQ(4u, writer_->frame_count());
    EXPECT_FALSE(writer_->stop_waiting_frames().empty());
  }
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, BlockAndBufferOnFirstCHLOPacketOfTwo) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  ProcessPacket(1);
  BlockOnNextWrite();
  writer_->set_is_write_blocked_data_buffered(true);
  connection_.SendStreamDataWithString(
      QuicUtils::GetCryptoStreamId(connection_.transport_version()), "foo", 0,
      NO_FIN);
  EXPECT_TRUE(writer_->IsWriteBlocked());
  EXPECT_FALSE(connection_.HasQueuedData());
  connection_.SendStreamDataWithString(
      QuicUtils::GetCryptoStreamId(connection_.transport_version()), "bar", 3,
      NO_FIN);
  EXPECT_TRUE(writer_->IsWriteBlocked());
  EXPECT_TRUE(connection_.HasQueuedData());
}

TEST_P(QuicConnectionTest, BundleAckForSecondCHLO) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
  EXPECT_CALL(visitor_, OnCanWrite())
      .WillOnce(IgnoreResult(InvokeWithoutArgs(
          &connection_, &TestConnection::SendCryptoStreamData)));
  // Process a packet from the crypto stream, which is frame1_'s default.
  // Receiving the CHLO as packet 2 first will cause the connection to
  // immediately send an ack, due to the packet gap.
  ProcessPacket(2);
  // Check that ack is sent and that delayed ack alarm is reset.
  if (GetParam().no_stop_waiting) {
    EXPECT_EQ(3u, writer_->frame_count());
    EXPECT_TRUE(writer_->stop_waiting_frames().empty());
  } else {
    EXPECT_EQ(4u, writer_->frame_count());
    EXPECT_FALSE(writer_->stop_waiting_frames().empty());
  }
  EXPECT_EQ(1u, writer_->stream_frames().size());
  EXPECT_EQ(1u, writer_->padding_frames().size());
  ASSERT_FALSE(writer_->ack_frames().empty());
  EXPECT_EQ(2u, LargestAcked(writer_->ack_frames().front()));
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, BundleAckForSecondCHLOTwoPacketReject) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());

  // Process two packets from the crypto stream, which is frame1_'s default,
  // simulating a 2 packet reject.
  {
    ProcessPacket(1);
    // Send the new CHLO when the REJ is processed.
    EXPECT_CALL(visitor_, OnStreamFrame(_))
        .WillOnce(IgnoreResult(InvokeWithoutArgs(
            &connection_, &TestConnection::SendCryptoStreamData)));
    ProcessDataPacket(2);
  }
  // Check that ack is sent and that delayed ack alarm is reset.
  if (GetParam().no_stop_waiting) {
    EXPECT_EQ(3u, writer_->frame_count());
    EXPECT_TRUE(writer_->stop_waiting_frames().empty());
  } else {
    EXPECT_EQ(4u, writer_->frame_count());
    EXPECT_FALSE(writer_->stop_waiting_frames().empty());
  }
  EXPECT_EQ(1u, writer_->stream_frames().size());
  EXPECT_EQ(1u, writer_->padding_frames().size());
  ASSERT_FALSE(writer_->ack_frames().empty());
  EXPECT_EQ(2u, LargestAcked(writer_->ack_frames().front()));
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, BundleAckWithDataOnIncomingAck) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  connection_.SendStreamDataWithString(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "foo",
      0, NO_FIN);
  connection_.SendStreamDataWithString(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "foo",
      3, NO_FIN);
  // Ack the second packet, which will retransmit the first packet.
  QuicAckFrame ack = ConstructAckFrame(2, 1);
  LostPacketVector lost_packets;
  lost_packets.push_back(LostPacket(1, kMaxPacketSize));
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _))
      .WillOnce(SetArgPointee<5>(lost_packets));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  ProcessAckPacket(&ack);
  EXPECT_EQ(1u, writer_->frame_count());
  EXPECT_EQ(1u, writer_->stream_frames().size());
  writer_->Reset();

  // Now ack the retransmission, which will both raise the high water mark
  // and see if there is more data to send.
  ack = ConstructAckFrame(3, 1);
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  ProcessAckPacket(&ack);

  // Check that no packet is sent and the ack alarm isn't set.
  EXPECT_EQ(0u, writer_->frame_count());
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
  writer_->Reset();

  // Send the same ack, but send both data and an ack together.
  ack = ConstructAckFrame(3, 1);
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _));
  EXPECT_CALL(visitor_, OnCanWrite())
      .WillOnce(IgnoreResult(InvokeWithoutArgs(
          &connection_, &TestConnection::EnsureWritableAndSendStreamData5)));
  ProcessAckPacket(&ack);

  // Check that ack is bundled with outgoing data and the delayed ack
  // alarm is reset.
  if (GetParam().no_stop_waiting) {
    EXPECT_EQ(2u, writer_->frame_count());
    EXPECT_TRUE(writer_->stop_waiting_frames().empty());
  } else {
    EXPECT_EQ(3u, writer_->frame_count());
    EXPECT_FALSE(writer_->stop_waiting_frames().empty());
  }
  EXPECT_FALSE(writer_->ack_frames().empty());
  EXPECT_EQ(3u, LargestAcked(writer_->ack_frames().front()));
  EXPECT_EQ(1u, writer_->stream_frames().size());
  EXPECT_FALSE(connection_.GetAckAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, NoAckSentForClose) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  ProcessPacket(1);
  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_PEER_GOING_AWAY, _,
                                           ConnectionCloseSource::FROM_PEER));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  ProcessClosePacket(2);
}

TEST_P(QuicConnectionTest, SendWhenDisconnected) {
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_PEER_GOING_AWAY, _,
                                           ConnectionCloseSource::FROM_SELF));
  connection_.CloseConnection(QUIC_PEER_GOING_AWAY, "no reason",
                              ConnectionCloseBehavior::SILENT_CLOSE);
  EXPECT_FALSE(connection_.connected());
  EXPECT_FALSE(connection_.CanWriteStreamData());
  std::unique_ptr<QuicPacket> packet = ConstructDataPacket(1, !kHasStopWaiting);
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, 1, _, _)).Times(0);
  connection_.SendPacket(ENCRYPTION_NONE, 1, std::move(packet),
                         HAS_RETRANSMITTABLE_DATA, false, false);
}

TEST_P(QuicConnectionTest, SendConnectivityProbingWhenDisconnected) {
  // EXPECT_QUIC_BUG tests are expensive so only run one instance of them.
  if (!IsDefaultTestConfiguration()) {
    return;
  }

  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_PEER_GOING_AWAY, _,
                                           ConnectionCloseSource::FROM_SELF));
  connection_.CloseConnection(QUIC_PEER_GOING_AWAY, "no reason",
                              ConnectionCloseBehavior::SILENT_CLOSE);
  EXPECT_FALSE(connection_.connected());
  EXPECT_FALSE(connection_.CanWriteStreamData());

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, 1, _, _)).Times(0);

  EXPECT_QUIC_BUG(connection_.SendConnectivityProbingPacket(
                      writer_.get(), connection_.peer_address()),
                  "Not sending connectivity probing packet as connection is "
                  "disconnected.");
}

TEST_P(QuicConnectionTest, WriteBlockedAfterClientSendsConnectivityProbe) {
  EXPECT_EQ(Perspective::IS_CLIENT, connection_.perspective());
  TestPacketWriter probing_writer(version(), &clock_);
  // Block next write so that sending connectivity probe will encounter a
  // blocked write when send a connectivity probe to the peer.
  probing_writer.BlockOnNextWrite();
  // Connection will not be marked as write blocked as connectivity probe only
  // affects the probing_writer which is not the default.
  EXPECT_CALL(visitor_, OnWriteBlocked()).Times(0);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, 1, _, _)).Times(1);
  connection_.SendConnectivityProbingPacket(&probing_writer,
                                            connection_.peer_address());
}

TEST_P(QuicConnectionTest, WriterBlockedAfterServerSendsConnectivityProbe) {
  set_perspective(Perspective::IS_SERVER);
  QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);

  // Block next write so that sending connectivity probe will encounter a
  // blocked write when send a connectivity probe to the peer.
  writer_->BlockOnNextWrite();
  // Connection will be marked as write blocked as server uses the default
  // writer to send connectivity probes.
  EXPECT_CALL(visitor_, OnWriteBlocked()).Times(1);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, 1, _, _)).Times(1);
  connection_.SendConnectivityProbingPacket(writer_.get(),
                                            connection_.peer_address());
}

TEST_P(QuicConnectionTest, WriterErrorWhenClientSendsConnectivityProbe) {
  EXPECT_EQ(Perspective::IS_CLIENT, connection_.perspective());
  TestPacketWriter probing_writer(version(), &clock_);
  probing_writer.SetShouldWriteFail();

  // Connection should not be closed if a connectivity probe is failed to be
  // sent.
  EXPECT_CALL(visitor_, OnConnectionClosed(_, _, _)).Times(0);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, 1, _, _)).Times(0);
  connection_.SendConnectivityProbingPacket(&probing_writer,
                                            connection_.peer_address());
}

TEST_P(QuicConnectionTest, WriterErrorWhenServerSendsConnectivityProbe) {
  set_perspective(Perspective::IS_SERVER);
  QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);

  writer_->SetShouldWriteFail();
  // Connection should not be closed if a connectivity probe is failed to be
  // sent.
  EXPECT_CALL(visitor_, OnConnectionClosed(_, _, _)).Times(0);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, 1, _, _)).Times(0);
  connection_.SendConnectivityProbingPacket(writer_.get(),
                                            connection_.peer_address());
}

TEST_P(QuicConnectionTest, PublicReset) {
  if (GetParam().version.transport_version > QUIC_VERSION_43) {
    return;
  }
  QuicPublicResetPacket header;
  // Public reset packet in only built by server.
  header.connection_id = connection_id_;
  std::unique_ptr<QuicEncryptedPacket> packet(
      framer_.BuildPublicResetPacket(header));
  std::unique_ptr<QuicReceivedPacket> received(
      ConstructReceivedPacket(*packet, QuicTime::Zero()));
  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_PUBLIC_RESET, _,
                                           ConnectionCloseSource::FROM_PEER));
  connection_.ProcessUdpPacket(kSelfAddress, kPeerAddress, *received);
}

TEST_P(QuicConnectionTest, IetfStatelessReset) {
  if (GetParam().version.transport_version <= QUIC_VERSION_43) {
    return;
  }
  const QuicUint128 kTestStatelessResetToken = 1010101;
  QuicConfig config;
  QuicConfigPeer::SetReceivedStatelessResetToken(&config,
                                                 kTestStatelessResetToken);
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  connection_.SetFromConfig(config);
  std::unique_ptr<QuicEncryptedPacket> packet(
      QuicFramer::BuildIetfStatelessResetPacket(connection_id_,
                                                kTestStatelessResetToken));
  std::unique_ptr<QuicReceivedPacket> received(
      ConstructReceivedPacket(*packet, QuicTime::Zero()));
  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_PUBLIC_RESET, _,
                                           ConnectionCloseSource::FROM_PEER));
  connection_.ProcessUdpPacket(kSelfAddress, kPeerAddress, *received);
}

TEST_P(QuicConnectionTest, GoAway) {
  if (GetParam().version.transport_version == QUIC_VERSION_99) {
    // GoAway is not available in version 99.
    return;
  }

  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  QuicGoAwayFrame goaway;
  goaway.last_good_stream_id = 1;
  goaway.error_code = QUIC_PEER_GOING_AWAY;
  goaway.reason_phrase = "Going away.";
  EXPECT_CALL(visitor_, OnGoAway(_));
  ProcessGoAwayPacket(&goaway);
}

TEST_P(QuicConnectionTest, WindowUpdate) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  QuicWindowUpdateFrame window_update;
  window_update.stream_id = 3;
  window_update.byte_offset = 1234;
  EXPECT_CALL(visitor_, OnWindowUpdateFrame(_));
  ProcessFramePacket(QuicFrame(&window_update));
}

TEST_P(QuicConnectionTest, Blocked) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  QuicBlockedFrame blocked;
  blocked.stream_id = 3;
  EXPECT_CALL(visitor_, OnBlockedFrame(_));
  ProcessFramePacket(QuicFrame(&blocked));
  EXPECT_EQ(1u, connection_.GetStats().blocked_frames_received);
  EXPECT_EQ(0u, connection_.GetStats().blocked_frames_sent);
}

TEST_P(QuicConnectionTest, ZeroBytePacket) {
  // Don't close the connection for zero byte packets.
  EXPECT_CALL(visitor_, OnConnectionClosed(_, _, _)).Times(0);
  QuicReceivedPacket encrypted(nullptr, 0, QuicTime::Zero());
  connection_.ProcessUdpPacket(kSelfAddress, kPeerAddress, encrypted);
}

TEST_P(QuicConnectionTest, MissingPacketsBeforeLeastUnacked) {
  if (GetParam().version.transport_version > QUIC_VERSION_43) {
    return;
  }
  // Set the packet number of the ack packet to be least unacked (4).
  QuicPacketCreatorPeer::SetPacketNumber(&peer_creator_, 3);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  QuicStopWaitingFrame frame = InitStopWaitingFrame(4);
  ProcessStopWaitingPacket(&frame);
  EXPECT_FALSE(outgoing_ack()->packets.Empty());
}

TEST_P(QuicConnectionTest, ServerSendsVersionNegotiationPacket) {
  // Turn off QUIC_VERSION_99.
  SetQuicFlag(&FLAGS_quic_enable_version_99, false);
  connection_.SetSupportedVersions(CurrentSupportedVersions());
  set_perspective(Perspective::IS_SERVER);
  if (GetParam().version.transport_version > QUIC_VERSION_43) {
    peer_framer_.set_version_for_tests(
        ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_99));
  } else {
    peer_framer_.set_version_for_tests(
        ParsedQuicVersion(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED));
  }

  QuicPacketHeader header;
  header.destination_connection_id = connection_id_;
  header.version_flag = true;
  header.packet_number = 12;

  QuicFrames frames;
  frames.push_back(QuicFrame(frame1_));
  std::unique_ptr<QuicPacket> packet(ConstructPacket(header, frames));
  char buffer[kMaxPacketSize];
  size_t encrypted_length = framer_.EncryptPayload(ENCRYPTION_NONE, 12, *packet,
                                                   buffer, kMaxPacketSize);

  framer_.set_version(version());
  // Writer's framer's perspective is client, so that it needs to have the right
  // version to process either IETF or GQUIC version negotiation packet.
  writer_->SetSupportedVersions({version()});
  connection_.ProcessUdpPacket(
      kSelfAddress, kPeerAddress,
      QuicReceivedPacket(buffer, encrypted_length, QuicTime::Zero(), false));
  EXPECT_TRUE(writer_->version_negotiation_packet() != nullptr);

  ParsedQuicVersionVector supported_versions = CurrentSupportedVersions();
  ASSERT_EQ(supported_versions.size(),
            writer_->version_negotiation_packet()->versions.size());

  // We expect all versions in supported_versions to be
  // included in the packet.
  for (size_t i = 0; i < supported_versions.size(); ++i) {
    EXPECT_EQ(supported_versions[i],
              writer_->version_negotiation_packet()->versions[i]);
  }
}

TEST_P(QuicConnectionTest, ServerSendsVersionNegotiationPacketSocketBlocked) {
  // Turn off QUIC_VERSION_99.
  SetQuicFlag(&FLAGS_quic_enable_version_99, false);
  connection_.SetSupportedVersions(CurrentSupportedVersions());
  set_perspective(Perspective::IS_SERVER);
  if (GetParam().version.transport_version > QUIC_VERSION_43) {
    peer_framer_.set_version_for_tests(
        ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_99));
  } else {
    peer_framer_.set_version_for_tests(
        ParsedQuicVersion(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED));
  }

  QuicPacketHeader header;
  header.destination_connection_id = connection_id_;
  header.version_flag = true;
  header.packet_number = 12;

  QuicFrames frames;
  frames.push_back(QuicFrame(frame1_));
  std::unique_ptr<QuicPacket> packet(ConstructPacket(header, frames));
  char buffer[kMaxPacketSize];
  size_t encrypted_length = framer_.EncryptPayload(ENCRYPTION_NONE, 12, *packet,
                                                   buffer, kMaxPacketSize);

  framer_.set_version(version());
  BlockOnNextWrite();
  // Writer's framer's perspective is client, so that it needs to have the right
  // version to process either IETF or GQUIC version negotiation packet.
  writer_->SetSupportedVersions({version()});
  connection_.ProcessUdpPacket(
      kSelfAddress, kPeerAddress,
      QuicReceivedPacket(buffer, encrypted_length, QuicTime::Zero(), false));
  EXPECT_EQ(0u, writer_->last_packet_size());
  EXPECT_TRUE(connection_.HasQueuedData());

  writer_->SetWritable();
  connection_.OnCanWrite();
  EXPECT_TRUE(writer_->version_negotiation_packet() != nullptr);

  ParsedQuicVersionVector supported_versions = CurrentSupportedVersions();
  ASSERT_EQ(supported_versions.size(),
            writer_->version_negotiation_packet()->versions.size());

  // We expect all versions in supported_versions to be
  // included in the packet.
  for (size_t i = 0; i < supported_versions.size(); ++i) {
    EXPECT_EQ(supported_versions[i],
              writer_->version_negotiation_packet()->versions[i]);
  }
}

TEST_P(QuicConnectionTest,
       ServerSendsVersionNegotiationPacketSocketBlockedDataBuffered) {
  // Turn off QUIC_VERSION_99.
  SetQuicFlag(&FLAGS_quic_enable_version_99, false);
  connection_.SetSupportedVersions(CurrentSupportedVersions());
  set_perspective(Perspective::IS_SERVER);
  if (GetParam().version.transport_version > QUIC_VERSION_43) {
    peer_framer_.set_version_for_tests(
        ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_99));
  } else {
    peer_framer_.set_version_for_tests(
        ParsedQuicVersion(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED));
  }

  QuicPacketHeader header;
  header.destination_connection_id = connection_id_;
  header.version_flag = true;
  header.packet_number = 12;

  QuicFrames frames;
  frames.push_back(QuicFrame(frame1_));
  std::unique_ptr<QuicPacket> packet(ConstructPacket(header, frames));
  char buffer[kMaxPacketSize];
  size_t encryped_length = framer_.EncryptPayload(ENCRYPTION_NONE, 12, *packet,
                                                  buffer, kMaxPacketSize);

  framer_.set_version(version());
  set_perspective(Perspective::IS_SERVER);
  BlockOnNextWrite();
  writer_->set_is_write_blocked_data_buffered(true);
  // Writer's framer's perspective is client, so that it needs to have the right
  // version to process either IETF or GQUIC version negotiation packet.
  writer_->SetSupportedVersions({version()});
  connection_.ProcessUdpPacket(
      kSelfAddress, kPeerAddress,
      QuicReceivedPacket(buffer, encryped_length, QuicTime::Zero(), false));
  EXPECT_EQ(0u, writer_->last_packet_size());
  EXPECT_FALSE(connection_.HasQueuedData());
}

TEST_P(QuicConnectionTest, ClientHandlesVersionNegotiation) {
  // Start out with some unsupported version.
  QuicConnectionPeer::GetFramer(&connection_)
      ->set_version_for_tests(ParsedQuicVersion(
          PROTOCOL_UNSUPPORTED,
          GetParam().version.transport_version == QUIC_VERSION_99
              ? QUIC_VERSION_99
              : QUIC_VERSION_UNSUPPORTED));

  // Send a version negotiation packet.
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      peer_framer_.BuildVersionNegotiationPacket(connection_id_, false,
                                                 AllSupportedVersions()));
  std::unique_ptr<QuicReceivedPacket> received(
      ConstructReceivedPacket(*encrypted, QuicTime::Zero()));
  connection_.ProcessUdpPacket(kSelfAddress, kPeerAddress, *received);

  // Now force another packet.  The connection should transition into
  // NEGOTIATED_VERSION state and tell the packet creator to StopSendingVersion.
  QuicPacketHeader header;
  header.destination_connection_id = connection_id_;
  header.destination_connection_id_length = PACKET_0BYTE_CONNECTION_ID;
  header.packet_number = 12;
  header.version_flag = false;
  QuicFrames frames;
  frames.push_back(QuicFrame(frame1_));
  std::unique_ptr<QuicPacket> packet(ConstructPacket(header, frames));
  char buffer[kMaxPacketSize];
  size_t encrypted_length = peer_framer_.EncryptPayload(
      ENCRYPTION_NONE, 12, *packet, buffer, kMaxPacketSize);
  ASSERT_NE(0u, encrypted_length);
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  connection_.ProcessUdpPacket(
      kSelfAddress, kPeerAddress,
      QuicReceivedPacket(buffer, encrypted_length, QuicTime::Zero(), false));
  if (GetParam().version.transport_version > QUIC_VERSION_43) {
    // IETF QUIC stops sending version when switch to FORWARD_SECURE.
    EXPECT_NE(ENCRYPTION_FORWARD_SECURE, connection_.encryption_level());
    ASSERT_TRUE(QuicPacketCreatorPeer::SendVersionInPacket(creator_));
  } else {
    ASSERT_FALSE(QuicPacketCreatorPeer::SendVersionInPacket(creator_));
  }
}

TEST_P(QuicConnectionTest, BadVersionNegotiation) {
  // Send a version negotiation packet with the version the client started with.
  // It should be rejected.
  EXPECT_CALL(visitor_,
              OnConnectionClosed(QUIC_INVALID_VERSION_NEGOTIATION_PACKET, _,
                                 ConnectionCloseSource::FROM_SELF));
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      framer_.BuildVersionNegotiationPacket(
          connection_id_, connection_.transport_version() > QUIC_VERSION_43,
          AllSupportedVersions()));
  std::unique_ptr<QuicReceivedPacket> received(
      ConstructReceivedPacket(*encrypted, QuicTime::Zero()));
  connection_.ProcessUdpPacket(kSelfAddress, kPeerAddress, *received);
}

TEST_P(QuicConnectionTest, CheckSendStats) {
  connection_.SetMaxTailLossProbes(0);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
  connection_.SendStreamDataWithString(3, "first", 0, NO_FIN);
  size_t first_packet_size = writer_->last_packet_size();

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
  connection_.SendStreamDataWithString(5, "second", 0, NO_FIN);
  size_t second_packet_size = writer_->last_packet_size();

  // 2 retransmissions due to rto, 1 due to explicit nack.
  EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout(true));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(3);

  // Retransmit due to RTO.
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(10));
  connection_.GetRetransmissionAlarm()->Fire();

  // Retransmit due to explicit nacks.
  QuicAckFrame nack_three = InitAckFrame({{2, 3}, {4, 5}});

  LostPacketVector lost_packets;
  lost_packets.push_back(LostPacket(1, kMaxPacketSize));
  lost_packets.push_back(LostPacket(3, kMaxPacketSize));
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _))
      .WillOnce(SetArgPointee<5>(lost_packets));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  if (!connection_.session_decides_what_to_write()) {
    EXPECT_CALL(visitor_, OnCanWrite());
  }
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  ProcessAckPacket(&nack_three);

  EXPECT_CALL(*send_algorithm_, BandwidthEstimate())
      .WillOnce(Return(QuicBandwidth::Zero()));

  const QuicConnectionStats& stats = connection_.GetStats();
  // For IETF QUIC, version is not included as the encryption level switches to
  // FORWARD_SECURE in SendStreamDataWithString.
  size_t save_on_version =
      GetParam().version.transport_version > QUIC_VERSION_43 ? 0
                                                             : kQuicVersionSize;
  EXPECT_EQ(3 * first_packet_size + 2 * second_packet_size - save_on_version,
            stats.bytes_sent);
  EXPECT_EQ(5u, stats.packets_sent);
  EXPECT_EQ(2 * first_packet_size + second_packet_size - save_on_version,
            stats.bytes_retransmitted);
  EXPECT_EQ(3u, stats.packets_retransmitted);
  EXPECT_EQ(1u, stats.rto_count);
  EXPECT_EQ(kDefaultMaxPacketSize, stats.max_packet_size);
}

TEST_P(QuicConnectionTest, ProcessFramesIfPacketClosedConnection) {
  // Construct a packet with stream frame and connection close frame.
  QuicPacketHeader header;
  header.destination_connection_id = connection_id_;
  if (peer_framer_.transport_version() > QUIC_VERSION_43) {
    header.destination_connection_id_length = PACKET_0BYTE_CONNECTION_ID;
  }
  header.packet_number = 1;
  header.version_flag = false;

  QuicConnectionCloseFrame qccf;
  qccf.error_code = QUIC_PEER_GOING_AWAY;

  QuicFrames frames;
  frames.push_back(QuicFrame(frame1_));
  frames.push_back(QuicFrame(&qccf));
  std::unique_ptr<QuicPacket> packet(ConstructPacket(header, frames));
  EXPECT_TRUE(nullptr != packet);
  char buffer[kMaxPacketSize];
  size_t encrypted_length = peer_framer_.EncryptPayload(
      ENCRYPTION_NONE, 1, *packet, buffer, kMaxPacketSize);

  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_PEER_GOING_AWAY, _,
                                           ConnectionCloseSource::FROM_PEER));
  EXPECT_CALL(visitor_, OnStreamFrame(_)).Times(1);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  connection_.ProcessUdpPacket(
      kSelfAddress, kPeerAddress,
      QuicReceivedPacket(buffer, encrypted_length, QuicTime::Zero(), false));
}

TEST_P(QuicConnectionTest, SelectMutualVersion) {
  connection_.SetSupportedVersions(AllSupportedVersions());
  // Set the connection to speak the lowest quic version.
  connection_.set_version(QuicVersionMin());
  EXPECT_EQ(QuicVersionMin(), connection_.version());

  // Pass in available versions which includes a higher mutually supported
  // version.  The higher mutually supported version should be selected.
  ParsedQuicVersionVector supported_versions = AllSupportedVersions();
  EXPECT_TRUE(connection_.SelectMutualVersion(supported_versions));
  EXPECT_EQ(QuicVersionMax(), connection_.version());

  // Expect that the lowest version is selected.
  // Ensure the lowest supported version is less than the max, unless they're
  // the same.
  ParsedQuicVersionVector lowest_version_vector;
  lowest_version_vector.push_back(QuicVersionMin());
  EXPECT_TRUE(connection_.SelectMutualVersion(lowest_version_vector));
  EXPECT_EQ(QuicVersionMin(), connection_.version());

  // Shouldn't be able to find a mutually supported version.
  ParsedQuicVersionVector unsupported_version;
  unsupported_version.push_back(
      ParsedQuicVersion(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED));
  EXPECT_FALSE(connection_.SelectMutualVersion(unsupported_version));
}

TEST_P(QuicConnectionTest, ConnectionCloseWhenWritable) {
  EXPECT_FALSE(writer_->IsWriteBlocked());

  // Send a packet.
  connection_.SendStreamDataWithString(1, "foo", 0, NO_FIN);
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  EXPECT_EQ(1u, writer_->packets_write_attempts());

  TriggerConnectionClose();
  EXPECT_EQ(2u, writer_->packets_write_attempts());
}

TEST_P(QuicConnectionTest, ConnectionCloseGettingWriteBlocked) {
  BlockOnNextWrite();
  TriggerConnectionClose();
  EXPECT_EQ(1u, writer_->packets_write_attempts());
  EXPECT_TRUE(writer_->IsWriteBlocked());
}

TEST_P(QuicConnectionTest, ConnectionCloseWhenWriteBlocked) {
  BlockOnNextWrite();
  connection_.SendStreamDataWithString(1, "foo", 0, NO_FIN);
  EXPECT_EQ(1u, connection_.NumQueuedPackets());
  EXPECT_EQ(1u, writer_->packets_write_attempts());
  EXPECT_TRUE(writer_->IsWriteBlocked());
  TriggerConnectionClose();
  EXPECT_EQ(1u, writer_->packets_write_attempts());
}

TEST_P(QuicConnectionTest, OnPacketSentDebugVisitor) {
  MockQuicConnectionDebugVisitor debug_visitor;
  connection_.set_debug_visitor(&debug_visitor);

  EXPECT_CALL(debug_visitor, OnPacketSent(_, _, _, _)).Times(1);
  connection_.SendStreamDataWithString(1, "foo", 0, NO_FIN);

  EXPECT_CALL(debug_visitor, OnPacketSent(_, _, _, _)).Times(1);
  connection_.SendConnectivityProbingPacket(writer_.get(),
                                            connection_.peer_address());
}

TEST_P(QuicConnectionTest, OnPacketHeaderDebugVisitor) {
  QuicPacketHeader header;
  header.packet_number = 1;
  if (GetParam().version.transport_version > QUIC_VERSION_43) {
    QuicFramerPeer::SetLastPacketIsIetfQuic(
        QuicConnectionPeer::GetFramer(&connection_), true);
  }

  MockQuicConnectionDebugVisitor debug_visitor;
  connection_.set_debug_visitor(&debug_visitor);
  EXPECT_CALL(debug_visitor, OnPacketHeader(Ref(header))).Times(1);
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_)).Times(1);
  EXPECT_CALL(debug_visitor, OnSuccessfulVersionNegotiation(_)).Times(1);
  connection_.OnPacketHeader(header);
}

TEST_P(QuicConnectionTest, Pacing) {
  TestConnection server(connection_id_, kSelfAddress, helper_.get(),
                        alarm_factory_.get(), writer_.get(),
                        Perspective::IS_SERVER, version());
  TestConnection client(connection_id_, kPeerAddress, helper_.get(),
                        alarm_factory_.get(), writer_.get(),
                        Perspective::IS_CLIENT, version());
  EXPECT_FALSE(QuicSentPacketManagerPeer::UsingPacing(
      static_cast<const QuicSentPacketManager*>(
          &client.sent_packet_manager())));
  EXPECT_FALSE(QuicSentPacketManagerPeer::UsingPacing(
      static_cast<const QuicSentPacketManager*>(
          &server.sent_packet_manager())));
}

TEST_P(QuicConnectionTest, WindowUpdateInstigateAcks) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  // Send a WINDOW_UPDATE frame.
  QuicWindowUpdateFrame window_update;
  window_update.stream_id = 3;
  window_update.byte_offset = 1234;
  EXPECT_CALL(visitor_, OnWindowUpdateFrame(_));
  ProcessFramePacket(QuicFrame(&window_update));

  // Ensure that this has caused the ACK alarm to be set.
  QuicAlarm* ack_alarm = QuicConnectionPeer::GetAckAlarm(&connection_);
  EXPECT_TRUE(ack_alarm->IsSet());
}

TEST_P(QuicConnectionTest, BlockedFrameInstigateAcks) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  // Send a BLOCKED frame.
  QuicBlockedFrame blocked;
  blocked.stream_id = 3;
  EXPECT_CALL(visitor_, OnBlockedFrame(_));
  ProcessFramePacket(QuicFrame(&blocked));

  // Ensure that this has caused the ACK alarm to be set.
  QuicAlarm* ack_alarm = QuicConnectionPeer::GetAckAlarm(&connection_);
  EXPECT_TRUE(ack_alarm->IsSet());
}

TEST_P(QuicConnectionTest, ReevaluateTimeUntilSendOnAck) {
  // Enable pacing.
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  QuicConfig config;
  connection_.SetFromConfig(config);

  // Send two packets.  One packet is not sufficient because if it gets acked,
  // there will be no packets in flight after that and the pacer will always
  // allow the next packet in that situation.
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, CanSend(_)).WillRepeatedly(Return(true));
  connection_.SendStreamDataWithString(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "foo",
      0, NO_FIN);
  connection_.SendStreamDataWithString(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "bar",
      3, NO_FIN);
  connection_.OnCanWrite();

  // Schedule the next packet for a few milliseconds in future.
  QuicSentPacketManagerPeer::DisablePacerBursts(manager_);
  QuicTime scheduled_pacing_time =
      clock_.Now() + QuicTime::Delta::FromMilliseconds(5);
  QuicSentPacketManagerPeer::SetNextPacedPacketTime(manager_,
                                                    scheduled_pacing_time);

  // Send a packet and have it be blocked by congestion control.
  EXPECT_CALL(*send_algorithm_, CanSend(_)).WillRepeatedly(Return(false));
  connection_.SendStreamDataWithString(
      GetNthClientInitiatedStreamId(1, connection_.transport_version()), "baz",
      6, NO_FIN);
  EXPECT_FALSE(connection_.GetSendAlarm()->IsSet());

  // Process an ack and the send alarm will be set to the new 5ms delay.
  QuicAckFrame ack = InitAckFrame(1);
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, CanSend(_)).WillRepeatedly(Return(true));
  ProcessAckPacket(&ack);
  EXPECT_EQ(1u, writer_->frame_count());
  EXPECT_EQ(1u, writer_->stream_frames().size());
  EXPECT_TRUE(connection_.GetSendAlarm()->IsSet());
  EXPECT_EQ(scheduled_pacing_time, connection_.GetSendAlarm()->deadline());
  writer_->Reset();
}

TEST_P(QuicConnectionTest, SendAcksImmediately) {
  CongestionBlockWrites();
  SendAckPacketToPeer();
}

TEST_P(QuicConnectionTest, SendPingImmediately) {
  MockQuicConnectionDebugVisitor debug_visitor;
  connection_.set_debug_visitor(&debug_visitor);

  CongestionBlockWrites();
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  EXPECT_CALL(debug_visitor, OnPacketSent(_, _, _, _)).Times(1);
  EXPECT_CALL(debug_visitor, OnPingSent()).Times(1);
  connection_.SendControlFrame(QuicFrame(QuicPingFrame(1)));
  EXPECT_FALSE(connection_.HasQueuedData());
}

TEST_P(QuicConnectionTest, SendBlockedImmediately) {
  MockQuicConnectionDebugVisitor debug_visitor;
  connection_.set_debug_visitor(&debug_visitor);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  EXPECT_CALL(debug_visitor, OnPacketSent(_, _, _, _)).Times(1);
  EXPECT_EQ(0u, connection_.GetStats().blocked_frames_sent);
  connection_.SendControlFrame(QuicFrame(new QuicBlockedFrame(1, 3)));
  EXPECT_EQ(1u, connection_.GetStats().blocked_frames_sent);
  EXPECT_FALSE(connection_.HasQueuedData());
}

TEST_P(QuicConnectionTest, SendingUnencryptedStreamDataFails) {
  // EXPECT_QUIC_BUG tests are expensive so only run one instance of them.
  if (!IsDefaultTestConfiguration()) {
    return;
  }

  EXPECT_CALL(visitor_,
              OnConnectionClosed(QUIC_ATTEMPT_TO_SEND_UNENCRYPTED_STREAM_DATA,
                                 _, ConnectionCloseSource::FROM_SELF));
  struct iovec iov;
  MakeIOVector("", &iov);
  EXPECT_QUIC_BUG(connection_.SaveAndSendStreamData(3, &iov, 1, 0, 0, FIN),
                  "Cannot send stream data without encryption.");
  EXPECT_FALSE(connection_.connected());
}

TEST_P(QuicConnectionTest, SetRetransmissionAlarmForCryptoPacket) {
  EXPECT_TRUE(connection_.connected());
  EXPECT_FALSE(connection_.GetRetransmissionAlarm()->IsSet());

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  connection_.SendCryptoStreamData();

  // Verify retransmission timer is correctly set after crypto packet has been
  // sent.
  EXPECT_TRUE(connection_.GetRetransmissionAlarm()->IsSet());
  QuicTime retransmission_time =
      QuicConnectionPeer::GetSentPacketManager(&connection_)
          ->GetRetransmissionTime();
  EXPECT_NE(retransmission_time, clock_.ApproximateNow());
  EXPECT_EQ(retransmission_time,
            connection_.GetRetransmissionAlarm()->deadline());

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  connection_.GetRetransmissionAlarm()->Fire();
}

TEST_P(QuicConnectionTest, PathDegradingAlarmForCryptoPacket) {
  EXPECT_TRUE(connection_.connected());
  EXPECT_FALSE(connection_.GetPathDegradingAlarm()->IsSet());
  EXPECT_FALSE(connection_.IsPathDegrading());

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  connection_.SendCryptoStreamData();

  EXPECT_TRUE(connection_.GetPathDegradingAlarm()->IsSet());
  EXPECT_FALSE(connection_.IsPathDegrading());
  QuicTime::Delta delay = QuicConnectionPeer::GetSentPacketManager(&connection_)
                              ->GetPathDegradingDelay();
  EXPECT_EQ(clock_.ApproximateNow() + delay,
            connection_.GetPathDegradingAlarm()->deadline());

  // Fire the path degrading alarm, path degrading signal should be sent to
  // the visitor.
  EXPECT_CALL(visitor_, OnPathDegrading());
  clock_.AdvanceTime(delay);
  connection_.GetPathDegradingAlarm()->Fire();
  EXPECT_TRUE(connection_.IsPathDegrading());
  EXPECT_FALSE(connection_.GetPathDegradingAlarm()->IsSet());
}

// Includes regression test for https://b.corp.google.com/issues/69979024.
TEST_P(QuicConnectionTest, PathDegradingAlarmForNonCryptoPackets) {
  EXPECT_TRUE(connection_.connected());
  EXPECT_FALSE(connection_.GetPathDegradingAlarm()->IsSet());
  EXPECT_FALSE(connection_.IsPathDegrading());

  const char data[] = "data";
  size_t data_size = strlen(data);
  QuicStreamOffset offset = 0;

  for (int i = 0; i < 2; ++i) {
    // Send a packet. Now there's a retransmittable packet on the wire, so the
    // path degrading alarm should be set.
    connection_.SendStreamDataWithString(
        GetNthClientInitiatedStreamId(1, connection_.transport_version()), data,
        offset, NO_FIN);
    offset += data_size;
    EXPECT_TRUE(connection_.GetPathDegradingAlarm()->IsSet());
    // Check the deadline of the path degrading alarm.
    QuicTime::Delta delay =
        QuicConnectionPeer::GetSentPacketManager(&connection_)
            ->GetPathDegradingDelay();
    EXPECT_EQ(clock_.ApproximateNow() + delay,
              connection_.GetPathDegradingAlarm()->deadline());

    // Send a second packet. The path degrading alarm's deadline should remain
    // the same.
    // Regression test for https://b.corp.google.com/issues/69979024.
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
    QuicTime prev_deadline = connection_.GetPathDegradingAlarm()->deadline();
    connection_.SendStreamDataWithString(
        GetNthClientInitiatedStreamId(1, connection_.transport_version()), data,
        offset, NO_FIN);
    offset += data_size;
    EXPECT_TRUE(connection_.GetPathDegradingAlarm()->IsSet());
    EXPECT_EQ(prev_deadline, connection_.GetPathDegradingAlarm()->deadline());

    // Now receive an ACK of the first packet. This should advance the path
    // degrading alarm's deadline since forward progress has been made.
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
    if (i == 0) {
      EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
    }
    EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
    QuicAckFrame frame = InitAckFrame({{1u + 2u * i, 2u + 2u * i}});
    ProcessAckPacket(&frame);
    EXPECT_TRUE(connection_.GetPathDegradingAlarm()->IsSet());
    // Check the deadline of the path degrading alarm.
    delay = QuicConnectionPeer::GetSentPacketManager(&connection_)
                ->GetPathDegradingDelay();
    EXPECT_EQ(clock_.ApproximateNow() + delay,
              connection_.GetPathDegradingAlarm()->deadline());

    if (i == 0) {
      // Now receive an ACK of the second packet. Since there are no more
      // retransmittable packets on the wire, this should cancel the path
      // degrading alarm.
      clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
      EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
      frame = InitAckFrame({{2, 3}});
      ProcessAckPacket(&frame);
      EXPECT_FALSE(connection_.GetPathDegradingAlarm()->IsSet());
    } else {
      // Advance time to the path degrading alarm's deadline and simulate
      // firing the alarm.
      clock_.AdvanceTime(delay);
      EXPECT_CALL(visitor_, OnPathDegrading());
      connection_.GetPathDegradingAlarm()->Fire();
      EXPECT_FALSE(connection_.GetPathDegradingAlarm()->IsSet());
    }
  }
  EXPECT_TRUE(connection_.IsPathDegrading());
}

TEST_P(QuicConnectionTest, RetransmittableOnWireSetsPathDegradingAlarm) {
  const QuicTime::Delta retransmittable_on_wire_timeout =
      QuicTime::Delta::FromMilliseconds(50);
  connection_.set_retransmittable_on_wire_timeout(
      retransmittable_on_wire_timeout);

  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(visitor_, HasOpenDynamicStreams()).WillRepeatedly(Return(true));

  EXPECT_FALSE(connection_.GetPathDegradingAlarm()->IsSet());
  EXPECT_FALSE(connection_.IsPathDegrading());
  EXPECT_FALSE(connection_.GetRetransmittableOnWireAlarm()->IsSet());

  const char data[] = "data";
  size_t data_size = strlen(data);
  QuicStreamOffset offset = 0;

  // Send a packet.
  connection_.SendStreamDataWithString(1, data, offset, NO_FIN);
  offset += data_size;
  // Now there's a retransmittable packet on the wire, so the path degrading
  // alarm should be set.
  // The retransmittable-on-wire alarm should not be set.
  EXPECT_TRUE(connection_.GetPathDegradingAlarm()->IsSet());
  QuicTime::Delta delay = QuicConnectionPeer::GetSentPacketManager(&connection_)
                              ->GetPathDegradingDelay();
  EXPECT_EQ(clock_.ApproximateNow() + delay,
            connection_.GetPathDegradingAlarm()->deadline());
  EXPECT_FALSE(connection_.GetRetransmittableOnWireAlarm()->IsSet());

  // Now receive an ACK of the packet.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  QuicAckFrame frame = InitAckFrame({{1, 2}});
  ProcessAckPacket(&frame);
  // No more retransmittable packets on the wire, so the path degrading alarm
  // should be cancelled, and the retransmittable-on-wire alarm should be set
  // since a PING might be needed.
  EXPECT_FALSE(connection_.GetPathDegradingAlarm()->IsSet());
  EXPECT_TRUE(connection_.GetRetransmittableOnWireAlarm()->IsSet());
  EXPECT_EQ(clock_.ApproximateNow() + retransmittable_on_wire_timeout,
            connection_.GetRetransmittableOnWireAlarm()->deadline());

  // Simulate firing the retransmittable-on-wire alarm and sending a PING.
  clock_.AdvanceTime(retransmittable_on_wire_timeout);
  EXPECT_CALL(visitor_, SendPing()).WillOnce(Invoke([this]() {
    connection_.SendControlFrame(QuicFrame(QuicPingFrame(1)));
  }));
  connection_.GetRetransmittableOnWireAlarm()->Fire();

  // Now there's a retransmittable packet (PING) on the wire, so the path
  // degrading alarm should be set.
  EXPECT_TRUE(connection_.GetPathDegradingAlarm()->IsSet());
  delay = QuicConnectionPeer::GetSentPacketManager(&connection_)
              ->GetPathDegradingDelay();
  EXPECT_EQ(clock_.ApproximateNow() + delay,
            connection_.GetPathDegradingAlarm()->deadline());
}

// This test verifies that the connection marks path as degrading and does not
// spin timer to detect path degrading when a new packet is sent on the
// degraded path.
TEST_P(QuicConnectionTest, NoPathDegradingAlarmIfPathIsDegrading) {
  EXPECT_TRUE(connection_.connected());
  EXPECT_FALSE(connection_.GetPathDegradingAlarm()->IsSet());
  EXPECT_FALSE(connection_.IsPathDegrading());

  const char data[] = "data";
  size_t data_size = strlen(data);
  QuicStreamOffset offset = 0;

  // Send the first packet. Now there's a retransmittable packet on the wire, so
  // the path degrading alarm should be set.
  connection_.SendStreamDataWithString(1, data, offset, NO_FIN);
  offset += data_size;
  EXPECT_TRUE(connection_.GetPathDegradingAlarm()->IsSet());
  // Check the deadline of the path degrading alarm.
  QuicTime::Delta delay = QuicConnectionPeer::GetSentPacketManager(&connection_)
                              ->GetPathDegradingDelay();
  EXPECT_EQ(clock_.ApproximateNow() + delay,
            connection_.GetPathDegradingAlarm()->deadline());

  // Send a second packet. The path degrading alarm's deadline should remain
  // the same.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  QuicTime prev_deadline = connection_.GetPathDegradingAlarm()->deadline();
  connection_.SendStreamDataWithString(1, data, offset, NO_FIN);
  offset += data_size;
  EXPECT_TRUE(connection_.GetPathDegradingAlarm()->IsSet());
  EXPECT_EQ(prev_deadline, connection_.GetPathDegradingAlarm()->deadline());

  // Now receive an ACK of the first packet. This should advance the path
  // degrading alarm's deadline since forward progress has been made.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  QuicAckFrame frame = InitAckFrame({{1u, 2u}});
  ProcessAckPacket(&frame);
  EXPECT_TRUE(connection_.GetPathDegradingAlarm()->IsSet());
  // Check the deadline of the path degrading alarm.
  delay = QuicConnectionPeer::GetSentPacketManager(&connection_)
              ->GetPathDegradingDelay();
  EXPECT_EQ(clock_.ApproximateNow() + delay,
            connection_.GetPathDegradingAlarm()->deadline());

  // Advance time to the path degrading alarm's deadline and simulate
  // firing the path degrading alarm. This path will be considered as
  // degrading.
  clock_.AdvanceTime(delay);
  EXPECT_CALL(visitor_, OnPathDegrading()).Times(1);
  connection_.GetPathDegradingAlarm()->Fire();
  EXPECT_FALSE(connection_.GetPathDegradingAlarm()->IsSet());
  EXPECT_TRUE(connection_.IsPathDegrading());

  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_FALSE(connection_.GetPathDegradingAlarm()->IsSet());
  // Send a third packet. The path degrading alarm is no longer set but path
  // should still be marked as degrading.
  connection_.SendStreamDataWithString(1, data, offset, NO_FIN);
  offset += data_size;
  EXPECT_FALSE(connection_.GetPathDegradingAlarm()->IsSet());
  EXPECT_TRUE(connection_.IsPathDegrading());
}

// This test verifies that the connection unmarks path as degrarding and spins
// the timer to detect future path degrading when forward progress is made
// after path has been marked degrading.
TEST_P(QuicConnectionTest, UnmarkPathDegradingOnForwardProgress) {
  EXPECT_TRUE(connection_.connected());
  EXPECT_FALSE(connection_.GetPathDegradingAlarm()->IsSet());
  EXPECT_FALSE(connection_.IsPathDegrading());

  const char data[] = "data";
  size_t data_size = strlen(data);
  QuicStreamOffset offset = 0;

  // Send the first packet. Now there's a retransmittable packet on the wire, so
  // the path degrading alarm should be set.
  connection_.SendStreamDataWithString(1, data, offset, NO_FIN);
  offset += data_size;
  EXPECT_TRUE(connection_.GetPathDegradingAlarm()->IsSet());
  // Check the deadline of the path degrading alarm.
  QuicTime::Delta delay = QuicConnectionPeer::GetSentPacketManager(&connection_)
                              ->GetPathDegradingDelay();
  EXPECT_EQ(clock_.ApproximateNow() + delay,
            connection_.GetPathDegradingAlarm()->deadline());

  // Send a second packet. The path degrading alarm's deadline should remain
  // the same.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  QuicTime prev_deadline = connection_.GetPathDegradingAlarm()->deadline();
  connection_.SendStreamDataWithString(1, data, offset, NO_FIN);
  offset += data_size;
  EXPECT_TRUE(connection_.GetPathDegradingAlarm()->IsSet());
  EXPECT_EQ(prev_deadline, connection_.GetPathDegradingAlarm()->deadline());

  // Now receive an ACK of the first packet. This should advance the path
  // degrading alarm's deadline since forward progress has been made.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  QuicAckFrame frame = InitAckFrame({{1u, 2u}});
  ProcessAckPacket(&frame);
  EXPECT_TRUE(connection_.GetPathDegradingAlarm()->IsSet());
  // Check the deadline of the path degrading alarm.
  delay = QuicConnectionPeer::GetSentPacketManager(&connection_)
              ->GetPathDegradingDelay();
  EXPECT_EQ(clock_.ApproximateNow() + delay,
            connection_.GetPathDegradingAlarm()->deadline());

  // Advance time to the path degrading alarm's deadline and simulate
  // firing the alarm.
  clock_.AdvanceTime(delay);
  EXPECT_CALL(visitor_, OnPathDegrading()).Times(1);
  connection_.GetPathDegradingAlarm()->Fire();
  EXPECT_FALSE(connection_.GetPathDegradingAlarm()->IsSet());
  EXPECT_TRUE(connection_.IsPathDegrading());

  // Send a third packet. The path degrading alarm is no longer set but path
  // should still be marked as degrading.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_FALSE(connection_.GetPathDegradingAlarm()->IsSet());
  connection_.SendStreamDataWithString(1, data, offset, NO_FIN);
  offset += data_size;
  EXPECT_FALSE(connection_.GetPathDegradingAlarm()->IsSet());
  EXPECT_TRUE(connection_.IsPathDegrading());

  // Now receive an ACK of the second packet. This should unmark the path as
  // degrading. And will set a timer to detect new path degrading.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  frame = InitAckFrame({{2, 3}});
  ProcessAckPacket(&frame);
  EXPECT_FALSE(connection_.IsPathDegrading());
  EXPECT_TRUE(connection_.GetPathDegradingAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, NoPathDegradingOnServer) {
  SetQuicReloadableFlag(quic_fix_path_degrading_alarm, true);
  set_perspective(Perspective::IS_SERVER);
  QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);

  EXPECT_FALSE(connection_.IsPathDegrading());
  EXPECT_FALSE(connection_.GetPathDegradingAlarm()->IsSet());

  // Send data.
  const char data[] = "data";
  connection_.SendStreamDataWithString(1, data, 0, NO_FIN);
  EXPECT_FALSE(connection_.IsPathDegrading());
  EXPECT_FALSE(connection_.GetPathDegradingAlarm()->IsSet());

  // Ack data.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  QuicAckFrame frame = InitAckFrame({{1u, 2u}});
  ProcessAckPacket(&frame);
  EXPECT_FALSE(connection_.IsPathDegrading());
  EXPECT_FALSE(connection_.GetPathDegradingAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, NoPathDegradingAfterSendingAck) {
  SetQuicReloadableFlag(quic_fix_path_degrading_alarm, true);

  SendAckPacketToPeer();
  EXPECT_FALSE(connection_.sent_packet_manager().unacked_packets().empty());
  EXPECT_FALSE(connection_.sent_packet_manager().HasInFlightPackets());
  EXPECT_FALSE(connection_.IsPathDegrading());
  EXPECT_FALSE(connection_.GetPathDegradingAlarm()->IsSet());
}

TEST_P(QuicConnectionTest, MultipleCallsToCloseConnection) {
  // Verifies that multiple calls to CloseConnection do not
  // result in multiple attempts to close the connection - it will be marked as
  // disconnected after the first call.
  EXPECT_CALL(visitor_, OnConnectionClosed(_, _, _)).Times(1);
  connection_.CloseConnection(QUIC_NO_ERROR, "no reason",
                              ConnectionCloseBehavior::SILENT_CLOSE);
  connection_.CloseConnection(QUIC_NO_ERROR, "no reason",
                              ConnectionCloseBehavior::SILENT_CLOSE);
}

TEST_P(QuicConnectionTest, ServerReceivesChloOnNonCryptoStream) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  set_perspective(Perspective::IS_SERVER);
  QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);

  CryptoHandshakeMessage message;
  CryptoFramer framer;
  message.set_tag(kCHLO);
  std::unique_ptr<QuicData> data(framer.ConstructHandshakeMessage(message));
  frame1_.stream_id = 10;
  frame1_.data_buffer = data->data();
  frame1_.data_length = data->length();

  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_MAYBE_CORRUPTED_MEMORY, _,
                                           ConnectionCloseSource::FROM_SELF));
  ForceProcessFramePacket(QuicFrame(frame1_));
}

TEST_P(QuicConnectionTest, ClientReceivesRejOnNonCryptoStream) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));

  CryptoHandshakeMessage message;
  CryptoFramer framer;
  message.set_tag(kREJ);
  std::unique_ptr<QuicData> data(framer.ConstructHandshakeMessage(message));
  frame1_.stream_id = 10;
  frame1_.data_buffer = data->data();
  frame1_.data_length = data->length();

  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_MAYBE_CORRUPTED_MEMORY, _,
                                           ConnectionCloseSource::FROM_SELF));
  ForceProcessFramePacket(QuicFrame(frame1_));
}

TEST_P(QuicConnectionTest, CloseConnectionOnPacketTooLarge) {
  SimulateNextPacketTooLarge();
  // A connection close packet is sent
  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_PACKET_WRITE_ERROR, _,
                                           ConnectionCloseSource::FROM_SELF))
      .Times(1);
  connection_.SendStreamDataWithString(3, "foo", 0, NO_FIN);
}

TEST_P(QuicConnectionTest, AlwaysGetPacketTooLarge) {
  // Test even we always get packet too large, we do not infinitely try to send
  // close packet.
  AlwaysGetPacketTooLarge();
  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_PACKET_WRITE_ERROR, _,
                                           ConnectionCloseSource::FROM_SELF))
      .Times(1);
  connection_.SendStreamDataWithString(3, "foo", 0, NO_FIN);
}

// Verify that if connection has no outstanding data, it notifies the send
// algorithm after the write.
TEST_P(QuicConnectionTest, SendDataAndBecomeApplicationLimited) {
  EXPECT_CALL(*send_algorithm_, OnApplicationLimited(_)).Times(1);
  {
    InSequence seq;
    EXPECT_CALL(visitor_, WillingAndAbleToWrite()).WillRepeatedly(Return(true));
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
    EXPECT_CALL(visitor_, WillingAndAbleToWrite())
        .WillRepeatedly(Return(false));
  }

  connection_.SendStreamData3();
}

// Verify that the connection does not become app-limited if there is
// outstanding data to send after the write.
TEST_P(QuicConnectionTest, NotBecomeApplicationLimitedIfMoreDataAvailable) {
  EXPECT_CALL(*send_algorithm_, OnApplicationLimited(_)).Times(0);
  {
    InSequence seq;
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
    EXPECT_CALL(visitor_, WillingAndAbleToWrite()).WillRepeatedly(Return(true));
  }

  connection_.SendStreamData3();
}

// Verify that the connection does not become app-limited after blocked write
// even if there is outstanding data to send after the write.
TEST_P(QuicConnectionTest, NotBecomeApplicationLimitedDueToWriteBlock) {
  EXPECT_CALL(*send_algorithm_, OnApplicationLimited(_)).Times(0);
  EXPECT_CALL(visitor_, WillingAndAbleToWrite()).WillRepeatedly(Return(true));
  BlockOnNextWrite();

  connection_.SendStreamData3();

  // Now unblock the writer, become congestion control blocked,
  // and ensure we become app-limited after writing.
  writer_->SetWritable();
  CongestionBlockWrites();
  EXPECT_CALL(visitor_, WillingAndAbleToWrite()).WillRepeatedly(Return(false));
  {
    InSequence seq;
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
    EXPECT_CALL(*send_algorithm_, OnApplicationLimited(_)).Times(1);
  }
  connection_.OnCanWrite();
}

// Test the mode in which the link is filled up with probing retransmissions if
// the connection becomes application-limited.
TEST_P(QuicConnectionTest, SendDataWhenApplicationLimited) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, ShouldSendProbingPacket())
      .WillRepeatedly(Return(true));
  {
    InSequence seq;
    EXPECT_CALL(visitor_, WillingAndAbleToWrite()).WillRepeatedly(Return(true));
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _));
    EXPECT_CALL(visitor_, WillingAndAbleToWrite())
        .WillRepeatedly(Return(false));
  }
  // Fix congestion window to be 20,000 bytes.
  EXPECT_CALL(*send_algorithm_, CanSend(Ge(20000u)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*send_algorithm_, CanSend(Lt(20000u)))
      .WillRepeatedly(Return(true));

  EXPECT_CALL(*send_algorithm_, OnApplicationLimited(_)).Times(0);
  ASSERT_EQ(0u, connection_.GetStats().packets_sent);
  connection_.set_fill_up_link_during_probing(true);
  connection_.OnHandshakeComplete();
  connection_.SendStreamData3();

  // We expect a lot of packets from a 20 kbyte window.
  EXPECT_GT(connection_.GetStats().packets_sent, 10u);
  // Ensure that the packets are padded.
  QuicByteCount average_packet_size =
      connection_.GetStats().bytes_sent / connection_.GetStats().packets_sent;
  EXPECT_GT(average_packet_size, 1000u);

  // Acknowledge all packets sent, except for the last one.
  QuicAckFrame ack = InitAckFrame(
      connection_.sent_packet_manager().GetLargestSentPacket() - 1);
  EXPECT_CALL(*loss_algorithm_, DetectLosses(_, _, _, _, _, _));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));

  // Ensure that since we no longer have retransmittable bytes in flight, this
  // will not cause any responses to be sent.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*send_algorithm_, OnApplicationLimited(_)).Times(1);
  ProcessAckPacket(&ack);
}

TEST_P(QuicConnectionTest, DonotForceSendingAckOnPacketTooLarge) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  // Send an ack by simulating delayed ack alarm firing.
  ProcessPacket(1);
  QuicAlarm* ack_alarm = QuicConnectionPeer::GetAckAlarm(&connection_);
  EXPECT_TRUE(ack_alarm->IsSet());
  connection_.GetAckAlarm()->Fire();
  // Simulate data packet causes write error.
  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_PACKET_WRITE_ERROR, _, _));
  SimulateNextPacketTooLarge();
  connection_.SendStreamDataWithString(3, "foo", 0, NO_FIN);
  EXPECT_EQ(1u, writer_->frame_count());
  EXPECT_FALSE(writer_->connection_close_frames().empty());
  // Ack frame is not bundled in connection close packet.
  EXPECT_TRUE(writer_->ack_frames().empty());
}

TEST_P(QuicConnectionTest, CloseConnectionForStatelessReject) {
  QuicString error_details("stateless reject");
  EXPECT_CALL(visitor_, OnConnectionClosed(
                            QUIC_CRYPTO_HANDSHAKE_STATELESS_REJECT,
                            error_details, ConnectionCloseSource::FROM_PEER));
  connection_.set_perspective(Perspective::IS_CLIENT);
  connection_.CloseConnection(QUIC_CRYPTO_HANDSHAKE_STATELESS_REJECT,
                              error_details,
                              ConnectionCloseBehavior::SILENT_CLOSE);
}

// Regression test for b/63620844.
TEST_P(QuicConnectionTest, FailedToWriteHandshakePacket) {
  SimulateNextPacketTooLarge();
  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_PACKET_WRITE_ERROR, _,
                                           ConnectionCloseSource::FROM_SELF))
      .Times(1);
  connection_.SendCryptoStreamData();
}

TEST_P(QuicConnectionTest, MaxPacingRate) {
  EXPECT_EQ(0, connection_.MaxPacingRate().ToBytesPerSecond());
  connection_.SetMaxPacingRate(QuicBandwidth::FromBytesPerSecond(100));
  EXPECT_EQ(100, connection_.MaxPacingRate().ToBytesPerSecond());
}

TEST_P(QuicConnectionTest, ClientAlwaysSendConnectionId) {
  EXPECT_EQ(Perspective::IS_CLIENT, connection_.perspective());
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  connection_.SendStreamDataWithString(3, "foo", 0, NO_FIN);
  EXPECT_EQ(PACKET_8BYTE_CONNECTION_ID,
            writer_->last_packet_header().destination_connection_id_length);

  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  QuicConfig config;
  QuicConfigPeer::SetReceivedBytesForConnectionId(&config, 0);
  connection_.SetFromConfig(config);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  connection_.SendStreamDataWithString(3, "bar", 3, NO_FIN);
  // Verify connection id is still sent in the packet.
  EXPECT_EQ(PACKET_8BYTE_CONNECTION_ID,
            writer_->last_packet_header().destination_connection_id_length);
}

TEST_P(QuicConnectionTest, SendProbingRetransmissions) {
  MockQuicConnectionDebugVisitor debug_visitor;
  connection_.set_debug_visitor(&debug_visitor);

  const QuicStreamId stream_id = 2;
  QuicPacketNumber last_packet;
  SendStreamDataToPeer(stream_id, "foo", 0, NO_FIN, &last_packet);
  SendStreamDataToPeer(stream_id, "bar", 3, NO_FIN, &last_packet);
  SendStreamDataToPeer(stream_id, "test", 6, NO_FIN, &last_packet);

  const QuicByteCount old_bytes_in_flight =
      connection_.sent_packet_manager().GetBytesInFlight();

  // Allow 9 probing retransmissions to be sent.
  {
    InSequence seq;
    EXPECT_CALL(*send_algorithm_, CanSend(_))
        .Times(9 * 2)
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*send_algorithm_, CanSend(_)).WillOnce(Return(false));
  }
  // Expect them retransmitted in cyclic order (foo, bar, test, foo, bar...).
  QuicPacketCount sent_count = 0;
  EXPECT_CALL(debug_visitor, OnPacketSent(_, _, _, _))
      .WillRepeatedly(Invoke([this, &sent_count](const SerializedPacket&,
                                                 QuicPacketNumber,
                                                 TransmissionType, QuicTime) {
        ASSERT_EQ(1u, writer_->stream_frames().size());
        // Identify the frames by stream offset (0, 3, 6, 0, 3...).
        EXPECT_EQ(3 * (sent_count % 3), writer_->stream_frames()[0]->offset);
        sent_count++;
      }));
  EXPECT_CALL(*send_algorithm_, ShouldSendProbingPacket())
      .WillRepeatedly(Return(true));

  connection_.SendProbingRetransmissions();

  // Ensure that the in-flight has increased.
  const QuicByteCount new_bytes_in_flight =
      connection_.sent_packet_manager().GetBytesInFlight();
  EXPECT_GT(new_bytes_in_flight, old_bytes_in_flight);
}

// Ensure that SendProbingRetransmissions() does not retransmit anything when
// there are no outstanding packets.
TEST_P(QuicConnectionTest,
       SendProbingRetransmissionsFailsWhenNothingToRetransmit) {
  ASSERT_TRUE(connection_.sent_packet_manager().unacked_packets().empty());

  MockQuicConnectionDebugVisitor debug_visitor;
  connection_.set_debug_visitor(&debug_visitor);
  EXPECT_CALL(debug_visitor, OnPacketSent(_, _, _, _)).Times(0);
  EXPECT_CALL(*send_algorithm_, ShouldSendProbingPacket())
      .WillRepeatedly(Return(true));

  connection_.SendProbingRetransmissions();
}

TEST_P(QuicConnectionTest, PingAfterLastRetransmittablePacketAcked) {
  const QuicTime::Delta retransmittable_on_wire_timeout =
      QuicTime::Delta::FromMilliseconds(50);
  connection_.set_retransmittable_on_wire_timeout(
      retransmittable_on_wire_timeout);

  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(visitor_, HasOpenDynamicStreams()).WillRepeatedly(Return(true));

  const char data[] = "data";
  size_t data_size = strlen(data);
  QuicStreamOffset offset = 0;

  // Advance 5ms, send a retransmittable packet to the peer.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_FALSE(connection_.GetRetransmittableOnWireAlarm()->IsSet());
  connection_.SendStreamDataWithString(1, data, offset, NO_FIN);
  offset += data_size;
  EXPECT_FALSE(connection_.GetRetransmittableOnWireAlarm()->IsSet());

  // Advance 5ms, send a second retransmittable packet to the peer.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_FALSE(connection_.GetRetransmittableOnWireAlarm()->IsSet());
  connection_.SendStreamDataWithString(1, data, offset, NO_FIN);
  offset += data_size;
  EXPECT_FALSE(connection_.GetRetransmittableOnWireAlarm()->IsSet());

  // Now receive an ACK of the first packet. This should not set the
  // retransmittable-on-wire alarm since packet 2 is still on the wire.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  QuicAckFrame frame = InitAckFrame({{1, 2}});
  ProcessAckPacket(&frame);
  EXPECT_FALSE(connection_.GetRetransmittableOnWireAlarm()->IsSet());

  // Now receive an ACK of the second packet. This should set the
  // retransmittable-on-wire alarm now that no retransmittable packets are on
  // the wire.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  frame = InitAckFrame({{2, 3}});
  ProcessAckPacket(&frame);
  EXPECT_TRUE(connection_.GetRetransmittableOnWireAlarm()->IsSet());
  EXPECT_EQ(clock_.ApproximateNow() + retransmittable_on_wire_timeout,
            connection_.GetRetransmittableOnWireAlarm()->deadline());

  // Now receive a duplicate ACK of the second packet. This should not update
  // the retransmittable-on-wire alarm.
  QuicTime prev_deadline =
      connection_.GetRetransmittableOnWireAlarm()->deadline();
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  frame = InitAckFrame({{2, 3}});
  ProcessAckPacket(&frame);
  EXPECT_TRUE(connection_.GetRetransmittableOnWireAlarm()->IsSet());
  EXPECT_EQ(prev_deadline,
            connection_.GetRetransmittableOnWireAlarm()->deadline());

  // Simulate the alarm firing and check that a PING is sent.
  EXPECT_CALL(visitor_, SendPing()).WillOnce(Invoke([this]() {
    connection_.SendControlFrame(QuicFrame(QuicPingFrame(1)));
  }));
  connection_.GetRetransmittableOnWireAlarm()->Fire();
  if (GetParam().no_stop_waiting) {
    EXPECT_EQ(2u, writer_->frame_count());
  } else {
    EXPECT_EQ(3u, writer_->frame_count());
  }
  ASSERT_EQ(1u, writer_->ping_frames().size());
}

TEST_P(QuicConnectionTest, NoPingIfRetransmittablePacketSent) {
  const QuicTime::Delta retransmittable_on_wire_timeout =
      QuicTime::Delta::FromMilliseconds(50);
  connection_.set_retransmittable_on_wire_timeout(
      retransmittable_on_wire_timeout);

  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(visitor_, HasOpenDynamicStreams()).WillRepeatedly(Return(true));

  const char data[] = "data";
  size_t data_size = strlen(data);
  QuicStreamOffset offset = 0;

  // Advance 5ms, send a retransmittable packet to the peer.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_FALSE(connection_.GetRetransmittableOnWireAlarm()->IsSet());
  connection_.SendStreamDataWithString(1, data, offset, NO_FIN);
  offset += data_size;
  EXPECT_FALSE(connection_.GetRetransmittableOnWireAlarm()->IsSet());

  // Now receive an ACK of the first packet. This should set the
  // retransmittable-on-wire alarm now that no retransmittable packets are on
  // the wire.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  QuicAckFrame frame = InitAckFrame({{1, 2}});
  ProcessAckPacket(&frame);
  EXPECT_TRUE(connection_.GetRetransmittableOnWireAlarm()->IsSet());
  EXPECT_EQ(clock_.ApproximateNow() + retransmittable_on_wire_timeout,
            connection_.GetRetransmittableOnWireAlarm()->deadline());

  // Before the alarm fires, send another retransmittable packet. This should
  // cancel the retransmittable-on-wire alarm since now there's a
  // retransmittable packet on the wire.
  connection_.SendStreamDataWithString(1, data, offset, NO_FIN);
  offset += data_size;
  EXPECT_FALSE(connection_.GetRetransmittableOnWireAlarm()->IsSet());

  // Now receive an ACK of the second packet. This should set the
  // retransmittable-on-wire alarm now that no retransmittable packets are on
  // the wire.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  frame = InitAckFrame({{2, 3}});
  ProcessAckPacket(&frame);
  EXPECT_TRUE(connection_.GetRetransmittableOnWireAlarm()->IsSet());
  EXPECT_EQ(clock_.ApproximateNow() + retransmittable_on_wire_timeout,
            connection_.GetRetransmittableOnWireAlarm()->deadline());

  // Simulate the alarm firing and check that a PING is sent.
  writer_->Reset();
  EXPECT_CALL(visitor_, SendPing()).WillOnce(Invoke([this]() {
    connection_.SendControlFrame(QuicFrame(QuicPingFrame(1)));
  }));
  connection_.GetRetransmittableOnWireAlarm()->Fire();
  if (GetParam().no_stop_waiting) {
    EXPECT_EQ(2u, writer_->frame_count());
  } else {
    EXPECT_EQ(3u, writer_->frame_count());
  }
  ASSERT_EQ(1u, writer_->ping_frames().size());
}

TEST_P(QuicConnectionTest, OnForwardProgressConfirmed) {
  EXPECT_CALL(visitor_, OnForwardProgressConfirmed()).Times(Exactly(0));
  EXPECT_TRUE(connection_.connected());

  const char data[] = "data";
  size_t data_size = strlen(data);
  QuicStreamOffset offset = 0;

  // Send two packets.
  connection_.SendStreamDataWithString(1, data, offset, NO_FIN);
  offset += data_size;
  connection_.SendStreamDataWithString(1, data, offset, NO_FIN);
  offset += data_size;

  // Ack packet 1. This increases the largest_acked to 1, so
  // OnForwardProgressConfirmed() should be called
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  EXPECT_CALL(visitor_, OnForwardProgressConfirmed());
  QuicAckFrame frame = InitAckFrame({{1, 2}});
  ProcessAckPacket(&frame);

  // Ack packet 1 again. largest_acked remains at 1, so
  // OnForwardProgressConfirmed() should not be called.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  frame = InitAckFrame({{1, 2}});
  ProcessAckPacket(&frame);

  // Ack packet 2. This increases the largest_acked to 2, so
  // OnForwardProgressConfirmed() should be called.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_CALL(*send_algorithm_, OnCongestionEvent(true, _, _, _, _));
  EXPECT_CALL(visitor_, OnForwardProgressConfirmed());
  frame = InitAckFrame({{2, 3}});
  ProcessAckPacket(&frame);
}

TEST_P(QuicConnectionTest, ValidStatelessResetToken) {
  const QuicUint128 kTestToken = 1010101;
  const QuicUint128 kWrongTestToken = 1010100;
  QuicConfig config;
  // No token has been received.
  EXPECT_FALSE(connection_.IsValidStatelessResetToken(kTestToken));

  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _)).Times(2);
  // Token is different from received token.
  QuicConfigPeer::SetReceivedStatelessResetToken(&config, kTestToken);
  connection_.SetFromConfig(config);
  EXPECT_FALSE(connection_.IsValidStatelessResetToken(kWrongTestToken));

  QuicConfigPeer::SetReceivedStatelessResetToken(&config, kTestToken);
  connection_.SetFromConfig(config);
  EXPECT_TRUE(connection_.IsValidStatelessResetToken(kTestToken));
}

TEST_P(QuicConnectionTest, WriteBlockedWithInvalidAck) {
  EXPECT_CALL(visitor_, OnSuccessfulVersionNegotiation(_));
  EXPECT_CALL(visitor_, OnConnectionClosed(QUIC_INVALID_ACK_DATA, _, _));

  BlockOnNextWrite();
  connection_.SendStreamDataWithString(5, "foo", 0, FIN);
  // This causes connection to be closed because packet 1 has not been sent yet.
  QuicAckFrame frame = InitAckFrame(1);
  ProcessAckPacket(1, &frame);
}

TEST_P(QuicConnectionTest, SendMessage) {
  if (connection_.transport_version() <= QUIC_VERSION_44) {
    return;
  }
  QuicString message(connection_.GetLargestMessagePayload() * 2, 'a');
  QuicStringPiece message_data(message);
  {
    QuicConnection::ScopedPacketFlusher flusher(&connection_,
                                                QuicConnection::SEND_ACK);
    connection_.SendStreamData3();
    // Send a message which cannot fit into current open packet, and 2 packets
    // get sent, one contains stream frame, and the other only contains the
    // message frame.
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(2);
    EXPECT_EQ(MESSAGE_STATUS_SUCCESS,
              connection_.SendMessage(
                  1, QuicStringPiece(message_data.data(),
                                     connection_.GetLargestMessagePayload())));
  }
  // Fail to send a message if connection is congestion control blocked.
  EXPECT_CALL(*send_algorithm_, CanSend(_)).WillOnce(Return(false));
  EXPECT_EQ(MESSAGE_STATUS_BLOCKED, connection_.SendMessage(2, "message"));

  // Always fail to send a message which cannot fit into one packet.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(0);
  EXPECT_EQ(
      MESSAGE_STATUS_TOO_LARGE,
      connection_.SendMessage(
          3, QuicStringPiece(message_data.data(),
                             connection_.GetLargestMessagePayload() + 1)));
}

// Test to check that the path challenge/path response logic works
// correctly. This test is only for version-99
TEST_P(QuicConnectionTest, PathChallengeResponse) {
  if (connection_.version().transport_version != QUIC_VERSION_99) {
    return;
  }
  // First check if we can probe from server to client and back
  set_perspective(Perspective::IS_SERVER);
  QuicPacketCreatorPeer::SetSendVersionInPacket(creator_, false);

  // Create and send the probe request (PATH_CHALLENGE frame).
  // SendConnectivityProbingPacket ends up calling
  // TestPacketWriter::WritePacket() which in turns receives and parses the
  // packet by calling framer_.ProcessPacket() -- which in turn calls
  // SimpleQuicFramer::OnPathChallengeFrame(). SimpleQuicFramer saves
  // the packet in writer_->path_challenge_frames()
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  connection_.SendConnectivityProbingPacket(writer_.get(),
                                            connection_.peer_address());
  // Save the random contents of the challenge for later comparison to the
  // response.
  QuicPathFrameBuffer challenge_data =
      writer_->path_challenge_frames().front().data_buffer;

  // Normally, QuicConnection::OnPathChallengeFrame and OnPaddingFrame would be
  // called and it will perform actions to ensure that the rest of the protocol
  // is performed (specifically, call UpdatePacketContent to say that this is a
  // path challenge so that when QuicConnection::OnPacketComplete is called
  // (again, out of the framer), the response is generated).  Simulate those
  // calls so that the right internal state is set up for generating
  // the response.
  EXPECT_TRUE(connection_.OnPathChallengeFrame(
      writer_->path_challenge_frames().front()));
  EXPECT_TRUE(connection_.OnPaddingFrame(writer_->padding_frames().front()));
  // Cause the response to be created and sent. Result is that the response
  // should be stashed in writer's path_response_frames.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _)).Times(1);
  connection_.SendConnectivityProbingResponsePacket(connection_.peer_address());

  // The final check is to ensure that the random data in the response matches
  // the random data from the challenge.
  EXPECT_EQ(0, memcmp(&challenge_data,
                      &(writer_->path_response_frames().front().data_buffer),
                      sizeof(challenge_data)));
}
}  // namespace
}  // namespace test
}  // namespace quic
