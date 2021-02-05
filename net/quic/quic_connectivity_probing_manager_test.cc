// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_connectivity_probing_manager.h"

#include "base/stl_util.h"
#include "base/test/test_mock_time_task_runner.h"
#include "net/log/test_net_log.h"
#include "net/quic/address_utils.h"
#include "net/socket/socket_test_util.h"
#include "net/test/gtest_util.h"
#include "net/third_party/quiche/src/quic/test_tools/mock_clock.h"
#include "net/third_party/quiche/src/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

namespace net {
namespace test {
namespace {

const NetworkChangeNotifier::NetworkHandle testNetworkHandle = 1;

const IPEndPoint kIpEndPoint =
    IPEndPoint(IPAddress::IPv4AllZeros(), quic::test::kTestPort);
const quic::QuicSocketAddress testPeerAddress =
    ToQuicSocketAddress(kIpEndPoint);

const IPEndPoint newIpEndPoint =
    IPEndPoint(IPAddress::IPv4AllZeros(), quic::test::kTestPort + 1);
const quic::QuicSocketAddress newPeerAddress =
    ToQuicSocketAddress(newIpEndPoint);
}  // anonymous namespace

class MockQuicChromiumClientSession
    : public QuicConnectivityProbingManager::Delegate,
      public QuicChromiumPacketReader::Visitor {
 public:
  MockQuicChromiumClientSession()
      : probed_network_(NetworkChangeNotifier::kInvalidNetworkHandle),
        is_successfully_probed_(false) {}
  ~MockQuicChromiumClientSession() override {}

  // QuicChromiumPacketReader::Visitor interface.
  MOCK_METHOD(bool,
              OnReadError,
              (int result, const DatagramClientSocket* socket),
              (override));

  MOCK_METHOD(bool,
              OnPacket,
              (const quic::QuicReceivedPacket& packet,
               const quic::QuicSocketAddress& local_address,
               const quic::QuicSocketAddress& peer_address),
              (override));

  MOCK_METHOD(void,
              OnProbeFailed,
              (NetworkChangeNotifier::NetworkHandle network,
               const quic::QuicSocketAddress& peer_address),
              (override));

  MOCK_METHOD(bool,
              OnSendConnectivityProbingPacket,
              (QuicChromiumPacketWriter * writer,
               const quic::QuicSocketAddress& peer_address),
              (override));

  void OnProbeSucceeded(
      NetworkChangeNotifier::NetworkHandle network,
      const quic::QuicSocketAddress& peer_address,
      const quic::QuicSocketAddress& self_address,
      std::unique_ptr<DatagramClientSocket> socket,
      std::unique_ptr<QuicChromiumPacketWriter> writer,
      std::unique_ptr<QuicChromiumPacketReader> reader) override {
    is_successfully_probed_ = true;
    probed_network_ = network;
    probed_peer_address_ = peer_address;
    probed_self_address_ = self_address;
  }

  bool IsProbedPathMatching(NetworkChangeNotifier::NetworkHandle network,
                            const quic::QuicSocketAddress& peer_address,
                            const quic::QuicSocketAddress& self_address) const {
    if (!is_successfully_probed_)
      return false;

    return probed_network_ == network && probed_peer_address_ == peer_address &&
           probed_self_address_ == self_address;
  }

  bool is_successfully_probed() const { return is_successfully_probed_; }

 private:
  NetworkChangeNotifier::NetworkHandle probed_network_;
  quic::QuicSocketAddress probed_peer_address_;
  quic::QuicSocketAddress probed_self_address_;
  bool is_successfully_probed_;

  DISALLOW_COPY_AND_ASSIGN(MockQuicChromiumClientSession);
};

class QuicConnectivityProbingManagerTest : public ::testing::Test {
 public:
  QuicConnectivityProbingManagerTest()
      : test_task_runner_(new base::TestMockTimeTaskRunner()),
        test_task_runner_context_(test_task_runner_),
        probing_manager_(&session_, test_task_runner_.get()),
        default_read_(new MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)),
        socket_data_(
            new SequencedSocketData(base::make_span(default_read_.get(), 1),
                                    base::span<MockWrite>())) {
    socket_factory_.AddSocketDataProvider(socket_data_.get());
    // Create a connected socket for probing.
    socket_ = socket_factory_.CreateDatagramClientSocket(
        DatagramSocket::DEFAULT_BIND, &net_log_, NetLogSource());
    EXPECT_THAT(socket_->Connect(kIpEndPoint), IsOk());
    IPEndPoint self_address;
    socket_->GetLocalAddress(&self_address);
    self_address_ = ToQuicSocketAddress(self_address);
    // Create packet writer and reader for probing.
    writer_.reset(
        new QuicChromiumPacketWriter(socket_.get(), test_task_runner_.get()));
    reader_ = std::make_unique<QuicChromiumPacketReader>(
        socket_.get(), &clock_, &session_, kQuicYieldAfterPacketsRead,
        quic::QuicTime::Delta::FromMilliseconds(
            kQuicYieldAfterDurationMilliseconds),
        bound_test_net_log_.bound());
  }

