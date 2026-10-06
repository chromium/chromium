// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_migration_attempt_context.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gtest_util.h"
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
#include "net/quic/quic_socket_config_step.h"
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
      QuicMigrationAttemptCause cause,
      base::RepeatingCallback<bool()> is_session_alive =
          base::BindRepeating([]() { return true; }),
      bool is_google_host = false) {
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
        std::move(is_session_alive), is_google_host);
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
        QuicMigrationAttemptCause::kOnNetworkDisconnected,
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
    auto context =
        CreateAttemptContext(QuicMigrationAttemptCause::kOnNetworkDisconnected,
                             base::BindRepeating([]() { return true; }));
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
    auto context = CreateAttemptContext(
        QuicMigrationAttemptCause::kChangeNetworkOnPathDegrading);
    context->SetIneligible(
        QuicMigrationAttemptIneligibleReason::kSessionBecameIdleDuringProbing);
  }

  QuicMigrationAttemptContext::RecordIneligible(
      QuicMigrationAttemptCause::kOnWriteError,
      QuicMigrationAttemptIneligibleReason::kDisabledByServer);
  QuicMigrationAttemptContext::RecordIneligible(
      QuicMigrationAttemptCause::kOnServerPreferredAddressAvailable,
      QuicMigrationAttemptIneligibleReason::kDisabledByClient);
  QuicMigrationAttemptContext::RecordIneligible(
      QuicMigrationAttemptCause::kOnNetworkDisconnected,
      QuicMigrationAttemptIneligibleReason::kOnlyNonMigratableStreams);
  QuicMigrationAttemptContext::RecordIneligible(
      QuicMigrationAttemptCause::kOnNetworkDisconnected,
      QuicMigrationAttemptIneligibleReason::kHandshakeNotConfirmed);
  QuicMigrationAttemptContext::RecordIneligible(
      QuicMigrationAttemptCause::kOnWriteError,
      QuicMigrationAttemptIneligibleReason::kTooManyMigrations);
  QuicMigrationAttemptContext::RecordIneligible(
      QuicMigrationAttemptCause::kChangePortOnPathDegrading,
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
    auto context = CreateAttemptContext(
        QuicMigrationAttemptCause::kChangeNetworkOnPathDegrading);
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
    auto context =
        CreateAttemptContext(QuicMigrationAttemptCause::kMultiPortPath);
    context->SetSuccess();
  }
  {
    auto context =
        CreateAttemptContext(QuicMigrationAttemptCause::kMultiPortPath);
    // Leaves in flight.
  }

  histogram_tester.ExpectTotalCount("Net.Quic.Migration.Attempt.Eligible", 0);
  histogram_tester.ExpectTotalCount("Net.Quic.Migration.Attempt.FailureReason",
                                    0);
  histogram_tester.ExpectTotalCount("Net.Quic.Migration.Attempt.Ineligible", 0);
  histogram_tester.ExpectTotalCount("Net.Quic.Migration.Attempt.Superseded", 0);
}

TEST_F(QuicMigrationAttemptContextTest, SpuriousOutcome) {
  auto context =
      CreateAttemptContext(QuicMigrationAttemptCause::kOnNetworkDisconnected);

  base::HistogramTester histogram_tester;
  context->SetSuccess();
  histogram_tester.ExpectTotalCount(
      "Net.Quic.Migration.Attempt.SpuriousOutcome", 0);

  // Calling Set* when an outcome has already been set records into
  // Net.Quic.Migration.Attempt.SpuriousOutcome.
  context->SetFailure(QuicMigrationAttemptFailureReason::kProbeUnknownFailure);
  context->SetSocketConfigFailure(QuicSocketConfigStep::kConnect, ERR_FAILED);
  context->SetIneligible(QuicMigrationAttemptIneligibleReason::kIdleSession);
  context->SetSuperseded(QuicMigrationAttemptCause::kOnNetworkDisconnected);
  context->SetSuccess();
  histogram_tester.ExpectUniqueSample(
      "Net.Quic.Migration.Attempt.SpuriousOutcome", true, 5);
}

