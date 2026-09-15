// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_migration_attempt_context.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_handle.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/address_utils.h"
#include "net/quic/quic_chromium_packet_reader.h"
#include "net/quic/quic_chromium_packet_writer.h"
#include "net/socket/socket_test_util.h"
#include "net/test/gtest_util.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/mock_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const IPEndPoint kIpEndPoint{IPAddress::IPv4Localhost(), 443};

class QuicMigrationAttemptContextTest : public ::testing::Test {
 public:
  std::unique_ptr<QuicMigrationAttemptContext> CreateAttemptContext(
      MigrationCause cause,
      base::RepeatingCallback<bool()> is_session_alive =
          base::BindRepeating([]() { return true; })) {
    auto reads = std::make_unique<std::vector<MockRead>>();
    reads->push_back(MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0));
    auto socket_data =
        std::make_unique<SequencedSocketData>(*reads, base::span<MockWrite>());
    socket_factory_.AddSocketDataProvider(socket_data.get());
    socket_reads_.push_back(std::move(reads));
    socket_data_providers_.push_back(std::move(socket_data));

    std::unique_ptr<DatagramClientSocket> socket =
        socket_factory_.CreateDatagramClientSocket(
            DatagramSocket::RANDOM_BIND, handles::kInvalidNetworkHandle,
            NetLog::Get(), NetLogSource());
    EXPECT_THAT(socket->Connect(kIpEndPoint), test::IsOk());
    IPEndPoint peer_address;
    socket->GetPeerAddress(&peer_address);
    auto reader = std::make_unique<QuicChromiumPacketReader>(
        std::move(socket), &clock_, /*visitor=*/nullptr,
        /*yield_after_packets=*/100,
        quic::QuicTime::Delta::FromMilliseconds(100), net_log_);
    reader->StartReading();
    auto writer = std::make_unique<QuicChromiumPacketWriter>(
        reader->socket(),
        base::SingleThreadTaskRunner::GetCurrentDefault().get());
    return std::make_unique<QuicMigrationAttemptContext>(
        cause, /*from_network=*/1, /*target_network=*/2,
        ToQuicSocketAddress(peer_address), std::move(reader), std::move(writer),
        std::move(is_session_alive));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  quic::MockClock clock_;
  NetLogWithSource net_log_;
  MockClientSocketFactory socket_factory_;
  std::vector<std::unique_ptr<std::vector<MockRead>>> socket_reads_;
  std::vector<std::unique_ptr<SequencedSocketData>> socket_data_providers_;
};

TEST_F(QuicMigrationAttemptContextTest, DestructorFallbackSessionDestroyed) {
  base::HistogramTester histogram_tester;
  bool session_alive = true;
  {
    auto context = CreateAttemptContext(
        ON_NETWORK_DISCONNECTED,
        base::BindRepeating([](bool* alive) { return *alive; },
                            base::Unretained(&session_alive)));
    // Simulate session destruction before the migration attempt completes.
    session_alive = false;
    // Context destructs while in flight.
  }

  histogram_tester.ExpectUniqueSample(
      "Net.Quic.Migration.Attempt.Ineligible",
      QuicMigrationAttemptIneligibleReason::kSessionDestroyed, 1);
  histogram_tester.ExpectUniqueSample(
      "Net.Quic.Migration.Attempt.Ineligible.ByTrigger.OnNetworkDisconnected",
      QuicMigrationAttemptIneligibleReason::kSessionDestroyed, 1);
  histogram_tester.ExpectTotalCount("Net.Quic.Migration.Attempt.Eligible", 0);
  histogram_tester.ExpectTotalCount("Net.Quic.Migration.Attempt.FailureReason",
                                    0);
  histogram_tester.ExpectTotalCount("Net.Quic.Migration.Attempt.Superseded", 0);
  histogram_tester.ExpectTotalCount(
      "Net.Quic.Migration.Attempt.UnclassifiedOutcome", 0);
}