 protected:
  // All tests will run inside the scope of |test_task_runner_|.
  scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner_;
  base::TestMockTimeTaskRunner::ScopedContext test_task_runner_context_;
  MockQuicChromiumClientSession session_;
  QuicConnectivityProbingManager probing_manager_;

  std::unique_ptr<MockRead> default_read_;
  std::unique_ptr<SequencedSocketData> socket_data_;

  std::unique_ptr<DatagramClientSocket> socket_;
  std::unique_ptr<QuicChromiumPacketWriter> writer_;
  std::unique_ptr<QuicChromiumPacketReader> reader_;
  quic::QuicSocketAddress self_address_;

  quic::MockClock clock_;
  MockClientSocketFactory socket_factory_;
  RecordingTestNetLog net_log_;
  RecordingBoundTestNetLog bound_test_net_log_;

  DISALLOW_COPY_AND_ASSIGN(QuicConnectivityProbingManagerTest);
};

TEST_F(QuicConnectivityProbingManagerTest, ReceiveProbingResponseOnSamePath) {
  EXPECT_FALSE(session_.is_successfully_probed());
  int initial_timeout_ms = 100;

  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .WillOnce(Return(true));

  // Target probing path: <testNetworkHandle, testPeerAddress>.
  probing_manager_.StartProbing(
      testNetworkHandle, testPeerAddress, std::move(socket_),
      std::move(writer_), std::move(reader_),
      base::TimeDelta::FromMilliseconds(initial_timeout_ms),
      bound_test_net_log_.bound());
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // Fast forward initial_timeout_ms, timeout the first connectivity probing
  // packet, cause another probing packet to be sent with timeout set to
  // 2 * initial_timeout_ms.
  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .WillOnce(Return(true));

  test_task_runner_->FastForwardBy(
      base::TimeDelta::FromMilliseconds(initial_timeout_ms));
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // Fast forward initial_timeout_ms, should be no-op.
  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .Times(0);
  test_task_runner_->FastForwardBy(
      base::TimeDelta::FromMilliseconds(initial_timeout_ms));
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());
  EXPECT_FALSE(session_.is_successfully_probed());

  // Notify the manager a connectivity probing packet is received from
  // testPeerAddress to |self_address_|, manager should decalre probing as
  // successful, notify delegate and will no longer send connectivity probing
  // packet for this probing.
  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .Times(0);
  probing_manager_.OnPacketReceived(self_address_, testPeerAddress, true);
  EXPECT_TRUE(session_.is_successfully_probed());
  EXPECT_TRUE(session_.IsProbedPathMatching(testNetworkHandle, testPeerAddress,
                                            self_address_));
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // Verify there's nothing to send.
  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .Times(0);
  test_task_runner_->FastForwardBy(
      base::TimeDelta::FromMilliseconds(initial_timeout_ms));
  EXPECT_EQ(0u, test_task_runner_->GetPendingTaskCount());
}

