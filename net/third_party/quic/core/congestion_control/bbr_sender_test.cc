// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/congestion_control/bbr_sender.h"

#include <algorithm>
#include <map>
#include <memory>

#include "net/third_party/quic/core/congestion_control/rtt_stats.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/platform/api/quic_logging.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/test_tools/mock_clock.h"
#include "net/third_party/quic/test_tools/quic_config_peer.h"
#include "net/third_party/quic/test_tools/quic_connection_peer.h"
#include "net/third_party/quic/test_tools/quic_sent_packet_manager_peer.h"
#include "net/third_party/quic/test_tools/quic_test_utils.h"
#include "net/third_party/quic/test_tools/simulator/quic_endpoint.h"
#include "net/third_party/quic/test_tools/simulator/simulator.h"
#include "net/third_party/quic/test_tools/simulator/switch.h"

namespace quic {
namespace test {

// Use the initial CWND of 10, as 32 is too much for the test network.
const uint32_t kInitialCongestionWindowPackets = 10;
const uint32_t kDefaultWindowTCP =
    kInitialCongestionWindowPackets * kDefaultTCPMSS;

// Test network parameters.  Here, the topology of the network is:
//
//          BBR sender
//               |
//               |  <-- local link (10 Mbps, 2 ms delay)
//               |
//        Network switch
//               *  <-- the bottleneck queue in the direction
//               |          of the receiver
//               |
//               |  <-- test link (4 Mbps, 30 ms delay)
//               |
//               |
//           Receiver
//
// The reason the bandwidths chosen are relatively low is the fact that the
// connection simulator uses QuicTime for its internal clock, and as such has
// the granularity of 1us, meaning that at bandwidth higher than 20 Mbps the
// packets can start to land on the same timestamp.
const QuicBandwidth kTestLinkBandwidth =
    QuicBandwidth::FromKBitsPerSecond(4000);
const QuicBandwidth kLocalLinkBandwidth =
    QuicBandwidth::FromKBitsPerSecond(10000);
const QuicTime::Delta kTestPropagationDelay =
    QuicTime::Delta::FromMilliseconds(30);
const QuicTime::Delta kLocalPropagationDelay =
    QuicTime::Delta::FromMilliseconds(2);
const QuicTime::Delta kTestTransferTime =
    kTestLinkBandwidth.TransferTime(kMaxPacketSize) +
    kLocalLinkBandwidth.TransferTime(kMaxPacketSize);
const QuicTime::Delta kTestRtt =
    (kTestPropagationDelay + kLocalPropagationDelay + kTestTransferTime) * 2;
const QuicByteCount kTestBdp = kTestRtt * kTestLinkBandwidth;

class BbrSenderTest : public QuicTest {
 protected:
  BbrSenderTest()
      : simulator_(),
        bbr_sender_(&simulator_,
                    "BBR sender",
                    "Receiver",
                    Perspective::IS_CLIENT,
                    /*connection_id=*/42),
        competing_sender_(&simulator_,
                          "Competing sender",
                          "Competing receiver",
                          Perspective::IS_CLIENT,
                          /*connection_id=*/43),
        receiver_(&simulator_,
                  "Receiver",
                  "BBR sender",
                  Perspective::IS_SERVER,
                  /*connection_id=*/42),
        competing_receiver_(&simulator_,
                            "Competing receiver",
                            "Competing sender",
                            Perspective::IS_SERVER,
                            /*connection_id=*/43),
        receiver_multiplexer_("Receiver multiplexer",
                              {&receiver_, &competing_receiver_}) {
    rtt_stats_ = bbr_sender_.connection()->sent_packet_manager().GetRttStats();
    sender_ = SetupBbrSender(&bbr_sender_);

    clock_ = simulator_.GetClock();
    simulator_.set_random_generator(&random_);

    uint64_t seed = QuicRandom::GetInstance()->RandUint64();
    random_.set_seed(seed);
    QUIC_LOG(INFO) << "BbrSenderTest simulator set up.  Seed: " << seed;
  }

  simulator::Simulator simulator_;
  simulator::QuicEndpoint bbr_sender_;
  simulator::QuicEndpoint competing_sender_;
  simulator::QuicEndpoint receiver_;
  simulator::QuicEndpoint competing_receiver_;
  simulator::QuicEndpointMultiplexer receiver_multiplexer_;
  std::unique_ptr<simulator::Switch> switch_;
  std::unique_ptr<simulator::SymmetricLink> bbr_sender_link_;
  std::unique_ptr<simulator::SymmetricLink> competing_sender_link_;
  std::unique_ptr<simulator::SymmetricLink> receiver_link_;

  SimpleRandom random_;

  // Owned by different components of the connection.
  const QuicClock* clock_;
  const RttStats* rtt_stats_;
  BbrSender* sender_;

  // Enables BBR on |endpoint| and returns the associated BBR congestion
  // controller.
  BbrSender* SetupBbrSender(simulator::QuicEndpoint* endpoint) {
    const RttStats* rtt_stats =
        endpoint->connection()->sent_packet_manager().GetRttStats();
    // Ownership of the sender will be overtaken by the endpoint.
    BbrSender* sender = new BbrSender(
        rtt_stats,
        QuicSentPacketManagerPeer::GetUnackedPacketMap(
            QuicConnectionPeer::GetSentPacketManager(endpoint->connection())),
        kInitialCongestionWindowPackets, kDefaultMaxCongestionWindowPackets,
        &random_);
    QuicConnectionPeer::SetSendAlgorithm(endpoint->connection(), sender);
    endpoint->RecordTrace();
    return sender;
  }

  // Creates a default setup, which is a network with a bottleneck between the
  // receiver and the switch.  The switch has the buffers four times larger than
  // the bottleneck BDP, which should guarantee a lack of losses.
  void CreateDefaultSetup() {
    switch_ = QuicMakeUnique<simulator::Switch>(&simulator_, "Switch", 8,
                                                2 * kTestBdp);
    bbr_sender_link_ = QuicMakeUnique<simulator::SymmetricLink>(
        &bbr_sender_, switch_->port(1), kLocalLinkBandwidth,
        kLocalPropagationDelay);
    receiver_link_ = QuicMakeUnique<simulator::SymmetricLink>(
        &receiver_, switch_->port(2), kTestLinkBandwidth,
        kTestPropagationDelay);
  }

  // Same as the default setup, except the buffer now is half of the BDP.
  void CreateSmallBufferSetup() {
    switch_ = QuicMakeUnique<simulator::Switch>(&simulator_, "Switch", 8,
                                                0.5 * kTestBdp);
    bbr_sender_link_ = QuicMakeUnique<simulator::SymmetricLink>(
        &bbr_sender_, switch_->port(1), kLocalLinkBandwidth,
        kTestPropagationDelay);
    receiver_link_ = QuicMakeUnique<simulator::SymmetricLink>(
        &receiver_, switch_->port(2), kTestLinkBandwidth,
        kTestPropagationDelay);
  }

  // Creates the variation of the default setup in which there is another sender
  // that competes for the same bottleneck link.
  void CreateCompetitionSetup() {
    switch_ = QuicMakeUnique<simulator::Switch>(&simulator_, "Switch", 8,
                                                2 * kTestBdp);

    // Add a small offset to the competing link in order to avoid
    // synchronization effects.
    const QuicTime::Delta small_offset = QuicTime::Delta::FromMicroseconds(3);
    bbr_sender_link_ = QuicMakeUnique<simulator::SymmetricLink>(
        &bbr_sender_, switch_->port(1), kLocalLinkBandwidth,
        kLocalPropagationDelay);
    competing_sender_link_ = QuicMakeUnique<simulator::SymmetricLink>(
        &competing_sender_, switch_->port(3), kLocalLinkBandwidth,
        kLocalPropagationDelay + small_offset);
    receiver_link_ = QuicMakeUnique<simulator::SymmetricLink>(
        &receiver_multiplexer_, switch_->port(2), kTestLinkBandwidth,
        kTestPropagationDelay);
  }

  // Creates a BBR vs BBR competition setup.
  void CreateBbrVsBbrSetup() {
    SetupBbrSender(&competing_sender_);
    CreateCompetitionSetup();
  }