TEST_F(QuicMigrationAttemptContextTest, SetSuccess) {
  base::HistogramTester histogram_tester;
  {
    auto context =
        CreateAttemptContext(QuicMigrationAttemptCause::kOnNetworkMadeDefault);
    context->SetSuccess();
  }

  histogram_tester.ExpectUniqueSample("Net.Quic.Migration.Attempt.Eligible",
                                      true, 1);
  histogram_tester.ExpectUniqueSample(
      "Net.Quic.Migration.Attempt.Eligible.ByTrigger.OnNetworkMadeDefault",
      true, 1);
  histogram_tester.ExpectTotalCount("Net.Quic.Migration.Attempt.FailureReason",
                                    0);
  histogram_tester.ExpectTotalCount("Net.Quic.Migration.Attempt.Ineligible", 0);
  histogram_tester.ExpectTotalCount("Net.Quic.Migration.Attempt.Superseded", 0);
  histogram_tester.ExpectTotalCount(
      "Net.Quic.Migration.Attempt.SpuriousOutcome", 0);
  histogram_tester.ExpectTotalCount(
      "Net.Quic.Migration.Attempt.UnclassifiedOutcome", 0);
}

TEST_F(QuicMigrationAttemptContextTest, SetFailure) {
  struct TestCase {
    QuicMigrationAttemptCause cause;
    const char* trigger_name;
    QuicMigrationAttemptFailureReason failure_reason;
  } test_cases[] = {
      {QuicMigrationAttemptCause::kOnNetworkMadeDefault, "OnNetworkMadeDefault",
       QuicMigrationAttemptFailureReason::kProbeTimeout},
      {QuicMigrationAttemptCause::kOnNetworkDisconnected,
       "OnNetworkDisconnected",
       QuicMigrationAttemptFailureReason::kNoUnusedConnectionId},
      {QuicMigrationAttemptCause::kChangeNetworkOnPathDegrading,
       "ChangeNetworkOnPathDegrading",
       QuicMigrationAttemptFailureReason::kStatelessReset},
      {QuicMigrationAttemptCause::kOnWriteError, "OnWriteError",
       QuicMigrationAttemptFailureReason::kProbeUnknownFailure},
      {QuicMigrationAttemptCause::kChangePortOnPathDegrading,
       "ChangePortOnPathDegrading",
       QuicMigrationAttemptFailureReason::kProbeWriteError},
  };

  base::HistogramTester histogram_tester;
  for (const auto& test_case : test_cases) {
    auto context = CreateAttemptContext(test_case.cause);
    context->SetFailure(test_case.failure_reason);
    ASSERT_NE(nullptr, context->failure_reason());
    EXPECT_EQ(test_case.failure_reason, *context->failure_reason());
    EXPECT_EQ(nullptr, context->socket_config_failure_details());
  }

  histogram_tester.ExpectUniqueSample("Net.Quic.Migration.Attempt.Eligible",
                                      false, std::size(test_cases));
  histogram_tester.ExpectBucketCount(
      "Net.Quic.Migration.Attempt.Eligible.ByTrigger.OnWriteError", false, 1);
  histogram_tester.ExpectBucketCount(
      "Net.Quic.Migration.Attempt.Eligible.ByTrigger.OnNetworkMadeDefault",
      false, 1);
  histogram_tester.ExpectBucketCount(
      "Net.Quic.Migration.Attempt.Eligible.ByTrigger.OnNetworkDisconnected",
      false, 1);
  histogram_tester.ExpectBucketCount(
      "Net.Quic.Migration.Attempt.Eligible.ByTrigger."
      "ChangeNetworkOnPathDegrading",
      false, 1);
  histogram_tester.ExpectBucketCount(
      "Net.Quic.Migration.Attempt.Eligible.ByTrigger."
      "ChangePortOnPathDegrading",
      false, 1);

  for (const auto& test_case : test_cases) {
    histogram_tester.ExpectBucketCount(
        "Net.Quic.Migration.Attempt.FailureReason", test_case.failure_reason,
        1);
    histogram_tester.ExpectBucketCount(
        base::StrCat({"Net.Quic.Migration.Attempt.FailureReason.ByTrigger.",
                      test_case.trigger_name}),
        test_case.failure_reason, 1);
  }

  histogram_tester.ExpectTotalCount("Net.Quic.Migration.Attempt.Ineligible", 0);
  histogram_tester.ExpectTotalCount("Net.Quic.Migration.Attempt.Superseded", 0);
  histogram_tester.ExpectTotalCount(
      "Net.Quic.Migration.Attempt.SpuriousOutcome", 0);
  histogram_tester.ExpectTotalCount(
      "Net.Quic.Migration.Attempt.UnclassifiedOutcome", 0);
}

