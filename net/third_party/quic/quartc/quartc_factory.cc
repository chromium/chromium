// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/quartc/quartc_factory.h"

#include "net/third_party/quic/core/crypto/quic_random.h"
#include "net/third_party/quic/platform/api/quic_goog_cc_sender.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/platform/api/quic_socket_address.h"
#include "net/third_party/quic/quartc/quartc_session.h"

namespace quic {

QuartcFactory::QuartcFactory(const QuartcFactoryConfig& factory_config)
    : alarm_factory_(factory_config.alarm_factory),
      clock_(factory_config.clock) {}

QuartcFactory::~QuartcFactory() {}

std::unique_ptr<QuartcSession> QuartcFactory::CreateQuartcSession(
    const QuartcSessionConfig& quartc_session_config) {
  DCHECK(quartc_session_config.packet_transport);

  Perspective perspective = quartc_session_config.perspective;

  // QuartcSession will eventually own both |writer| and |quic_connection|.
  auto writer =
      QuicMakeUnique<QuartcPacketWriter>(quartc_session_config.packet_transport,
                                         quartc_session_config.max_packet_size);

  // Fixes behavior of StopReading() with level-triggered stream sequencers.
  SetQuicReloadableFlag(quic_stop_reading_when_level_triggered, true);

  // Quartc uses zombie streams -- we close the stream for read as soon as we
  // create a stream -- this makes the stream a zombie stream. b/115323618
  // revealed that closing zombie streams is problematic, and enabling this flag
  // fixes it.
  SetQuicReloadableFlag(quic_fix_reset_zombie_streams, true);

  std::unique_ptr<QuicConnection> quic_connection =
      CreateQuicConnection(perspective, writer.get());

  QuicTagVector copt;
  copt.push_back(kNSTP);

  // Enable and request QUIC to include receive timestamps in ACK frames.
  SetQuicReloadableFlag(quic_send_timestamps, true);
  copt.push_back(kSTMP);

  // Enable ACK_DECIMATION_WITH_REORDERING. It requires ack_decimation to be
  // false.
  SetQuicReloadableFlag(quic_enable_ack_decimation, false);
  copt.push_back(kAKD2);

  // Enable time-based loss detection.
  copt.push_back(kTIME);

  switch (quartc_session_config.congestion_control_type) {
    case kBBR:
      copt.push_back(kTBBR);
      break;
    case kGoogCC: {
      QuicSentPacketManager& sent_packet_manager =
          quic_connection->sent_packet_manager();
      SendAlgorithmInterface* sender = CreateGoogCcSender(
          clock_, sent_packet_manager.GetRttStats(),
          &sent_packet_manager.unacked_packets(), GetRandomGenerator(),
          /*stats=*/nullptr, sent_packet_manager.initial_congestion_window(),
          kDefaultMaxCongestionWindowPackets);
      sent_packet_manager.SetSendAlgorithm(sender);
      break;
    }
    case kCubicBytes:
      QUIC_LOG(FATAL) << "kCubicBytes is not supported";
      break;
    case kRenoBytes:
      QUIC_LOG(FATAL) << "kRenoBytes is not supported";
      break;
    case kPCC:
      QUIC_LOG(FATAL) << "kPCC is not supported";
      break;
  }

  // Note: flag settings have no effect for Exoblaze builds since
  // SetQuicReloadableFlag() gets stubbed out.
  SetQuicReloadableFlag(quic_bbr_less_probe_rtt, true);   // Enable BBR6,7,8.
  SetQuicReloadableFlag(quic_unified_iw_options, true);   // Enable IWXX opts.
  SetQuicReloadableFlag(quic_bbr_slower_startup3, true);  // Enable BBQX opts.
  SetQuicReloadableFlag(quic_bbr_flexible_app_limited, true);  // Enable BBR9.
  copt.push_back(kBBR3);  // Stay in low-gain until in-flight < BDP.
  copt.push_back(kBBR5);  // 40 RTT ack aggregation.
  copt.push_back(kBBR6);  // Use a 0.75 * BDP cwnd during PROBE_RTT.
  copt.push_back(kBBR8);  // Skip PROBE_RTT if app-limited.
  copt.push_back(kBBR9);  // Ignore app-limited if enough data is in flight.
  copt.push_back(kBBQ1);  // 2.773 pacing gain in STARTUP.
  copt.push_back(kBBQ2);  // 2.0 CWND gain in STARTUP.
  copt.push_back(kBBQ4);  // 0.75 pacing gain in DRAIN.
  copt.push_back(k1RTT);  // Exit STARTUP after 1 RTT with no gains.
  copt.push_back(kIW10);  // 10-packet (14600 byte) initial cwnd.

  quic_connection->set_fill_up_link_during_probing(true);

  // TODO(b/112192153):  Test and possible enable slower startup when pipe
  // filling is ready to use.  Slower startup is kBBRS.

  QuicConfig quic_config;

  // Use the limits for the session & stream flow control. The default 16KB
  // limit leads to significantly undersending (not reaching BWE on the outgoing
  // bitrate) due to blocked frames, and it leads to high latency (and one-way
  // delay). Setting it to its limits is not going to cause issues (our streams
  // are small generally, and if we were to buffer 24MB it wouldn't be the end
  // of the world). We can consider setting different limits in future (e.g. 1MB
  // stream, 1.5MB session). It's worth noting that on 1mbps bitrate, limit of
  // 24MB can capture approx 4 minutes of the call, and the default increase in
  // size of the window (half of the window size) is approximately 2 minutes of
  // the call.
  quic_config.SetInitialSessionFlowControlWindowToSend(
      kSessionReceiveWindowLimit);
  quic_config.SetInitialStreamFlowControlWindowToSend(
      kStreamReceiveWindowLimit);
  quic_config.SetConnectionOptionsToSend(copt);
  quic_config.SetClientConnectionOptions(copt);
  if (quartc_session_config.max_time_before_crypto_handshake >
      QuicTime::Delta::Zero()) {
    quic_config.set_max_time_before_crypto_handshake(
        quartc_session_config.max_time_before_crypto_handshake);
  }
  if (quartc_session_config.max_idle_time_before_crypto_handshake >
      QuicTime::Delta::Zero()) {
    quic_config.set_max_idle_time_before_crypto_handshake(
        quartc_session_config.max_idle_time_before_crypto_handshake);
  }
  if (quartc_session_config.idle_network_timeout > QuicTime::Delta::Zero()) {
    quic_config.SetIdleNetworkTimeout(
        quartc_session_config.idle_network_timeout,
        quartc_session_config.idle_network_timeout);
  }

  // The ICE transport provides a unique 5-tuple for each connection. Save
  // overhead by omitting the connection id.
  quic_config.SetBytesForConnectionIdToSend(0);

  // Allow up to 1000 incoming streams at once. Quartc streams typically contain
  // one audio or video frame and close immediately. However, when a video frame
  // becomes larger than one packet, there is some delay between the start and
  // end of each stream. The default maximum of 100 only leaves about 1 second
  // of headroom (Quartc sends ~30 video frames per second) before QUIC starts
  // to refuse incoming streams. Back-pressure should clear backlogs of
  // incomplete streams, but targets 1 second for recovery. Increasing the
  // number of open streams gives sufficient headroom to recover before QUIC
  // refuses new streams.
  quic_config.SetMaxIncomingDynamicStreamsToSend(1000);
  return QuicMakeUnique<QuartcSession>(
      std::move(quic_connection), quic_config, CurrentSupportedVersions(),
      quartc_session_config.unique_remote_server_id, perspective,
      this /*QuicConnectionHelperInterface*/, clock_, std::move(writer));
}

std::unique_ptr<QuicConnection> QuartcFactory::CreateQuicConnection(
    Perspective perspective,
    QuartcPacketWriter* packet_writer) {
  // dummy_id and dummy_address are used because Quartc network layer will not
  // use these two.
  QuicConnectionId dummy_id = 0;
  QuicSocketAddress dummy_address(QuicIpAddress::Any4(), 0 /*Port*/);
  return QuicMakeUnique<QuicConnection>(
      dummy_id, dummy_address, this, /*QuicConnectionHelperInterface*/
      alarm_factory_ /*QuicAlarmFactory*/, packet_writer, /*owns_writer=*/false,
      perspective, CurrentSupportedVersions());
}

const QuicClock* QuartcFactory::GetClock() const {
  return clock_;
}

QuicRandom* QuartcFactory::GetRandomGenerator() {
  return QuicRandom::GetInstance();
}

QuicBufferAllocator* QuartcFactory::GetStreamSendBufferAllocator() {
  return &buffer_allocator_;
}

std::unique_ptr<QuartcFactory> CreateQuartcFactory(
    const QuartcFactoryConfig& factory_config) {
  return std::unique_ptr<QuartcFactory>(new QuartcFactory(factory_config));
}

}  // namespace quic