  void EnableAggregation(QuicByteCount aggregation_bytes,
                         QuicTime::Delta aggregation_timeout) {
    // Enable aggregation on the path from the receiver to the sender.
    switch_->port_queue(1)->EnableAggregation(aggregation_bytes,
                                              aggregation_timeout);
  }

  void DoSimpleTransfer(QuicByteCount transfer_size, QuicTime::Delta deadline) {
    bbr_sender_.AddBytesToTransfer(transfer_size);
    // TODO(vasilvv): consider rewriting this to run until the receiver actually
    // receives the intended amount of bytes.
    bool simulator_result = simulator_.RunUntilOrTimeout(
        [this]() { return bbr_sender_.bytes_to_transfer() == 0; }, deadline);
    EXPECT_TRUE(simulator_result)
        << "Simple transfer failed.  Bytes remaining: "
        << bbr_sender_.bytes_to_transfer();
    QUIC_LOG(INFO) << "Simple transfer state: " << sender_->ExportDebugState();
  }

  // Drive the simulator by sending enough data to enter PROBE_BW.
  void DriveOutOfStartup() {
    ASSERT_FALSE(sender_->ExportDebugState().is_at_full_bandwidth);
    DoSimpleTransfer(1024 * 1024, QuicTime::Delta::FromSeconds(15));
    EXPECT_EQ(BbrSender::PROBE_BW, sender_->ExportDebugState().mode);
    ExpectApproxEq(kTestLinkBandwidth,
                   sender_->ExportDebugState().max_bandwidth, 0.02f);
  }

  // Send |bytes|-sized bursts of data |number_of_bursts| times, waiting for
  // |wait_time| between each burst.
  void SendBursts(size_t number_of_bursts,
                  QuicByteCount bytes,
                  QuicTime::Delta wait_time) {
    ASSERT_EQ(0u, bbr_sender_.bytes_to_transfer());
    for (size_t i = 0; i < number_of_bursts; i++) {
      bbr_sender_.AddBytesToTransfer(bytes);

      // Transfer data and wait for three seconds between each transfer.
      simulator_.RunFor(wait_time);

      // Ensure the connection did not time out.
      ASSERT_TRUE(bbr_sender_.connection()->connected());
      ASSERT_TRUE(receiver_.connection()->connected());
    }

    simulator_.RunFor(wait_time + kTestRtt);
    ASSERT_EQ(0u, bbr_sender_.bytes_to_transfer());
  }