TEST_F(QuicConnectivityProbingManagerTest,
       ReceiveProbingResponseOnDifferentPath) {
  EXPECT_FALSE(session_.is_successfully_probed());
  int initial_timeout_ms = 100;

  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .WillOnce(Return(true));

  // Target probing path: <testNetworkHandle, testPeerAddress>.
  probing_manager_.StartProbing(
      testNetworkHandle, testPeerAddress, std::move(socket_),
      std::move(writer_), std::move(reader_),
      base::TimeDelta::FromMilliseconds(initial_timeout_ms),
      bound_test_net_log_.bound());
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // Fast forward initial_timeout_ms, timeout the first connectivity probing
  // packet, cause another probing packet to be sent with timeout set to
  // 2 * initial_timeout_ms.
  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .WillOnce(Return(true));
  test_task_runner_->FastForwardBy(
      base::TimeDelta::FromMilliseconds(initial_timeout_ms));
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // Fast forward initial_timeout_ms, should be no-op.
  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .Times(0);
  test_task_runner_->FastForwardBy(
      base::TimeDelta::FromMilliseconds(initial_timeout_ms));
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // Notify the manager a connectivity probing packet is received from
  // testPeerAddress to a different self address, manager should ignore the
  // probing response and continue waiting.
  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .Times(0);
  probing_manager_.OnPacketReceived(quic::QuicSocketAddress(), testPeerAddress,
                                    true);
  EXPECT_FALSE(session_.is_successfully_probed());
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // Fast forward another initial_timeout_ms, another probing packet will be
  // sent.
  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .WillOnce(Return(true));
  test_task_runner_->FastForwardBy(
      base::TimeDelta::FromMilliseconds(initial_timeout_ms));
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // Finally receive the probing response on the same path.
  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .Times(0);
  probing_manager_.OnPacketReceived(self_address_, testPeerAddress, true);
  EXPECT_TRUE(session_.is_successfully_probed());
  EXPECT_TRUE(session_.IsProbedPathMatching(testNetworkHandle, testPeerAddress,
                                            self_address_));
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // Verify there's nothing to send.
  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .Times(0);
  test_task_runner_->RunUntilIdle();
}

TEST_F(QuicConnectivityProbingManagerTest,
       ReceiveProbingResponseOnDifferentPort) {
  EXPECT_FALSE(session_.is_successfully_probed());
  int initial_timeout_ms = 100;

  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .WillOnce(Return(true));

  // Target probing path: <NetworkChangeNotifier::kInvalidNetworkHandle,
  //                       testPeerAddress>.
  probing_manager_.StartProbing(
      NetworkChangeNotifier::kInvalidNetworkHandle, testPeerAddress,
      std::move(socket_), std::move(writer_), std::move(reader_),
      base::TimeDelta::FromMilliseconds(initial_timeout_ms),
      bound_test_net_log_.bound());
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // Fast forward initial_timeout_ms, timeout the first connectivity probing
  // packet, cause another probing packet to be sent with timeout set to
  // 2 * initial_timeout_ms.
  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .WillOnce(Return(true));
  test_task_runner_->FastForwardBy(
      base::TimeDelta::FromMilliseconds(initial_timeout_ms));
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // Fast forward initial_timeout_ms, should be no-op.
  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .Times(0);
  test_task_runner_->FastForwardBy(
      base::TimeDelta::FromMilliseconds(initial_timeout_ms));
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // Notify the manager a connectivity probing packet is received from
  // testPeerAddress to a different self address (which only differs in the
  // port), manager should ignore the probing response and continue waiting.
  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .Times(0);
  uint16_t different_port = self_address_.port() + 1;
  quic::QuicSocketAddress different_self_address(self_address_.host(),
                                                 different_port);
  probing_manager_.OnPacketReceived(different_self_address, testPeerAddress,
                                    true);
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());
  // Verify that session's probed network is still not valid.
  EXPECT_FALSE(session_.is_successfully_probed());

  // Fast forward another initial_timeout_ms, another probing packet will be
  // sent.
  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .WillOnce(Return(true));
  test_task_runner_->FastForwardBy(
      base::TimeDelta::FromMilliseconds(initial_timeout_ms));
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // Finally receive the probing response on the same self address and peer
  // address.
  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .Times(0);
  probing_manager_.OnPacketReceived(self_address_, testPeerAddress, true);
  // Verify that session's probed network is not valid yet.
  EXPECT_TRUE(session_.is_successfully_probed());
  EXPECT_TRUE(session_.IsProbedPathMatching(
      NetworkChangeNotifier::kInvalidNetworkHandle, testPeerAddress,
      self_address_));
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // Verify there's nothing to send.
  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .Times(0);
  test_task_runner_->RunUntilIdle();
}