TEST_F(QuicMigrationAttemptContextTest, DestructorSessionAlive) {
  base::HistogramTester histogram_tester;
  {
    auto context = CreateAttemptContext(
        ON_NETWORK_DISCONNECTED, base::BindRepeating([]() { return true; }));
    // Context destructs while in flight, but session is still alive.
  }

  histogram_tester.ExpectUniqueSample(
      "Net.Quic.Migration.Attempt.UnclassifiedOutcome", true, 1);
  histogram_tester.ExpectTotalCount("Net.Quic.Migration.Attempt.Eligible", 0);
  histogram_tester.ExpectTotalCount("Net.Quic.Migration.Attempt.FailureReason",
                                    0);
  histogram_tester.ExpectTotalCount("Net.Quic.Migration.Attempt.Ineligible", 0);
  histogram_tester.ExpectTotalCount("Net.Quic.Migration.Attempt.Superseded", 0);
}

TEST_F(QuicMigrationAttemptContextTest, SetIneligibleAndRecordIneligible) {
  base::HistogramTester histogram_tester;
  {
    auto context = CreateAttemptContext(CHANGE_NETWORK_ON_PATH_DEGRADING);
    context->SetIneligible(
        QuicMigrationAttemptIneligibleReason::kSessionBecameIdleDuringProbing);
  }

  QuicMigrationAttemptContext::RecordIneligible(
      ON_WRITE_ERROR, QuicMigrationAttemptIneligibleReason::kDisabledByServer);
  QuicMigrationAttemptContext::RecordIneligible(
      ON_SERVER_PREFERRED_ADDRESS_AVAILABLE,
      QuicMigrationAttemptIneligibleReason::kDisabledByClient);
  QuicMigrationAttemptContext::RecordIneligible(
      ON_NETWORK_DISCONNECTED,
      QuicMigrationAttemptIneligibleReason::kOnlyNonMigratableStreams);
  QuicMigrationAttemptContext::RecordIneligible(
      ON_NETWORK_DISCONNECTED,
      QuicMigrationAttemptIneligibleReason::kHandshakeNotConfirmed);
  QuicMigrationAttemptContext::RecordIneligible(
      ON_WRITE_ERROR, QuicMigrationAttemptIneligibleReason::kTooManyMigrations);
  QuicMigrationAttemptContext::RecordIneligible(
      CHANGE_PORT_ON_PATH_DEGRADING,
      QuicMigrationAttemptIneligibleReason::kTooManyPacketReaders);

  histogram_tester.ExpectBucketCount(
      "Net.Quic.Migration.Attempt.Ineligible",
      QuicMigrationAttemptIneligibleReason::kSessionBecameIdleDuringProbing, 1);
  histogram_tester.ExpectBucketCount(
      "Net.Quic.Migration.Attempt.Ineligible.ByTrigger."
      "ChangeNetworkOnPathDegrading",
      QuicMigrationAttemptIneligibleReason::kSessionBecameIdleDuringProbing, 1);
  histogram_tester.ExpectBucketCount(
      "Net.Quic.Migration.Attempt.Ineligible",
      QuicMigrationAttemptIneligibleReason::kDisabledByServer, 1);
  histogram_tester.ExpectBucketCount(
      "Net.Quic.Migration.Attempt.Ineligible.ByTrigger.OnWriteError",
      QuicMigrationAttemptIneligibleReason::kDisabledByServer, 1);
  histogram_tester.ExpectBucketCount(
      "Net.Quic.Migration.Attempt.Ineligible",
      QuicMigrationAttemptIneligibleReason::kDisabledByClient, 1);
  histogram_tester.ExpectBucketCount(
      "Net.Quic.Migration.Attempt.Ineligible.ByTrigger."
      "OnServerPreferredAddressAvailable",
      QuicMigrationAttemptIneligibleReason::kDisabledByClient, 1);
  histogram_tester.ExpectBucketCount(
      "Net.Quic.Migration.Attempt.Ineligible",
      QuicMigrationAttemptIneligibleReason::kOnlyNonMigratableStreams, 1);
  histogram_tester.ExpectBucketCount(
      "Net.Quic.Migration.Attempt.Ineligible.ByTrigger.OnNetworkDisconnected",
      QuicMigrationAttemptIneligibleReason::kOnlyNonMigratableStreams, 1);
  histogram_tester.ExpectBucketCount(
      "Net.Quic.Migration.Attempt.Ineligible",
      QuicMigrationAttemptIneligibleReason::kHandshakeNotConfirmed, 1);
  histogram_tester.ExpectBucketCount(
      "Net.Quic.Migration.Attempt.Ineligible.ByTrigger.OnNetworkDisconnected",
      QuicMigrationAttemptIneligibleReason::kHandshakeNotConfirmed, 1);
  histogram_tester.ExpectBucketCount(
      "Net.Quic.Migration.Attempt.Ineligible",
      QuicMigrationAttemptIneligibleReason::kTooManyMigrations, 1);
  histogram_tester.ExpectBucketCount(
      "Net.Quic.Migration.Attempt.Ineligible.ByTrigger.OnWriteError",
      QuicMigrationAttemptIneligibleReason::kTooManyMigrations, 1);
  histogram_tester.ExpectBucketCount(
      "Net.Quic.Migration.Attempt.Ineligible",
      QuicMigrationAttemptIneligibleReason::kTooManyPacketReaders, 1);
  histogram_tester.ExpectBucketCount(
      "Net.Quic.Migration.Attempt.Ineligible.ByTrigger."
      "ChangePortOnPathDegrading",
      QuicMigrationAttemptIneligibleReason::kTooManyPacketReaders, 1);
  histogram_tester.ExpectTotalCount("Net.Quic.Migration.Attempt.Ineligible", 7);
  histogram_tester.ExpectTotalCount("Net.Quic.Migration.Attempt.Eligible", 0);
  histogram_tester.ExpectTotalCount("Net.Quic.Migration.Attempt.FailureReason",
                                    0);
  histogram_tester.ExpectTotalCount("Net.Quic.Migration.Attempt.Superseded", 0);
}