  void SetConnectionOption(QuicTag option) {
    QuicConfig config;
    QuicTagVector options;
    options.push_back(option);
    QuicConfigPeer::SetReceivedConnectionOptions(&config, options);
    sender_->SetFromConfig(config, Perspective::IS_SERVER);
  }
};

TEST_F(BbrSenderTest, SetInitialCongestionWindow) {
  EXPECT_NE(3u * kDefaultTCPMSS, sender_->GetCongestionWindow());
  sender_->SetInitialCongestionWindowInPackets(3);
  EXPECT_EQ(3u * kDefaultTCPMSS, sender_->GetCongestionWindow());
}

// Test a simple long data transfer in the default setup.
TEST_F(BbrSenderTest, SimpleTransfer) {
  // Disable Ack Decimation on the receiver, because it can increase srtt.
  QuicConnectionPeer::SetAckMode(receiver_.connection(),
                                 QuicConnection::AckMode::TCP_ACKING);
  CreateDefaultSetup();

  // At startup make sure we are at the default.
  EXPECT_EQ(kDefaultWindowTCP, sender_->GetCongestionWindow());
  // At startup make sure we can send.
  EXPECT_TRUE(sender_->CanSend(0));
  // And that window is un-affected.
  EXPECT_EQ(kDefaultWindowTCP, sender_->GetCongestionWindow());

  // Verify that Sender is in slow start.
  EXPECT_TRUE(sender_->InSlowStart());

  // Verify that pacing rate is based on the initial RTT.
  QuicBandwidth expected_pacing_rate = QuicBandwidth::FromBytesAndTimeDelta(
      2.885 * kDefaultWindowTCP, rtt_stats_->initial_rtt());
  ExpectApproxEq(expected_pacing_rate.ToBitsPerSecond(),
                 sender_->PacingRate(0).ToBitsPerSecond(), 0.01f);

  ASSERT_GE(kTestBdp, kDefaultWindowTCP + kDefaultTCPMSS);

  DoSimpleTransfer(12 * 1024 * 1024, QuicTime::Delta::FromSeconds(30));
  EXPECT_EQ(BbrSender::PROBE_BW, sender_->ExportDebugState().mode);
  EXPECT_EQ(0u, bbr_sender_.connection()->GetStats().packets_lost);
  EXPECT_FALSE(sender_->ExportDebugState().last_sample_is_app_limited);

  // The margin here is quite high, since there exists a possibility that the
  // connection just exited high gain cycle.
  ExpectApproxEq(kTestRtt, rtt_stats_->smoothed_rtt(), 0.2f);
}

// Test a simple transfer in a situation when the buffer is less than BDP.
TEST_F(BbrSenderTest, SimpleTransferSmallBuffer) {
  CreateSmallBufferSetup();

  DoSimpleTransfer(12 * 1024 * 1024, QuicTime::Delta::FromSeconds(30));
  EXPECT_EQ(BbrSender::PROBE_BW, sender_->ExportDebugState().mode);
  ExpectApproxEq(kTestLinkBandwidth, sender_->ExportDebugState().max_bandwidth,
                 0.01f);
  EXPECT_GE(bbr_sender_.connection()->GetStats().packets_lost, 0u);
  EXPECT_FALSE(sender_->ExportDebugState().last_sample_is_app_limited);
}

TEST_F(BbrSenderTest, SimpleTransferEarlyPacketLoss) {
  SetQuicReloadableFlag(quic_bbr_no_bytes_acked_in_startup_recovery, true);
  // Enable rate based startup so the recovery window doesn't hide the true
  // congestion_window_ in GetCongestionWindow().
  SetConnectionOption(kBBS1);
  // Disable Ack Decimation on the receiver, because it can increase srtt.
  QuicConnectionPeer::SetAckMode(receiver_.connection(),
                                 QuicConnection::AckMode::TCP_ACKING);
  CreateDefaultSetup();

  // At startup make sure we are at the default.
  EXPECT_EQ(kDefaultWindowTCP, sender_->GetCongestionWindow());
  // Verify that Sender is in slow start.
  EXPECT_TRUE(sender_->InSlowStart());
  // At startup make sure we can send.
  EXPECT_TRUE(sender_->CanSend(0));
  // And that window is un-affected.
  EXPECT_EQ(kDefaultWindowTCP, sender_->GetCongestionWindow());

  // Transfer 12MB.
  bbr_sender_.AddBytesToTransfer(12 * 1024 * 1024);
  // Drop the first packet.
  receiver_.DropNextIncomingPacket();
  bool simulator_result = simulator_.RunUntilOrTimeout(
      [this]() {
        if (sender_->InRecovery()) {
          // Two packets are acked before the first is declared lost.
          EXPECT_LE(sender_->GetCongestionWindow(),
                    (kDefaultWindowTCP + 2 * kDefaultTCPMSS));
        }
        return bbr_sender_.bytes_to_transfer() == 0 || !sender_->InSlowStart();
      },
      QuicTime::Delta::FromSeconds(30));
  EXPECT_TRUE(simulator_result) << "Simple transfer failed.  Bytes remaining: "
                                << bbr_sender_.bytes_to_transfer();
  EXPECT_EQ(BbrSender::DRAIN, sender_->ExportDebugState().mode);
  EXPECT_EQ(1u, bbr_sender_.connection()->GetStats().packets_lost);
  EXPECT_FALSE(sender_->ExportDebugState().last_sample_is_app_limited);
}

// Test a simple long data transfer with 2 rtts of aggregation.
TEST_F(BbrSenderTest, SimpleTransfer2RTTAggregationBytes) {
  CreateDefaultSetup();
  // 2 RTTs of aggregation, with a max of 10kb.
  EnableAggregation(10 * 1024, 2 * kTestRtt);

  // Transfer 12MB.
  DoSimpleTransfer(12 * 1024 * 1024, QuicTime::Delta::FromSeconds(35));
  EXPECT_EQ(BbrSender::PROBE_BW, sender_->ExportDebugState().mode);
  // It's possible to read a bandwidth as much as 50% too high with aggregation.
  EXPECT_LE(kTestLinkBandwidth * 0.99f,
            sender_->ExportDebugState().max_bandwidth);
  // TODO(ianswett): Tighten this bound once we understand why BBR is
  // overestimating bandwidth with aggregation. b/36022633
  EXPECT_GE(kTestLinkBandwidth * 1.5f,
            sender_->ExportDebugState().max_bandwidth);
  // TODO(ianswett): Expect 0 packets are lost once BBR no longer measures
  // bandwidth higher than the link rate.
  // The margin here is high, because the aggregation greatly increases
  // smoothed rtt.
  EXPECT_GE(kTestRtt * 4, rtt_stats_->smoothed_rtt());
  ExpectApproxEq(kTestRtt, rtt_stats_->min_rtt(), 0.2f);
}

// Test a simple long data transfer with 2 rtts of aggregation.
TEST_F(BbrSenderTest, SimpleTransferAckDecimation) {
  // Decrease the CWND gain so extra CWND is required with stretch acks.
  FLAGS_quic_bbr_cwnd_gain = 1.0;
  sender_ = new BbrSender(
      rtt_stats_,
      QuicSentPacketManagerPeer::GetUnackedPacketMap(
          QuicConnectionPeer::GetSentPacketManager(bbr_sender_.connection())),
      kInitialCongestionWindowPackets, kDefaultMaxCongestionWindowPackets,
      &random_);
  QuicConnectionPeer::SetSendAlgorithm(bbr_sender_.connection(), sender_);
  // Enable Ack Decimation on the receiver.
  QuicConnectionPeer::SetAckMode(receiver_.connection(),
                                 QuicConnection::AckMode::ACK_DECIMATION);
  CreateDefaultSetup();

  // Transfer 12MB.
  DoSimpleTransfer(12 * 1024 * 1024, QuicTime::Delta::FromSeconds(35));
  EXPECT_EQ(BbrSender::PROBE_BW, sender_->ExportDebugState().mode);
  // It's possible to read a bandwidth as much as 50% too high with aggregation.
  EXPECT_LE(kTestLinkBandwidth * 0.99f,
            sender_->ExportDebugState().max_bandwidth);
  // TODO(ianswett): Tighten this bound once we understand why BBR is
  // overestimating bandwidth with aggregation. b/36022633
  EXPECT_GE(kTestLinkBandwidth * 1.5f,
            sender_->ExportDebugState().max_bandwidth);
  // TODO(ianswett): Expect 0 packets are lost once BBR no longer measures
  // bandwidth higher than the link rate.
  EXPECT_FALSE(sender_->ExportDebugState().last_sample_is_app_limited);
  // The margin here is high, because the aggregation greatly increases
  // smoothed rtt.
  EXPECT_GE(kTestRtt * 2, rtt_stats_->smoothed_rtt());
  ExpectApproxEq(kTestRtt, rtt_stats_->min_rtt(), 0.1f);
}

// Test a simple long data transfer with 2 rtts of aggregation.
TEST_F(BbrSenderTest, SimpleTransfer2RTTAggregationBytes20RTTWindow) {
  // Disable Ack Decimation on the receiver, because it can increase srtt.
  QuicConnectionPeer::SetAckMode(receiver_.connection(),
                                 QuicConnection::AckMode::TCP_ACKING);
  CreateDefaultSetup();
  SetConnectionOption(kBBR4);
  // 2 RTTs of aggregation, with a max of 10kb.
  EnableAggregation(10 * 1024, 2 * kTestRtt);

  // Transfer 12MB.
  DoSimpleTransfer(12 * 1024 * 1024, QuicTime::Delta::FromSeconds(35));
  EXPECT_EQ(BbrSender::PROBE_BW, sender_->ExportDebugState().mode);
  // It's possible to read a bandwidth as much as 50% too high with aggregation.
  EXPECT_LE(kTestLinkBandwidth * 0.99f,
            sender_->ExportDebugState().max_bandwidth);
  // TODO(ianswett): Tighten this bound once we understand why BBR is
  // overestimating bandwidth with aggregation. b/36022633
  EXPECT_GE(kTestLinkBandwidth * 1.5f,
            sender_->ExportDebugState().max_bandwidth);
  // TODO(ianswett): Expect 0 packets are lost once BBR no longer measures
  // bandwidth higher than the link rate.
  // The margin here is high, because the aggregation greatly increases
  // smoothed rtt.
  EXPECT_GE(kTestRtt * 4, rtt_stats_->smoothed_rtt());
  ExpectApproxEq(kTestRtt, rtt_stats_->min_rtt(), 0.12f);
}

// Test a simple long data transfer with 2 rtts of aggregation.
TEST_F(BbrSenderTest, SimpleTransfer2RTTAggregationBytes40RTTWindow) {
  // Disable Ack Decimation on the receiver, because it can increase srtt.
  QuicConnectionPeer::SetAckMode(receiver_.connection(),
                                 QuicConnection::AckMode::TCP_ACKING);
  CreateDefaultSetup();
  SetConnectionOption(kBBR5);
  // 2 RTTs of aggregation, with a max of 10kb.
  EnableAggregation(10 * 1024, 2 * kTestRtt);

  // Transfer 12MB.
  DoSimpleTransfer(12 * 1024 * 1024, QuicTime::Delta::FromSeconds(35));
  EXPECT_EQ(BbrSender::PROBE_BW, sender_->ExportDebugState().mode);
  // It's possible to read a bandwidth as much as 50% too high with aggregation.
  EXPECT_LE(kTestLinkBandwidth * 0.99f,
            sender_->ExportDebugState().max_bandwidth);
  // TODO(ianswett): Tighten this bound once we understand why BBR is
  // overestimating bandwidth with aggregation. b/36022633
  EXPECT_GE(kTestLinkBandwidth * 1.5f,
            sender_->ExportDebugState().max_bandwidth);
  // TODO(ianswett): Expect 0 packets are lost once BBR no longer measures
  // bandwidth higher than the link rate.
  // The margin here is high, because the aggregation greatly increases
  // smoothed rtt.
  EXPECT_GE(kTestRtt * 4, rtt_stats_->smoothed_rtt());
  ExpectApproxEq(kTestRtt, rtt_stats_->min_rtt(), 0.12f);
}

// Test the number of losses incurred by the startup phase in a situation when
// the buffer is less than BDP.
TEST_F(BbrSenderTest, PacketLossOnSmallBufferStartup) {
  CreateSmallBufferSetup();

  DriveOutOfStartup();
  float loss_rate =
      static_cast<float>(bbr_sender_.connection()->GetStats().packets_lost) /
      bbr_sender_.connection()->GetStats().packets_sent;
  EXPECT_LE(loss_rate, 0.31);
}

// Ensures the code transitions loss recovery states correctly (NOT_IN_RECOVERY
// -> CONSERVATION -> GROWTH -> NOT_IN_RECOVERY).
TEST_F(BbrSenderTest, RecoveryStates) {
  // Set seed to the position where the gain cycling causes the sender go
  // into conservation upon entering PROBE_BW.
  //
  // TODO(vasilvv): there should be a better way to test this.
  random_.set_seed(UINT64_C(14719894707049085006));

  const QuicTime::Delta timeout = QuicTime::Delta::FromSeconds(10);
  bool simulator_result;
  CreateSmallBufferSetup();

  bbr_sender_.AddBytesToTransfer(100 * 1024 * 1024);
  ASSERT_EQ(BbrSender::NOT_IN_RECOVERY,
            sender_->ExportDebugState().recovery_state);

  simulator_result = simulator_.RunUntilOrTimeout(
      [this]() {
        return sender_->ExportDebugState().recovery_state !=
               BbrSender::NOT_IN_RECOVERY;
      },
      timeout);
  ASSERT_TRUE(simulator_result);
  ASSERT_EQ(BbrSender::CONSERVATION,
            sender_->ExportDebugState().recovery_state);

  simulator_result = simulator_.RunUntilOrTimeout(
      [this]() {
        return sender_->ExportDebugState().recovery_state !=
               BbrSender::CONSERVATION;
      },
      timeout);
  ASSERT_TRUE(simulator_result);
  ASSERT_EQ(BbrSender::GROWTH, sender_->ExportDebugState().recovery_state);

  simulator_result = simulator_.RunUntilOrTimeout(
      [this]() {
        return sender_->ExportDebugState().recovery_state != BbrSender::GROWTH;
      },
      timeout);

  ASSERT_EQ(BbrSender::PROBE_BW, sender_->ExportDebugState().mode);
  ASSERT_EQ(BbrSender::NOT_IN_RECOVERY,
            sender_->ExportDebugState().recovery_state);
  ASSERT_TRUE(simulator_result);
}

// Ensures the code transitions loss recovery states correctly when in STARTUP
// and the BBS2 connection option is used.
// (NOT_IN_RECOVERY -> CONSERVATION -> GROWTH -> NOT_IN_RECOVERY).
TEST_F(BbrSenderTest, StartupMediumRecoveryStates) {
  // Set seed to the position where the gain cycling causes the sender go
  // into conservation upon entering PROBE_BW.
  //
  // TODO(vasilvv): there should be a better way to test this.
  random_.set_seed(UINT64_C(14719894707049085006));

  const QuicTime::Delta timeout = QuicTime::Delta::FromSeconds(10);
  bool simulator_result;
  CreateSmallBufferSetup();
  SetConnectionOption(kBBS2);

  bbr_sender_.AddBytesToTransfer(100 * 1024 * 1024);
  ASSERT_EQ(BbrSender::NOT_IN_RECOVERY,
            sender_->ExportDebugState().recovery_state);

  simulator_result = simulator_.RunUntilOrTimeout(
      [this]() {
        return sender_->ExportDebugState().recovery_state !=
               BbrSender::NOT_IN_RECOVERY;
      },
      timeout);
  ASSERT_TRUE(simulator_result);
  ASSERT_EQ(BbrSender::MEDIUM_GROWTH,
            sender_->ExportDebugState().recovery_state);

  simulator_result = simulator_.RunUntilOrTimeout(
      [this]() {
        return sender_->ExportDebugState().recovery_state !=
               BbrSender::MEDIUM_GROWTH;
      },
      timeout);
  ASSERT_TRUE(simulator_result);
  ASSERT_EQ(BbrSender::GROWTH, sender_->ExportDebugState().recovery_state);

  simulator_result = simulator_.RunUntilOrTimeout(
      [this]() {
        return sender_->ExportDebugState().recovery_state != BbrSender::GROWTH;
      },
      timeout);

  ASSERT_EQ(BbrSender::PROBE_BW, sender_->ExportDebugState().mode);
  ASSERT_EQ(BbrSender::NOT_IN_RECOVERY,
            sender_->ExportDebugState().recovery_state);
  ASSERT_TRUE(simulator_result);
}

// Ensures the code transitions loss recovery states correctly when in STARTUP
// and the BBS3 connection option is used.
// (NOT_IN_RECOVERY -> GROWTH -> NOT_IN_RECOVERY).
TEST_F(BbrSenderTest, StartupGrowthRecoveryStates) {
  // Set seed to the position where the gain cycling causes the sender go
  // into conservation upon entering PROBE_BW.
  //
  // TODO(vasilvv): there should be a better way to test this.
  random_.set_seed(UINT64_C(14719894707049085006));

  const QuicTime::Delta timeout = QuicTime::Delta::FromSeconds(10);
  bool simulator_result;
  CreateSmallBufferSetup();
  SetConnectionOption(kBBS3);

  bbr_sender_.AddBytesToTransfer(100 * 1024 * 1024);
  ASSERT_EQ(BbrSender::NOT_IN_RECOVERY,
            sender_->ExportDebugState().recovery_state);

  simulator_result = simulator_.RunUntilOrTimeout(
      [this]() {
        return sender_->ExportDebugState().recovery_state !=
               BbrSender::NOT_IN_RECOVERY;
      },
      timeout);
  ASSERT_TRUE(simulator_result);
  ASSERT_EQ(BbrSender::GROWTH, sender_->ExportDebugState().recovery_state);

  simulator_result = simulator_.RunUntilOrTimeout(
      [this]() {
        return sender_->ExportDebugState().recovery_state != BbrSender::GROWTH;
      },
      timeout);
  ASSERT_TRUE(simulator_result);

  ASSERT_EQ(BbrSender::PROBE_BW, sender_->ExportDebugState().mode);
  ASSERT_EQ(BbrSender::NOT_IN_RECOVERY,
            sender_->ExportDebugState().recovery_state);
  ASSERT_TRUE(simulator_result);
}

// Verify the behavior of the algorithm in the case when the connection sends
// small bursts of data after sending continuously for a while.
TEST_F(BbrSenderTest, ApplicationLimitedBursts) {
  CreateDefaultSetup();

  DriveOutOfStartup();
  EXPECT_FALSE(sender_->ExportDebugState().last_sample_is_app_limited);

  SendBursts(20, 512, QuicTime::Delta::FromSeconds(3));
  EXPECT_TRUE(sender_->ExportDebugState().last_sample_is_app_limited);
  ExpectApproxEq(kTestLinkBandwidth, sender_->ExportDebugState().max_bandwidth,
                 0.01f);
}

// Verify the behavior of the algorithm in the case when the connection sends
// small bursts of data and then starts sending continuously.
TEST_F(BbrSenderTest, ApplicationLimitedBurstsWithoutPrior) {
  CreateDefaultSetup();

  SendBursts(40, 512, QuicTime::Delta::FromSeconds(3));
  EXPECT_TRUE(sender_->ExportDebugState().last_sample_is_app_limited);

  DriveOutOfStartup();
  ExpectApproxEq(kTestLinkBandwidth, sender_->ExportDebugState().max_bandwidth,
                 0.01f);
  EXPECT_FALSE(sender_->ExportDebugState().last_sample_is_app_limited);
}

// Verify that the DRAIN phase works correctly.
TEST_F(BbrSenderTest, Drain) {
  // Disable Ack Decimation on the receiver, because it can increase srtt.
  QuicConnectionPeer::SetAckMode(receiver_.connection(),
                                 QuicConnection::AckMode::TCP_ACKING);
  CreateDefaultSetup();
  const QuicTime::Delta timeout = QuicTime::Delta::FromSeconds(10);
  // Get the queue at the bottleneck, which is the outgoing queue at the port to
  // which the receiver is connected.
  const simulator::Queue* queue = switch_->port_queue(2);
  bool simulator_result;

  // We have no intention of ever finishing this transfer.
  bbr_sender_.AddBytesToTransfer(100 * 1024 * 1024);

  // Run the startup, and verify that it fills up the queue.
  ASSERT_EQ(BbrSender::STARTUP, sender_->ExportDebugState().mode);
  simulator_result = simulator_.RunUntilOrTimeout(
      [this]() {
        return sender_->ExportDebugState().mode != BbrSender::STARTUP;
      },
      timeout);
  ASSERT_TRUE(simulator_result);
  ASSERT_EQ(BbrSender::DRAIN, sender_->ExportDebugState().mode);
  ExpectApproxEq(sender_->BandwidthEstimate() * (1 / 2.885f),
                 sender_->PacingRate(0), 0.01f);
  // BBR uses CWND gain of 2.88 during STARTUP, hence it will fill the buffer
  // with approximately 1.88 BDPs.  Here, we use 1.5 to give some margin for
  // error.
  EXPECT_GE(queue->bytes_queued(), 1.5 * kTestBdp);

  // Observe increased RTT due to bufferbloat.
  const QuicTime::Delta queueing_delay =
      kTestLinkBandwidth.TransferTime(queue->bytes_queued());
  ExpectApproxEq(kTestRtt + queueing_delay, rtt_stats_->latest_rtt(), 0.1f);

  // Transition to the drain phase and verify that it makes the queue
  // have at most a BDP worth of packets.
  simulator_result = simulator_.RunUntilOrTimeout(
      [this]() { return sender_->ExportDebugState().mode != BbrSender::DRAIN; },
      timeout);
  ASSERT_TRUE(simulator_result);
  ASSERT_EQ(BbrSender::PROBE_BW, sender_->ExportDebugState().mode);
  EXPECT_LE(queue->bytes_queued(), kTestBdp);

  // Wait for a few round trips and ensure we're in appropriate phase of gain
  // cycling before taking an RTT measurement.
  const QuicRoundTripCount start_round_trip =
      sender_->ExportDebugState().round_trip_count;
  simulator_result = simulator_.RunUntilOrTimeout(
      [this, start_round_trip]() {
        QuicRoundTripCount rounds_passed =
            sender_->ExportDebugState().round_trip_count - start_round_trip;
        return rounds_passed >= 4 &&
               sender_->ExportDebugState().gain_cycle_index == 7;
      },
      timeout);
  ASSERT_TRUE(simulator_result);

  // Observe the bufferbloat go away.
  ExpectApproxEq(kTestRtt, rtt_stats_->smoothed_rtt(), 0.1f);
}

// Verify that the DRAIN phase works correctly.
TEST_F(BbrSenderTest, ShallowDrain) {
  SetQuicReloadableFlag(quic_bbr_slower_startup3, true);
  // Disable Ack Decimation on the receiver, because it can increase srtt.
  QuicConnectionPeer::SetAckMode(receiver_.connection(),
                                 QuicConnection::AckMode::TCP_ACKING);

  CreateDefaultSetup();
  // BBQ4 increases the pacing gain in DRAIN to 0.75
  SetConnectionOption(kBBQ4);
  const QuicTime::Delta timeout = QuicTime::Delta::FromSeconds(10);
  // Get the queue at the bottleneck, which is the outgoing queue at the port to
  // which the receiver is connected.
  const simulator::Queue* queue = switch_->port_queue(2);
  bool simulator_result;

  // We have no intention of ever finishing this transfer.
  bbr_sender_.AddBytesToTransfer(100 * 1024 * 1024);

  // Run the startup, and verify that it fills up the queue.
  ASSERT_EQ(BbrSender::STARTUP, sender_->ExportDebugState().mode);
  simulator_result = simulator_.RunUntilOrTimeout(
      [this]() {
        return sender_->ExportDebugState().mode != BbrSender::STARTUP;
      },
      timeout);
  ASSERT_TRUE(simulator_result);
  ASSERT_EQ(BbrSender::DRAIN, sender_->ExportDebugState().mode);
  EXPECT_EQ(0.75 * sender_->BandwidthEstimate(), sender_->PacingRate(0));
  // BBR uses CWND gain of 2.88 during STARTUP, hence it will fill the buffer
  // with approximately 1.88 BDPs.  Here, we use 1.5 to give some margin for
  // error.
  EXPECT_GE(queue->bytes_queued(), 1.5 * kTestBdp);

  // Observe increased RTT due to bufferbloat.
  const QuicTime::Delta queueing_delay =
      kTestLinkBandwidth.TransferTime(queue->bytes_queued());
  ExpectApproxEq(kTestRtt + queueing_delay, rtt_stats_->latest_rtt(), 0.1f);

  // Transition to the drain phase and verify that it makes the queue
  // have at most a BDP worth of packets.
  simulator_result = simulator_.RunUntilOrTimeout(
      [this]() { return sender_->ExportDebugState().mode != BbrSender::DRAIN; },
      timeout);
  ASSERT_TRUE(simulator_result);
  ASSERT_EQ(BbrSender::PROBE_BW, sender_->ExportDebugState().mode);
  EXPECT_LE(queue->bytes_queued(), kTestBdp);

  // Wait for a few round trips and ensure we're in appropriate phase of gain
  // cycling before taking an RTT measurement.
  const QuicRoundTripCount start_round_trip =
      sender_->ExportDebugState().round_trip_count;
  simulator_result = simulator_.RunUntilOrTimeout(
      [this, start_round_trip]() {
        QuicRoundTripCount rounds_passed =
            sender_->ExportDebugState().round_trip_count - start_round_trip;
        return rounds_passed >= 4 &&
               sender_->ExportDebugState().gain_cycle_index == 7;
      },
      timeout);
  ASSERT_TRUE(simulator_result);

  // Observe the bufferbloat go away.
  ExpectApproxEq(kTestRtt, rtt_stats_->smoothed_rtt(), 0.1f);
}

// Verify that the connection enters and exits PROBE_RTT correctly.
TEST_F(BbrSenderTest, ProbeRtt) {
  CreateDefaultSetup();
  DriveOutOfStartup();

  // We have no intention of ever finishing this transfer.
  bbr_sender_.AddBytesToTransfer(100 * 1024 * 1024);

  // Wait until the connection enters PROBE_RTT.
  const QuicTime::Delta timeout = QuicTime::Delta::FromSeconds(12);
  bool simulator_result = simulator_.RunUntilOrTimeout(
      [this]() {
        return sender_->ExportDebugState().mode == BbrSender::PROBE_RTT;
      },
      timeout);
  ASSERT_TRUE(simulator_result);
  ASSERT_EQ(BbrSender::PROBE_RTT, sender_->ExportDebugState().mode);

  // Exit PROBE_RTT.
  const QuicTime probe_rtt_start = clock_->Now();
  const QuicTime::Delta time_to_exit_probe_rtt =
      kTestRtt + QuicTime::Delta::FromMilliseconds(200);
  simulator_.RunFor(1.5 * time_to_exit_probe_rtt);
  EXPECT_EQ(BbrSender::PROBE_BW, sender_->ExportDebugState().mode);
  EXPECT_GE(sender_->ExportDebugState().min_rtt_timestamp, probe_rtt_start);
}

// Verify that the first sample after PROBE_RTT is not used as the bandwidth,
// because the round counter doesn't advance during PROBE_RTT.
TEST_F(BbrSenderTest, AppLimitedRecoveryNoBandwidthDecrease) {
  SetQuicReloadableFlag(quic_bbr_app_limited_recovery, true);
  CreateDefaultSetup();
  DriveOutOfStartup();

  // We have no intention of ever finishing this transfer.
  bbr_sender_.AddBytesToTransfer(100 * 1024 * 1024);

  // Wait until the connection enters PROBE_RTT.
  const QuicTime::Delta timeout = QuicTime::Delta::FromSeconds(12);
  bool simulator_result = simulator_.RunUntilOrTimeout(
      [this]() {
        return sender_->ExportDebugState().mode == BbrSender::PROBE_RTT;
      },
      timeout);
  ASSERT_TRUE(simulator_result);
  ASSERT_EQ(BbrSender::PROBE_RTT, sender_->ExportDebugState().mode);

  const QuicBandwidth beginning_bw = sender_->BandwidthEstimate();

  // Run for most of PROBE_RTT.
  const QuicTime probe_rtt_start = clock_->Now();
  const QuicTime::Delta time_to_exit_probe_rtt =
      kTestRtt + QuicTime::Delta::FromMilliseconds(200);
  simulator_.RunFor(0.60 * time_to_exit_probe_rtt);
  EXPECT_EQ(BbrSender::PROBE_RTT, sender_->ExportDebugState().mode);
  EXPECT_TRUE(sender_->ExportDebugState().last_sample_is_app_limited);
  // Lose a packet before exiting PROBE_RTT, which puts us in packet
  // conservation and then continue there for a while and ensure the bandwidth
  // estimate doesn't decrease.
  for (int i = 0; i < 20; ++i) {
    receiver_.DropNextIncomingPacket();
    simulator_.RunFor(0.9 * kTestRtt);
    // Ensure the bandwidth didn't decrease and the samples are app limited.
    EXPECT_LE(beginning_bw, sender_->BandwidthEstimate());
    EXPECT_TRUE(sender_->ExportDebugState().last_sample_is_app_limited);
  }
  EXPECT_GE(sender_->ExportDebugState().min_rtt_timestamp, probe_rtt_start);
}

// Verify that the connection enters and exits PROBE_RTT correctly.
TEST_F(BbrSenderTest, ProbeRttBDPBasedCWNDTarget) {
  CreateDefaultSetup();
  SetQuicReloadableFlag(quic_bbr_less_probe_rtt, true);
  SetConnectionOption(kBBR6);
  DriveOutOfStartup();

  // We have no intention of ever finishing this transfer.
  bbr_sender_.AddBytesToTransfer(100 * 1024 * 1024);

  // Wait until the connection enters PROBE_RTT.
  const QuicTime::Delta timeout = QuicTime::Delta::FromSeconds(12);
  bool simulator_result = simulator_.RunUntilOrTimeout(
      [this]() {
        return sender_->ExportDebugState().mode == BbrSender::PROBE_RTT;
      },
      timeout);
  ASSERT_TRUE(simulator_result);
  ASSERT_EQ(BbrSender::PROBE_RTT, sender_->ExportDebugState().mode);

  // Exit PROBE_RTT.
  const QuicTime probe_rtt_start = clock_->Now();
  const QuicTime::Delta time_to_exit_probe_rtt =
      kTestRtt + QuicTime::Delta::FromMilliseconds(200);
  simulator_.RunFor(1.5 * time_to_exit_probe_rtt);
  EXPECT_EQ(BbrSender::PROBE_BW, sender_->ExportDebugState().mode);
  EXPECT_GE(sender_->ExportDebugState().min_rtt_timestamp, probe_rtt_start);
}

// Verify that the connection enters does not enter PROBE_RTT.
TEST_F(BbrSenderTest, ProbeRttSkippedAfterAppLimitedAndStableRtt) {
  CreateDefaultSetup();
  SetQuicReloadableFlag(quic_bbr_less_probe_rtt, true);
  SetConnectionOption(kBBR7);
  DriveOutOfStartup();

  // We have no intention of ever finishing this transfer.
  bbr_sender_.AddBytesToTransfer(100 * 1024 * 1024);

  // Wait until the connection enters PROBE_RTT.
  const QuicTime::Delta timeout = QuicTime::Delta::FromSeconds(12);
  bool simulator_result = simulator_.RunUntilOrTimeout(
      [this]() {
        return sender_->ExportDebugState().mode == BbrSender::PROBE_RTT;
      },
      timeout);
  ASSERT_FALSE(simulator_result);
  ASSERT_EQ(BbrSender::PROBE_BW, sender_->ExportDebugState().mode);
}

// Verify that the connection enters does not enter PROBE_RTT.
TEST_F(BbrSenderTest, ProbeRttSkippedAfterAppLimited) {
  CreateDefaultSetup();
  SetQuicReloadableFlag(quic_bbr_less_probe_rtt, true);
  SetConnectionOption(kBBR8);
  DriveOutOfStartup();

  // We have no intention of ever finishing this transfer.
  bbr_sender_.AddBytesToTransfer(100 * 1024 * 1024);

  // Wait until the connection enters PROBE_RTT.
  const QuicTime::Delta timeout = QuicTime::Delta::FromSeconds(12);
  bool simulator_result = simulator_.RunUntilOrTimeout(
      [this]() {
        return sender_->ExportDebugState().mode == BbrSender::PROBE_RTT;
      },
      timeout);
  ASSERT_FALSE(simulator_result);
  ASSERT_EQ(BbrSender::PROBE_BW, sender_->ExportDebugState().mode);
}

// Ensure that a connection that is app-limited and is at sufficiently low
// bandwidth will not exit high gain phase, and similarly ensure that the
// connection will exit low gain early if the number of bytes in flight is low.
TEST_F(BbrSenderTest, InFlightAwareGainCycling) {
  // Disable Ack Decimation on the receiver, because it can increase srtt.
  QuicConnectionPeer::SetAckMode(receiver_.connection(),
                                 QuicConnection::AckMode::TCP_ACKING);
  CreateDefaultSetup();
  DriveOutOfStartup();

  const QuicTime::Delta timeout = QuicTime::Delta::FromSeconds(5);
  bool simulator_result;

  // Start a few cycles prior to the high gain one.
  simulator_result = simulator_.RunUntilOrTimeout(
      [this]() { return sender_->ExportDebugState().gain_cycle_index == 6; },
      timeout);

  // Send at 10% of available rate.  Run for 3 seconds, checking in the middle
  // and at the end.  The pacing gain should be high throughout.
  QuicBandwidth target_bandwidth = 0.1f * kTestLinkBandwidth;
  QuicTime::Delta burst_interval = QuicTime::Delta::FromMilliseconds(300);
  for (int i = 0; i < 2; i++) {
    SendBursts(5, target_bandwidth * burst_interval, burst_interval);
    EXPECT_EQ(BbrSender::PROBE_BW, sender_->ExportDebugState().mode);
    EXPECT_EQ(0, sender_->ExportDebugState().gain_cycle_index);
    ExpectApproxEq(kTestLinkBandwidth,
                   sender_->ExportDebugState().max_bandwidth, 0.01f);
  }

  // Now that in-flight is almost zero and the pacing gain is still above 1,
  // send approximately 1.25 BDPs worth of data.  This should cause the
  // PROBE_BW mode to enter low gain cycle, and exit it earlier than one min_rtt
  // due to running out of data to send.
  bbr_sender_.AddBytesToTransfer(1.3 * kTestBdp);
  simulator_result = simulator_.RunUntilOrTimeout(
      [this]() { return sender_->ExportDebugState().gain_cycle_index == 1; },
      timeout);
  ASSERT_TRUE(simulator_result);
  simulator_.RunFor(0.75 * sender_->ExportDebugState().min_rtt);
  EXPECT_EQ(BbrSender::PROBE_BW, sender_->ExportDebugState().mode);
  EXPECT_EQ(2, sender_->ExportDebugState().gain_cycle_index);
}

// Ensure that the pacing rate does not drop at startup.
TEST_F(BbrSenderTest, NoBandwidthDropOnStartup) {
  CreateDefaultSetup();

  const QuicTime::Delta timeout = QuicTime::Delta::FromSeconds(5);
  bool simulator_result;

  QuicBandwidth initial_rate = QuicBandwidth::FromBytesAndTimeDelta(
      kInitialCongestionWindowPackets * kDefaultTCPMSS,
      rtt_stats_->initial_rtt());
  EXPECT_GE(sender_->PacingRate(0), initial_rate);

  // Send a packet.
  bbr_sender_.AddBytesToTransfer(1000);
  simulator_result = simulator_.RunUntilOrTimeout(
      [this]() { return receiver_.bytes_received() == 1000; }, timeout);
  ASSERT_TRUE(simulator_result);
  EXPECT_GE(sender_->PacingRate(0), initial_rate);

  // Wait for a while.
  simulator_.RunFor(QuicTime::Delta::FromSeconds(2));
  EXPECT_GE(sender_->PacingRate(0), initial_rate);

  // Send another packet.
  bbr_sender_.AddBytesToTransfer(1000);
  simulator_result = simulator_.RunUntilOrTimeout(
      [this]() { return receiver_.bytes_received() == 2000; }, timeout);
  ASSERT_TRUE(simulator_result);
  EXPECT_GE(sender_->PacingRate(0), initial_rate);
}

// Test exiting STARTUP earlier due to the 1RTT connection option.
TEST_F(BbrSenderTest, SimpleTransfer1RTTStartup) {
  CreateDefaultSetup();

  SetConnectionOption(k1RTT);
  EXPECT_EQ(1u, sender_->num_startup_rtts());

  // Run until the full bandwidth is reached and check how many rounds it was.
  bbr_sender_.AddBytesToTransfer(12 * 1024 * 1024);
  QuicRoundTripCount max_bw_round = 0;
  QuicBandwidth max_bw(QuicBandwidth::Zero());
  bool simulator_result = simulator_.RunUntilOrTimeout(
      [this, &max_bw, &max_bw_round]() {
        if (max_bw < sender_->ExportDebugState().max_bandwidth) {
          max_bw = sender_->ExportDebugState().max_bandwidth;
          max_bw_round = sender_->ExportDebugState().round_trip_count;
        }
        return sender_->ExportDebugState().is_at_full_bandwidth;
      },
      QuicTime::Delta::FromSeconds(5));
  ASSERT_TRUE(simulator_result);
  EXPECT_EQ(BbrSender::DRAIN, sender_->ExportDebugState().mode);
  EXPECT_EQ(1u, sender_->ExportDebugState().round_trip_count - max_bw_round);
  EXPECT_EQ(1u, sender_->ExportDebugState().rounds_without_bandwidth_gain);
  EXPECT_EQ(0u, bbr_sender_.connection()->GetStats().packets_lost);
  EXPECT_FALSE(sender_->ExportDebugState().last_sample_is_app_limited);
}

// Test exiting STARTUP earlier due to the 2RTT connection option.
TEST_F(BbrSenderTest, SimpleTransfer2RTTStartup) {
  CreateDefaultSetup();

  SetConnectionOption(k2RTT);
  EXPECT_EQ(2u, sender_->num_startup_rtts());

  // Run until the full bandwidth is reached and check how many rounds it was.
  bbr_sender_.AddBytesToTransfer(12 * 1024 * 1024);
  QuicRoundTripCount max_bw_round = 0;
  QuicBandwidth max_bw(QuicBandwidth::Zero());
  bool simulator_result = simulator_.RunUntilOrTimeout(
      [this, &max_bw, &max_bw_round]() {
        if (max_bw < sender_->ExportDebugState().max_bandwidth) {
          max_bw = sender_->ExportDebugState().max_bandwidth;
          max_bw_round = sender_->ExportDebugState().round_trip_count;
        }
        return sender_->ExportDebugState().is_at_full_bandwidth;
      },
      QuicTime::Delta::FromSeconds(5));
  ASSERT_TRUE(simulator_result);
  EXPECT_EQ(BbrSender::DRAIN, sender_->ExportDebugState().mode);
  EXPECT_EQ(2u, sender_->ExportDebugState().round_trip_count - max_bw_round);
  EXPECT_EQ(2u, sender_->ExportDebugState().rounds_without_bandwidth_gain);
  EXPECT_EQ(0u, bbr_sender_.connection()->GetStats().packets_lost);
  EXPECT_FALSE(sender_->ExportDebugState().last_sample_is_app_limited);
}

// Test exiting STARTUP earlier upon loss due to the LRTT connection option.
TEST_F(BbrSenderTest, SimpleTransferLRTTStartup) {
  CreateDefaultSetup();

  SetConnectionOption(kLRTT);
  EXPECT_EQ(3u, sender_->num_startup_rtts());

  // Run until the full bandwidth is reached and check how many rounds it was.
  bbr_sender_.AddBytesToTransfer(12 * 1024 * 1024);
  QuicRoundTripCount max_bw_round = 0;
  QuicBandwidth max_bw(QuicBandwidth::Zero());
  bool simulator_result = simulator_.RunUntilOrTimeout(
      [this, &max_bw, &max_bw_round]() {
        if (max_bw < sender_->ExportDebugState().max_bandwidth) {
          max_bw = sender_->ExportDebugState().max_bandwidth;
          max_bw_round = sender_->ExportDebugState().round_trip_count;
        }
        return sender_->ExportDebugState().is_at_full_bandwidth;
      },
      QuicTime::Delta::FromSeconds(5));
  ASSERT_TRUE(simulator_result);
  EXPECT_EQ(BbrSender::DRAIN, sender_->ExportDebugState().mode);
  EXPECT_EQ(3u, sender_->ExportDebugState().round_trip_count - max_bw_round);
  EXPECT_EQ(3u, sender_->ExportDebugState().rounds_without_bandwidth_gain);
  EXPECT_EQ(0u, bbr_sender_.connection()->GetStats().packets_lost);
  EXPECT_FALSE(sender_->ExportDebugState().last_sample_is_app_limited);
}

// Test exiting STARTUP earlier upon loss due to the LRTT connection option.
TEST_F(BbrSenderTest, SimpleTransferLRTTStartupSmallBuffer) {
  CreateSmallBufferSetup();

  SetConnectionOption(kLRTT);
  EXPECT_EQ(3u, sender_->num_startup_rtts());

  // Run until the full bandwidth is reached and check how many rounds it was.
  bbr_sender_.AddBytesToTransfer(12 * 1024 * 1024);
  QuicRoundTripCount max_bw_round = 0;
  QuicBandwidth max_bw(QuicBandwidth::Zero());
  bool simulator_result = simulator_.RunUntilOrTimeout(
      [this, &max_bw, &max_bw_round]() {
        if (max_bw < sender_->ExportDebugState().max_bandwidth) {
          max_bw = sender_->ExportDebugState().max_bandwidth;
          max_bw_round = sender_->ExportDebugState().round_trip_count;
        }
        return sender_->ExportDebugState().is_at_full_bandwidth;
      },
      QuicTime::Delta::FromSeconds(5));
  ASSERT_TRUE(simulator_result);
  EXPECT_EQ(BbrSender::DRAIN, sender_->ExportDebugState().mode);
  EXPECT_GE(2u, sender_->ExportDebugState().round_trip_count - max_bw_round);
  EXPECT_EQ(1u, sender_->ExportDebugState().rounds_without_bandwidth_gain);
  EXPECT_NE(0u, bbr_sender_.connection()->GetStats().packets_lost);
  EXPECT_FALSE(sender_->ExportDebugState().last_sample_is_app_limited);
}

// Test slower pacing after loss in STARTUP due to the BBRS connection option.
TEST_F(BbrSenderTest, SimpleTransferSlowerStartup) {
  CreateSmallBufferSetup();

  SetConnectionOption(kBBRS);
  EXPECT_EQ(3u, sender_->num_startup_rtts());

  // Run until the full bandwidth is reached and check how many rounds it was.
  bbr_sender_.AddBytesToTransfer(12 * 1024 * 1024);
  QuicRoundTripCount max_bw_round = 0;
  QuicBandwidth max_bw(QuicBandwidth::Zero());
  bool simulator_result = simulator_.RunUntilOrTimeout(
      [this, &max_bw, &max_bw_round]() {
        if (max_bw < sender_->ExportDebugState().max_bandwidth) {
          max_bw = sender_->ExportDebugState().max_bandwidth;
          max_bw_round = sender_->ExportDebugState().round_trip_count;
        }
        // Expect the pacing rate in STARTUP to decrease once packet loss
        // is observed, but the CWND does not.
        if (bbr_sender_.connection()->GetStats().packets_lost > 0 &&
            !sender_->ExportDebugState().is_at_full_bandwidth &&
            sender_->has_non_app_limited_sample()) {
          EXPECT_EQ(1.5f * max_bw, sender_->PacingRate(0));
        }
        return sender_->ExportDebugState().is_at_full_bandwidth;
      },
      QuicTime::Delta::FromSeconds(5));
  ASSERT_TRUE(simulator_result);
  EXPECT_EQ(BbrSender::DRAIN, sender_->ExportDebugState().mode);
  EXPECT_GE(3u, sender_->ExportDebugState().round_trip_count - max_bw_round);
  EXPECT_EQ(3u, sender_->ExportDebugState().rounds_without_bandwidth_gain);
  EXPECT_NE(0u, bbr_sender_.connection()->GetStats().packets_lost);
  EXPECT_FALSE(sender_->ExportDebugState().last_sample_is_app_limited);
}

// Ensures no change in congestion window in STARTUP after loss.
TEST_F(BbrSenderTest, SimpleTransferNoConservationInStartup) {
  CreateSmallBufferSetup();

  SetConnectionOption(kBBS1);

  // Run until the full bandwidth is reached and check how many rounds it was.
  bbr_sender_.AddBytesToTransfer(12 * 1024 * 1024);
  bool used_conservation_cwnd = false;
  bool simulator_result = simulator_.RunUntilOrTimeout(
      [this, &used_conservation_cwnd]() {
        if (!sender_->ExportDebugState().is_at_full_bandwidth &&
            sender_->GetCongestionWindow() <
                sender_->ExportDebugState().congestion_window) {
          used_conservation_cwnd = true;
        }
        return sender_->ExportDebugState().is_at_full_bandwidth;
      },
      QuicTime::Delta::FromSeconds(5));
  ASSERT_TRUE(simulator_result);
  EXPECT_FALSE(used_conservation_cwnd);
  EXPECT_EQ(BbrSender::DRAIN, sender_->ExportDebugState().mode);
  EXPECT_EQ(3u, sender_->ExportDebugState().rounds_without_bandwidth_gain);
  EXPECT_NE(0u, bbr_sender_.connection()->GetStats().packets_lost);
  EXPECT_FALSE(sender_->ExportDebugState().last_sample_is_app_limited);
}

TEST_F(BbrSenderTest, DerivedPacingGainStartup) {
  SetQuicReloadableFlag(quic_bbr_slower_startup3, true);
  CreateDefaultSetup();

  SetConnectionOption(kBBQ1);
  EXPECT_EQ(3u, sender_->num_startup_rtts());
  // Verify that Sender is in slow start.
  EXPECT_TRUE(sender_->InSlowStart());
  // Verify that pacing rate is based on the initial RTT.
  QuicBandwidth expected_pacing_rate = QuicBandwidth::FromBytesAndTimeDelta(
      2.773 * kDefaultWindowTCP, rtt_stats_->initial_rtt());
  ExpectApproxEq(expected_pacing_rate.ToBitsPerSecond(),
                 sender_->PacingRate(0).ToBitsPerSecond(), 0.01f);

  // Run until the full bandwidth is reached and check how many rounds it was.
  bbr_sender_.AddBytesToTransfer(12 * 1024 * 1024);
  bool simulator_result = simulator_.RunUntilOrTimeout(
      [this]() { return sender_->ExportDebugState().is_at_full_bandwidth; },
      QuicTime::Delta::FromSeconds(5));
  ASSERT_TRUE(simulator_result);
  EXPECT_EQ(BbrSender::DRAIN, sender_->ExportDebugState().mode);
  EXPECT_EQ(3u, sender_->ExportDebugState().rounds_without_bandwidth_gain);
  ExpectApproxEq(kTestLinkBandwidth, sender_->ExportDebugState().max_bandwidth,
                 0.01f);
  EXPECT_EQ(0u, bbr_sender_.connection()->GetStats().packets_lost);
  EXPECT_FALSE(sender_->ExportDebugState().last_sample_is_app_limited);
}

TEST_F(BbrSenderTest, DerivedCWNDGainStartup) {
  SetQuicReloadableFlag(quic_bbr_slower_startup3, true);
  CreateDefaultSetup();

  SetConnectionOption(kBBQ2);
  EXPECT_EQ(3u, sender_->num_startup_rtts());
  // Verify that Sender is in slow start.
  EXPECT_TRUE(sender_->InSlowStart());
  // Verify that pacing rate is based on the initial RTT.
  QuicBandwidth expected_pacing_rate = QuicBandwidth::FromBytesAndTimeDelta(
      2.885 * kDefaultWindowTCP, rtt_stats_->initial_rtt());
  ExpectApproxEq(expected_pacing_rate.ToBitsPerSecond(),
                 sender_->PacingRate(0).ToBitsPerSecond(), 0.01f);

  // Run until the full bandwidth is reached and check how many rounds it was.
  bbr_sender_.AddBytesToTransfer(12 * 1024 * 1024);
  bool simulator_result = simulator_.RunUntilOrTimeout(
      [this]() { return sender_->ExportDebugState().is_at_full_bandwidth; },
      QuicTime::Delta::FromSeconds(5));
  ASSERT_TRUE(simulator_result);
  EXPECT_EQ(BbrSender::DRAIN, sender_->ExportDebugState().mode);
  EXPECT_EQ(3u, sender_->ExportDebugState().rounds_without_bandwidth_gain);
  ExpectApproxEq(kTestLinkBandwidth, sender_->ExportDebugState().max_bandwidth,
                 0.01f);
  EXPECT_EQ(0u, bbr_sender_.connection()->GetStats().packets_lost);
  EXPECT_FALSE(sender_->ExportDebugState().last_sample_is_app_limited);
  // Expect an SRTT less than 2.7 * Min RTT on exit from STARTUP.
  EXPECT_GT(kTestRtt * 2.7, rtt_stats_->smoothed_rtt());
}

TEST_F(BbrSenderTest, AckAggregationInStartup) {
  SetQuicReloadableFlag(quic_bbr_slower_startup3, true);
  // Disable Ack Decimation on the receiver to avoid loss and make results
  // consistent.
  QuicConnectionPeer::SetAckMode(receiver_.connection(),
                                 QuicConnection::AckMode::TCP_ACKING);
  CreateDefaultSetup();

  SetConnectionOption(kBBQ3);
  EXPECT_EQ(3u, sender_->num_startup_rtts());
  // Verify that Sender is in slow start.
  EXPECT_TRUE(sender_->InSlowStart());
  // Verify that pacing rate is based on the initial RTT.
  QuicBandwidth expected_pacing_rate = QuicBandwidth::FromBytesAndTimeDelta(
      2.885 * kDefaultWindowTCP, rtt_stats_->initial_rtt());
  ExpectApproxEq(expected_pacing_rate.ToBitsPerSecond(),
                 sender_->PacingRate(0).ToBitsPerSecond(), 0.01f);

  // Run until the full bandwidth is reached and check how many rounds it was.
  bbr_sender_.AddBytesToTransfer(12 * 1024 * 1024);
  bool simulator_result = simulator_.RunUntilOrTimeout(
      [this]() { return sender_->ExportDebugState().is_at_full_bandwidth; },
      QuicTime::Delta::FromSeconds(5));
  ASSERT_TRUE(simulator_result);
  EXPECT_EQ(BbrSender::DRAIN, sender_->ExportDebugState().mode);
  EXPECT_EQ(3u, sender_->ExportDebugState().rounds_without_bandwidth_gain);
  ExpectApproxEq(kTestLinkBandwidth, sender_->ExportDebugState().max_bandwidth,
                 0.01f);
  EXPECT_EQ(0u, bbr_sender_.connection()->GetStats().packets_lost);
  EXPECT_FALSE(sender_->ExportDebugState().last_sample_is_app_limited);
}

// Test that two BBR flows started slightly apart from each other terminate.
TEST_F(BbrSenderTest, SimpleCompetition) {
  const QuicByteCount transfer_size = 10 * 1024 * 1024;
  const QuicTime::Delta transfer_time =
      kTestLinkBandwidth.TransferTime(transfer_size);
  CreateBbrVsBbrSetup();

  // Transfer 10% of data in first transfer.
  bbr_sender_.AddBytesToTransfer(transfer_size);
  bool simulator_result = simulator_.RunUntilOrTimeout(
      [this]() { return receiver_.bytes_received() >= 0.1 * transfer_size; },
      transfer_time);
  ASSERT_TRUE(simulator_result);

  // Start the second transfer and wait until both finish.
  competing_sender_.AddBytesToTransfer(transfer_size);
  simulator_result = simulator_.RunUntilOrTimeout(
      [this]() {
        return receiver_.bytes_received() == transfer_size &&
               competing_receiver_.bytes_received() == transfer_size;
      },
      3 * transfer_time);
  ASSERT_TRUE(simulator_result);
}

// Test that BBR can resume bandwidth from cached network parameters.
TEST_F(BbrSenderTest, ResumeConnectionState) {
  CreateDefaultSetup();

  bbr_sender_.connection()->AdjustNetworkParameters(kTestLinkBandwidth,
                                                    kTestRtt);
  EXPECT_EQ(kTestLinkBandwidth, sender_->ExportDebugState().max_bandwidth);
  EXPECT_EQ(kTestLinkBandwidth, sender_->BandwidthEstimate());
  ExpectApproxEq(kTestRtt, sender_->ExportDebugState().min_rtt, 0.01f);

  DriveOutOfStartup();
}

// Test with a min CWND of 1 instead of 4 packets.
TEST_F(BbrSenderTest, ProbeRTTMinCWND1) {
  CreateDefaultSetup();
  SetConnectionOption(kMIN1);
  DriveOutOfStartup();

  // We have no intention of ever finishing this transfer.
  bbr_sender_.AddBytesToTransfer(100 * 1024 * 1024);

  // Wait until the connection enters PROBE_RTT.
  const QuicTime::Delta timeout = QuicTime::Delta::FromSeconds(12);
  bool simulator_result = simulator_.RunUntilOrTimeout(
      [this]() {
        return sender_->ExportDebugState().mode == BbrSender::PROBE_RTT;
      },
      timeout);
  ASSERT_TRUE(simulator_result);
  ASSERT_EQ(BbrSender::PROBE_RTT, sender_->ExportDebugState().mode);
  // The PROBE_RTT CWND should be 1 if the min CWND is 1.
  EXPECT_EQ(kDefaultTCPMSS, sender_->GetCongestionWindow());

  // Exit PROBE_RTT.
  const QuicTime probe_rtt_start = clock_->Now();
  const QuicTime::Delta time_to_exit_probe_rtt =
      kTestRtt + QuicTime::Delta::FromMilliseconds(200);
  simulator_.RunFor(1.5 * time_to_exit_probe_rtt);
  EXPECT_EQ(BbrSender::PROBE_BW, sender_->ExportDebugState().mode);
  EXPECT_GE(sender_->ExportDebugState().min_rtt_timestamp, probe_rtt_start);
}

}  // namespace test
}  // namespace quic