TEST_F(QuicMigrationAttemptContextTest, SetSocketConfigFailure) {
  struct TestCase {
    QuicMigrationAttemptCause cause;
    const char* trigger_name;
    QuicSocketConfigStep step;
    const char* step_name;
    int net_error;
  } test_cases[] = {
      {QuicMigrationAttemptCause::kOnNetworkMadeDefault, "OnNetworkMadeDefault",
       QuicSocketConfigStep::kConnect, "Connect", ERR_ADDRESS_UNREACHABLE},
      {QuicMigrationAttemptCause::kOnNetworkDisconnected,
       "OnNetworkDisconnected", QuicSocketConfigStep::kSetReceiveBufferSize,
       "SetReceiveBufferSize", ERR_FAILED},
      {QuicMigrationAttemptCause::kChangeNetworkOnPathDegrading,
       "ChangeNetworkOnPathDegrading", QuicSocketConfigStep::kSetDoNotFragment,
       "SetDoNotFragment", ERR_ACCESS_DENIED},
      {QuicMigrationAttemptCause::kChangePortOnPathDegrading,
       "ChangePortOnPathDegrading", QuicSocketConfigStep::kSetReceiveEcn,
       "SetReceiveEcn", ERR_NETWORK_CHANGED},
      {QuicMigrationAttemptCause::kOnWriteError, "OnWriteError",
       QuicSocketConfigStep::kSetSendBufferSize, "SetSendBufferSize",
       ERR_UNEXPECTED},
  };

  base::HistogramTester histogram_tester;
  for (const auto& test_case : test_cases) {
    auto context = CreateAttemptContext(test_case.cause);
    context->SetSocketConfigFailure(test_case.step, test_case.net_error);
    ASSERT_NE(nullptr, context->socket_config_failure_details());
    EXPECT_EQ(test_case.step, context->socket_config_failure_details()->step);
    EXPECT_EQ(test_case.net_error,
              context->socket_config_failure_details()->net_error);
    EXPECT_EQ(nullptr, context->failure_reason());
  }

  histogram_tester.ExpectUniqueSample("Net.Quic.Migration.Attempt.Eligible",
                                      false, std::size(test_cases));
  histogram_tester.ExpectUniqueSample(
      "Net.Quic.Migration.Attempt.FailureReason",
      QuicMigrationAttemptFailureReason::kSocketConfigFailed,
      std::size(test_cases));

  for (const auto& test_case : test_cases) {
    histogram_tester.ExpectBucketCount(
        base::StrCat({"Net.Quic.Migration.Attempt.FailureReason.ByTrigger.",
                      test_case.trigger_name}),
        QuicMigrationAttemptFailureReason::kSocketConfigFailed, 1);
    histogram_tester.ExpectBucketCount(
        "Net.Quic.Migration.Attempt.SocketConfigError.Step", test_case.step, 1);
    histogram_tester.ExpectBucketCount(
        base::StrCat(
            {"Net.Quic.Migration.Attempt.SocketConfigError.Step.ByTrigger.",
             test_case.trigger_name}),
        test_case.step, 1);
    histogram_tester.ExpectBucketCount(
        "Net.Quic.Migration.Attempt.SocketConfigError.NetError",
        -test_case.net_error, 1);
    histogram_tester.ExpectBucketCount(
        base::StrCat(
            {"Net.Quic.Migration.Attempt.SocketConfigError.NetError.ByStep.",
             test_case.step_name}),
        -test_case.net_error, 1);
    histogram_tester.ExpectBucketCount(
        base::StrCat(
            {"Net.Quic.Migration.Attempt.SocketConfigError.NetError.ByTrigger.",
             test_case.trigger_name}),
        -test_case.net_error, 1);
  }

  histogram_tester.ExpectTotalCount("Net.Quic.Migration.Attempt.Ineligible", 0);
  histogram_tester.ExpectTotalCount("Net.Quic.Migration.Attempt.Superseded", 0);
  histogram_tester.ExpectTotalCount(
      "Net.Quic.Migration.Attempt.SpuriousOutcome", 0);
  histogram_tester.ExpectTotalCount(
      "Net.Quic.Migration.Attempt.UnclassifiedOutcome", 0);
}