TEST_F(QuicMigrationAttemptContextTest, SetSuperseded) {
  base::HistogramTester histogram_tester;
  {
    auto context = CreateAttemptContext(CHANGE_NETWORK_ON_PATH_DEGRADING);
    context->SetSuperseded(QuicMigrationAttemptCause::kOnNetworkDisconnected);
  }

  histogram_tester.ExpectUniqueSample(
      "Net.Quic.Migration.Attempt.Superseded",
      QuicMigrationAttemptCause::kOnNetworkDisconnected, 1);
  histogram_tester.ExpectTotalCount("Net.Quic.Migration.Attempt.Eligible", 0);
  histogram_tester.ExpectTotalCount("Net.Quic.Migration.Attempt.FailureReason",
                                    0);
  histogram_tester.ExpectTotalCount("Net.Quic.Migration.Attempt.Ineligible", 0);
}

TEST_F(QuicMigrationAttemptContextTest, MultiPortPathSkipped) {
  base::HistogramTester histogram_tester;
  {
    auto context = CreateAttemptContext(MULTI_PORT_PATH);
    context->SetSuccess();
  }
  {
    auto context = CreateAttemptContext(MULTI_PORT_PATH);
    // Leaves in flight.
  }

  histogram_tester.ExpectTotalCount("Net.Quic.Migration.Attempt.Eligible", 0);
  histogram_tester.ExpectTotalCount("Net.Quic.Migration.Attempt.FailureReason",
                                    0);
  histogram_tester.ExpectTotalCount("Net.Quic.Migration.Attempt.Ineligible", 0);
  histogram_tester.ExpectTotalCount("Net.Quic.Migration.Attempt.Superseded", 0);
}

TEST_F(QuicMigrationAttemptContextTest, SpuriousOutcome) {
  auto context = CreateAttemptContext(ON_NETWORK_DISCONNECTED);

  base::HistogramTester histogram_tester;
  context->SetSuccess();
  histogram_tester.ExpectTotalCount(
      "Net.Quic.Migration.Attempt.SpuriousOutcome", 0);

  // Calling Set* when an outcome has already been set records into
  // Net.Quic.Migration.Attempt.SpuriousOutcome.
  context->SetFailure(QuicMigrationAttemptFailureReason::kProbeFailed);
  context->SetIneligible(QuicMigrationAttemptIneligibleReason::kIdleSession);
  context->SetSuperseded(QuicMigrationAttemptCause::kOnNetworkDisconnected);
  context->SetSuccess();
  histogram_tester.ExpectUniqueSample(
      "Net.Quic.Migration.Attempt.SpuriousOutcome", true, 4);
}

}  // namespace
}  // namespace net