TEST_F(QuicConnectivityProbingManagerTest, RetryProbingWithExponentailBackoff) {
  int initial_timeout_ms = 100;

  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .WillOnce(Return(true));
  probing_manager_.StartProbing(
      testNetworkHandle, testPeerAddress, std::move(socket_),
      std::move(writer_), std::move(reader_),
      base::TimeDelta::FromMilliseconds(initial_timeout_ms),
      bound_test_net_log_.bound());
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // For expential backoff, this will try to resend: 100ms, 200ms, 400ms, 800ms,
  // 1600ms.
  for (int retry_count = 0; retry_count < 4; retry_count++) {
    EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
        .WillOnce(Return(true));
    int timeout_ms = (1 << retry_count) * initial_timeout_ms;
    test_task_runner_->FastForwardBy(
        base::TimeDelta::FromMilliseconds(timeout_ms));
    EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());
  }

  // Move forward another 1600ms, expect probing manager will no longer send any
  // connectivity probing packet but declare probing as failed .
  EXPECT_CALL(session_, OnProbeFailed(testNetworkHandle, testPeerAddress))
      .Times(1);
  int timeout_ms = (1 << 4) * initial_timeout_ms;
  test_task_runner_->FastForwardBy(
      base::TimeDelta::FromMilliseconds(timeout_ms));
  EXPECT_EQ(0u, test_task_runner_->GetPendingTaskCount());
}

TEST_F(QuicConnectivityProbingManagerTest, ProbingReceivedStatelessReset) {
  int initial_timeout_ms = 100;
  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .WillOnce(Return(true));
  probing_manager_.StartProbing(
      testNetworkHandle, testPeerAddress, std::move(socket_),
      std::move(writer_), std::move(reader_),
      base::TimeDelta::FromMilliseconds(initial_timeout_ms),
      bound_test_net_log_.bound());
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // Fast forward initial_timeout_ms, timeout the first connectivity probing
  // packet, cause another probing packet to be sent with timeout set to
  // 2 * initial_timeout_ms.
  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .WillOnce(Return(true));
  test_task_runner_->FastForwardBy(
      base::TimeDelta::FromMilliseconds(initial_timeout_ms));
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // Fast forward initial_timeout_ms, should be no-op.
  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .Times(0);
  test_task_runner_->FastForwardBy(
      base::TimeDelta::FromMilliseconds(initial_timeout_ms));
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  EXPECT_CALL(session_, OnProbeFailed(testNetworkHandle, testPeerAddress))
      .Times(1);
  EXPECT_TRUE(
      probing_manager_.ValidateStatelessReset(self_address_, testPeerAddress));
  EXPECT_FALSE(session_.is_successfully_probed());
  EXPECT_FALSE(
      probing_manager_.IsUnderProbing(testNetworkHandle, testPeerAddress));
  test_task_runner_->RunUntilIdle();
}

TEST_F(QuicConnectivityProbingManagerTest,
       StatelessResetReceivedAfterProbingCancelled) {
  int initial_timeout_ms = 100;

  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .WillOnce(Return(true));
  probing_manager_.StartProbing(
      testNetworkHandle, testPeerAddress, std::move(socket_),
      std::move(writer_), std::move(reader_),
      base::TimeDelta::FromMilliseconds(initial_timeout_ms),
      bound_test_net_log_.bound());
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // Fast forward initial_timeout_ms, timeout the first connectivity probing
  // packet, cause another probing packet to be sent with timeout set to
  // 2 * initial_timeout_ms.
  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .WillOnce(Return(true));
  test_task_runner_->FastForwardBy(
      base::TimeDelta::FromMilliseconds(initial_timeout_ms));
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // Fast forward initial_timeout_ms, should be no-op.
  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .Times(0);
  test_task_runner_->FastForwardBy(
      base::TimeDelta::FromMilliseconds(initial_timeout_ms));
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // Request cancel probing, manager will no longer send connectivity probes.
  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, _)).Times(0);
  EXPECT_CALL(session_, OnProbeFailed(_, _)).Times(0);
  probing_manager_.CancelProbing(testNetworkHandle, testPeerAddress);
  EXPECT_FALSE(
      probing_manager_.IsUnderProbing(testNetworkHandle, testPeerAddress));

  // Verify that the probing manager is still able to verify STATELESS_RESET
  // received on the previous probing path.
  EXPECT_TRUE(
      probing_manager_.ValidateStatelessReset(self_address_, testPeerAddress));
  EXPECT_FALSE(session_.is_successfully_probed());
  EXPECT_FALSE(
      probing_manager_.IsUnderProbing(testNetworkHandle, testPeerAddress));
  test_task_runner_->RunUntilIdle();
}