#if GTEST_HAS_DEATH_TEST
TEST_F(QuicMigrationAttemptContextTest, SetFailureSocketConfigFailedCrashes) {
  auto context =
      CreateAttemptContext(QuicMigrationAttemptCause::kOnNetworkMadeDefault);
  EXPECT_CHECK_DEATH(context->SetFailure(
      QuicMigrationAttemptFailureReason::kSocketConfigFailed));
}

TEST_F(QuicMigrationAttemptContextTest,
       SetSocketConfigFailureNonNegativeErrorCrashes) {
  auto context =
      CreateAttemptContext(QuicMigrationAttemptCause::kOnNetworkMadeDefault);
  EXPECT_CHECK_DEATH(
      context->SetSocketConfigFailure(QuicSocketConfigStep::kConnect, OK));
}
#endif

TEST_F(QuicMigrationAttemptContextTest, SetFailureGoogleHost) {
  base::HistogramTester histogram_tester;
  {
    auto context =
        CreateAttemptContext(QuicMigrationAttemptCause::kOnNetworkDisconnected,
                             base::BindRepeating([]() { return true; }),
                             /*is_google_host=*/true);
    context->SetFailure(
        QuicMigrationAttemptFailureReason::kNoUnusedConnectionId);
  }

  {
    auto context =
        CreateAttemptContext(QuicMigrationAttemptCause::kOnNetworkDisconnected,
                             base::BindRepeating([]() { return true; }),
                             /*is_google_host=*/true);
    context->SetSocketConfigFailure(QuicSocketConfigStep::kConnect,
                                    ERR_CONNECTION_REFUSED);
  }

  histogram_tester.ExpectBucketCount(
      "Net.Quic.Migration.Attempt.FailureReason",
      QuicMigrationAttemptFailureReason::kNoUnusedConnectionId, 1);
  histogram_tester.ExpectBucketCount(
      "Net.Quic.Migration.Attempt.FailureReason",
      QuicMigrationAttemptFailureReason::kSocketConfigFailed, 1);
  histogram_tester.ExpectBucketCount(
      "Net.Quic.Migration.Attempt.FailureReason.GoogleHost",
      QuicMigrationAttemptFailureReason::kNoUnusedConnectionId, 1);
  histogram_tester.ExpectBucketCount(
      "Net.Quic.Migration.Attempt.FailureReason.GoogleHost",
      QuicMigrationAttemptFailureReason::kSocketConfigFailed, 1);

  // Non-Google host should not record the GoogleHost histogram.
  {
    auto context =
        CreateAttemptContext(QuicMigrationAttemptCause::kOnNetworkDisconnected,
                             base::BindRepeating([]() { return true; }),
                             /*is_google_host=*/false);
    context->SetFailure(QuicMigrationAttemptFailureReason::kProbeTimeout);
  }

  {
    auto context =
        CreateAttemptContext(QuicMigrationAttemptCause::kOnNetworkDisconnected,
                             base::BindRepeating([]() { return true; }),
                             /*is_google_host=*/false);
    context->SetSocketConfigFailure(QuicSocketConfigStep::kConnect,
                                    ERR_CONNECTION_REFUSED);
  }

  histogram_tester.ExpectBucketCount(
      "Net.Quic.Migration.Attempt.FailureReason",
      QuicMigrationAttemptFailureReason::kProbeTimeout, 1);
  histogram_tester.ExpectBucketCount(
      "Net.Quic.Migration.Attempt.FailureReason",
      QuicMigrationAttemptFailureReason::kSocketConfigFailed, 2);
  histogram_tester.ExpectTotalCount("Net.Quic.Migration.Attempt.FailureReason",
                                    4);
  histogram_tester.ExpectTotalCount(
      "Net.Quic.Migration.Attempt.FailureReason.GoogleHost", 2);
}

}  // namespace
}  // namespace net