TEST_F(QuicConnectivityProbingManagerTest, CancelProbing) {
  int initial_timeout_ms = 100;

  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .WillOnce(Return(true));
  probing_manager_.StartProbing(
      testNetworkHandle, testPeerAddress, std::move(socket_),
      std::move(writer_), std::move(reader_),
      base::TimeDelta::FromMilliseconds(initial_timeout_ms),
      bound_test_net_log_.bound());
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // Fast forward initial_timeout_ms, timeout the first connectivity probing
  // packet, cause another probing packet to be sent with timeout set to
  // 2 * initial_timeout_ms.
  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .WillOnce(Return(true));
  test_task_runner_->FastForwardBy(
      base::TimeDelta::FromMilliseconds(initial_timeout_ms));
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // Fast forward initial_timeout_ms, should be no-op.
  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .Times(0);
  test_task_runner_->FastForwardBy(
      base::TimeDelta::FromMilliseconds(initial_timeout_ms));
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // Request cancel probing, manager will no longer send connectivity probes.
  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, _)).Times(0);
  EXPECT_CALL(session_, OnProbeFailed(_, _)).Times(0);
  probing_manager_.CancelProbing(testNetworkHandle, testPeerAddress);
  EXPECT_FALSE(
      probing_manager_.IsUnderProbing(testNetworkHandle, testPeerAddress));

  test_task_runner_->RunUntilIdle();
}

TEST_F(QuicConnectivityProbingManagerTest, DoNotCancelProbing) {
  int initial_timeout_ms = 100;

  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .WillOnce(Return(true));
  // Start probing |testPeerAddress| on |testNetworkHandle|.
  probing_manager_.StartProbing(
      testNetworkHandle, testPeerAddress, std::move(socket_),
      std::move(writer_), std::move(reader_),
      base::TimeDelta::FromMilliseconds(initial_timeout_ms),
      bound_test_net_log_.bound());
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // Request cancel probing for |newPeerAddress| on |testNetworkHandle| doesn't
  // affect the exisiting probing.
  probing_manager_.CancelProbing(testNetworkHandle, newPeerAddress);
  EXPECT_TRUE(
      probing_manager_.IsUnderProbing(testNetworkHandle, testPeerAddress));
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  for (int retry_count = 0; retry_count < 4; retry_count++) {
    EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
        .WillOnce(Return(true));
    int timeout_ms = (1 << retry_count) * initial_timeout_ms;
    test_task_runner_->FastForwardBy(
        base::TimeDelta::FromMilliseconds(timeout_ms));
    EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());
  }

  EXPECT_CALL(session_, OnProbeFailed(testNetworkHandle, testPeerAddress))
      .Times(1);
  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, _)).Times(0);
  int timeout_ms = (1 << 4) * initial_timeout_ms;
  test_task_runner_->FastForwardBy(
      base::TimeDelta::FromMilliseconds(timeout_ms));
  EXPECT_EQ(0u, test_task_runner_->GetPendingTaskCount());
}

TEST_F(QuicConnectivityProbingManagerTest, ProbingWriterError) {
  int initial_timeout_ms = 100;

  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .WillOnce(Return(true));
  QuicChromiumPacketWriter* writer_ptr = writer_.get();
  probing_manager_.StartProbing(
      testNetworkHandle, testPeerAddress, std::move(socket_),
      std::move(writer_), std::move(reader_),
      base::TimeDelta::FromMilliseconds(initial_timeout_ms),
      bound_test_net_log_.bound());
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // Fast forward initial_timeout_ms, timeout the first connectivity probing
  // packet, cause another probing packet to be sent with timeout set to
  // 2 * initial_timeout_ms.
  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .WillOnce(Return(true));
  test_task_runner_->FastForwardBy(
      base::TimeDelta::FromMilliseconds(initial_timeout_ms));
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // Fast forward initial_timeout_ms, should be no-op.
  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .Times(0);
  test_task_runner_->FastForwardBy(
      base::TimeDelta::FromMilliseconds(initial_timeout_ms));
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // Probing packet writer received an write error, notifies manager to handle
  // write error. Manager will notify session of the probe failure, cancel
  // probing to prevent future connectivity probing packet to be sent.
  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, _)).Times(0);
  EXPECT_CALL(session_, OnProbeFailed(testNetworkHandle, testPeerAddress))
      .Times(1);
  writer_ptr->OnWriteComplete(ERR_CONNECTION_CLOSED);
  test_task_runner_->FastForwardBy(
      base::TimeDelta::FromMilliseconds(initial_timeout_ms));
  EXPECT_EQ(0u, test_task_runner_->GetPendingTaskCount());
}

TEST_F(QuicConnectivityProbingManagerTest,
       ProbeServerPreferredAddressSucceeded) {
  int initial_timeout_ms = 100;

  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .WillOnce(Return(true));
  // A probe for server preferred address is usually initiated with an
  // invalid network handle passed in.
  probing_manager_.StartProbing(
      NetworkChangeNotifier::kInvalidNetworkHandle, testPeerAddress,
      std::move(socket_), std::move(writer_), std::move(reader_),
      base::TimeDelta::FromMilliseconds(initial_timeout_ms),
      bound_test_net_log_.bound());
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .WillOnce(Return(true));

  test_task_runner_->FastForwardBy(
      base::TimeDelta::FromMilliseconds(initial_timeout_ms));
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // Fast forward initial_timeout_ms, should be no-op.
  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .Times(0);
  test_task_runner_->FastForwardBy(
      base::TimeDelta::FromMilliseconds(initial_timeout_ms));
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // Notify the manager a connectivity probing packet is received from
  // testPeerAddress to |self_address_|, manager should decalre probing as
  // successful, notify delegate and will no longer send connectivity probes.
  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .Times(0);
  probing_manager_.OnPacketReceived(self_address_, testPeerAddress, true);

  // Verify that session marked <kInvalidNetworkHandle, testPeerAddress> as
  // successfully probed.
  EXPECT_TRUE(session_.is_successfully_probed());
  EXPECT_TRUE(session_.IsProbedPathMatching(
      NetworkChangeNotifier::kInvalidNetworkHandle, testPeerAddress,
      self_address_));
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .Times(0);
  test_task_runner_->FastForwardBy(
      base::TimeDelta::FromMilliseconds(initial_timeout_ms));
  EXPECT_EQ(0u, test_task_runner_->GetPendingTaskCount());
}

TEST_F(QuicConnectivityProbingManagerTest, ProbeServerPreferredAddressFailed) {
  int initial_timeout_ms = 100;

  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .WillOnce(Return(true));
  QuicChromiumPacketWriter* writer_ptr = writer_.get();
  // A probe for server preferred address is usually initiated with an
  // invalid network handle passed in.
  probing_manager_.StartProbing(
      NetworkChangeNotifier::kInvalidNetworkHandle, testPeerAddress,
      std::move(socket_), std::move(writer_), std::move(reader_),
      base::TimeDelta::FromMilliseconds(initial_timeout_ms),
      bound_test_net_log_.bound());
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .WillOnce(Return(true));
  test_task_runner_->FastForwardBy(
      base::TimeDelta::FromMilliseconds(initial_timeout_ms));
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // Fast forward initial_timeout_ms, should be no-op.
  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, testPeerAddress))
      .Times(0);
  test_task_runner_->FastForwardBy(
      base::TimeDelta::FromMilliseconds(initial_timeout_ms));
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // Probing packet writer received an write error, notifies manager to handle
  // write error. Manager will notify session of the probe failure, cancel
  // probing to prevent future connectivity probing packet to be sent.
  EXPECT_CALL(session_, OnSendConnectivityProbingPacket(_, _)).Times(0);
  EXPECT_CALL(session_,
              OnProbeFailed(NetworkChangeNotifier::kInvalidNetworkHandle,
                            testPeerAddress))
      .Times(1);
  writer_ptr->OnWriteComplete(ERR_CONNECTION_CLOSED);
  test_task_runner_->FastForwardBy(
      base::TimeDelta::FromMilliseconds(initial_timeout_ms));
  EXPECT_EQ(0u, test_task_runner_->GetPendingTaskCount());
}

}  // namespace test
}  // namespace net
