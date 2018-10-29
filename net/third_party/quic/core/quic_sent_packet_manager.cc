// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/quic_sent_packet_manager.h"

#include <algorithm>

#include "net/third_party/quic/core/congestion_control/general_loss_algorithm.h"
#include "net/third_party/quic/core/congestion_control/pacing_sender.h"
#include "net/third_party/quic/core/crypto/crypto_protocol.h"
#include "net/third_party/quic/core/proto/cached_network_parameters.pb.h"
#include "net/third_party/quic/core/quic_connection_stats.h"
#include "net/third_party/quic/core/quic_pending_retransmission.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/platform/api/quic_bug_tracker.h"
#include "net/third_party/quic/platform/api/quic_flag_utils.h"
#include "net/third_party/quic/platform/api/quic_flags.h"
#include "net/third_party/quic/platform/api/quic_logging.h"
#include "net/third_party/quic/platform/api/quic_map_util.h"
#include "net/third_party/quic/platform/api/quic_string.h"

namespace quic {

namespace {
static const int64_t kDefaultRetransmissionTimeMs = 500;
static const int64_t kMaxRetransmissionTimeMs = 60000;
// Maximum number of exponential backoffs used for RTO timeouts.
static const size_t kMaxRetransmissions = 10;
// Maximum number of packets retransmitted upon an RTO.
static const size_t kMaxRetransmissionsOnTimeout = 2;
// The path degrading delay is the sum of this number of consecutive RTO delays.
const size_t kNumRetransmissionDelaysForPathDegradingDelay = 2;

// Ensure the handshake timer isnt't faster than 10ms.
// This limits the tenth retransmitted packet to 10s after the initial CHLO.
static const int64_t kMinHandshakeTimeoutMs = 10;

// Sends up to two tail loss probes before firing an RTO,
// per draft RFC draft-dukkipati-tcpm-tcp-loss-probe.
static const size_t kDefaultMaxTailLossProbes = 2;

inline bool HasCryptoHandshake(const QuicTransmissionInfo& transmission_info) {
  DCHECK(!transmission_info.has_crypto_handshake ||
         !transmission_info.retransmittable_frames.empty());
  return transmission_info.has_crypto_handshake;
}

// Returns true if retransmissions the specified type leave the data in flight.
inline bool RetransmissionLeavesBytesInFlight(
    TransmissionType transmission_type) {
  // Both TLP and the new RTO leave the packets in flight and let the loss
  // detection decide if packets are lost.
  return transmission_type == TLP_RETRANSMISSION ||
         transmission_type == PROBING_RETRANSMISSION ||
         transmission_type == RTO_RETRANSMISSION;
}

// Returns true of retransmissions of the specified type should retransmit
// the frames directly (as opposed to resulting in a loss notification).
inline bool ShouldForceRetransmission(TransmissionType transmission_type) {
  return transmission_type == HANDSHAKE_RETRANSMISSION ||
         transmission_type == TLP_RETRANSMISSION ||
         transmission_type == PROBING_RETRANSMISSION ||
         transmission_type == RTO_RETRANSMISSION;
}

}  // namespace

#define ENDPOINT \
  (perspective_ == Perspective::IS_SERVER ? "Server: " : "Client: ")

QuicSentPacketManager::QuicSentPacketManager(
    Perspective perspective,
    const QuicClock* clock,
    QuicConnectionStats* stats,
    CongestionControlType congestion_control_type,
    LossDetectionType loss_type)
    : unacked_packets_(),
      perspective_(perspective),
      clock_(clock),
      stats_(stats),
      debug_delegate_(nullptr),
      network_change_visitor_(nullptr),
      initial_congestion_window_(kInitialCongestionWindow),
      loss_algorithm_(&general_loss_algorithm_),
      general_loss_algorithm_(loss_type),
      n_connection_simulation_(false),
      first_rto_transmission_(0),
      consecutive_rto_count_(0),
      consecutive_tlp_count_(0),
      consecutive_crypto_retransmission_count_(0),
      pending_timer_transmission_count_(0),
      max_tail_loss_probes_(kDefaultMaxTailLossProbes),
      max_rto_packets_(kMaxRetransmissionsOnTimeout),
      enable_half_rtt_tail_loss_probe_(false),
      using_pacing_(false),
      use_new_rto_(false),
      conservative_handshake_retransmits_(false),
      min_tlp_timeout_(
          QuicTime::Delta::FromMilliseconds(kMinTailLossProbeTimeoutMs)),
      min_rto_timeout_(
          QuicTime::Delta::FromMilliseconds(kMinRetransmissionTimeMs)),
      ietf_style_tlp_(false),
      ietf_style_2x_tlp_(false),
      largest_newly_acked_(0),
      largest_mtu_acked_(0),
      handshake_confirmed_(false),
      largest_packet_peer_knows_is_acked_(0),
      delayed_ack_time_(
          QuicTime::Delta::FromMilliseconds(kDefaultDelayedAckTimeMs)),
      rtt_updated_(false),
      acked_packets_iter_(last_ack_frame_.packets.rbegin()),
      aggregate_acked_stream_frames_(
          GetQuicReloadableFlag(quic_aggregate_acked_stream_frames_2)),
      fix_mark_for_loss_retransmission_(
          GetQuicReloadableFlag(quic_fix_mark_for_loss_retransmission)) {
  SetSendAlgorithm(congestion_control_type);
}

QuicSentPacketManager::~QuicSentPacketManager() {}

void QuicSentPacketManager::SetFromConfig(const QuicConfig& config) {
  if (config.HasReceivedInitialRoundTripTimeUs() &&
      config.ReceivedInitialRoundTripTimeUs() > 0) {
    if (!config.HasClientSentConnectionOption(kNRTT, perspective_)) {
      SetInitialRtt(QuicTime::Delta::FromMicroseconds(
          config.ReceivedInitialRoundTripTimeUs()));
    }
  } else if (config.HasInitialRoundTripTimeUsToSend() &&
             config.GetInitialRoundTripTimeUsToSend() > 0) {
    SetInitialRtt(QuicTime::Delta::FromMicroseconds(
        config.GetInitialRoundTripTimeUsToSend()));
  }
  if (config.HasClientSentConnectionOption(kMAD0, perspective_)) {
    rtt_stats_.set_ignore_max_ack_delay(true);
  }
  if (config.HasClientSentConnectionOption(kMAD1, perspective_)) {
    rtt_stats_.set_initial_max_ack_delay(delayed_ack_time_);
  }
  if (config.HasClientSentConnectionOption(kMAD2, perspective_)) {
    min_tlp_timeout_ = QuicTime::Delta::Zero();
  }
  if (config.HasClientSentConnectionOption(kMAD3, perspective_)) {
    min_rto_timeout_ = QuicTime::Delta::Zero();
  }
  if (config.HasClientSentConnectionOption(kMAD4, perspective_)) {
    ietf_style_tlp_ = true;
  }
  if (config.HasClientSentConnectionOption(kMAD5, perspective_)) {
    ietf_style_2x_tlp_ = true;
  }

  // Configure congestion control.
  if (config.HasClientRequestedIndependentOption(kTBBR, perspective_)) {
    SetSendAlgorithm(kBBR);
  }
  if (config.HasClientRequestedIndependentOption(kRENO, perspective_)) {
    SetSendAlgorithm(kRenoBytes);
  } else if (config.HasClientRequestedIndependentOption(kBYTE, perspective_) ||
             (GetQuicReloadableFlag(quic_default_to_bbr) &&
              config.HasClientRequestedIndependentOption(kQBIC,
                                                         perspective_))) {
    SetSendAlgorithm(kCubicBytes);
  } else if (GetQuicReloadableFlag(quic_enable_pcc3) &&
             config.HasClientRequestedIndependentOption(kTPCC, perspective_)) {
    SetSendAlgorithm(kPCC);
  }
  // Initial window.
  if (GetQuicReloadableFlag(quic_unified_iw_options)) {
    if (config.HasClientRequestedIndependentOption(kIW03, perspective_)) {
      initial_congestion_window_ = 3;
      send_algorithm_->SetInitialCongestionWindowInPackets(3);
    }
    if (config.HasClientRequestedIndependentOption(kIW10, perspective_)) {
      initial_congestion_window_ = 10;
      send_algorithm_->SetInitialCongestionWindowInPackets(10);
    }
    if (config.HasClientRequestedIndependentOption(kIW20, perspective_)) {
      initial_congestion_window_ = 20;
      send_algorithm_->SetInitialCongestionWindowInPackets(20);
    }
    if (config.HasClientRequestedIndependentOption(kIW50, perspective_)) {
      initial_congestion_window_ = 50;
      send_algorithm_->SetInitialCongestionWindowInPackets(50);
    }
  }

  using_pacing_ = !FLAGS_quic_disable_pacing_for_perf_tests;

  if (config.HasClientSentConnectionOption(k1CON, perspective_)) {
    send_algorithm_->SetNumEmulatedConnections(1);
  }
  if (config.HasClientSentConnectionOption(kNCON, perspective_)) {
    n_connection_simulation_ = true;
  }
  if (config.HasClientSentConnectionOption(kNTLP, perspective_)) {
    max_tail_loss_probes_ = 0;
  }
  if (config.HasClientSentConnectionOption(k1TLP, perspective_)) {
    max_tail_loss_probes_ = 1;
  }
  if (config.HasClientSentConnectionOption(k1RTO, perspective_)) {
    max_rto_packets_ = 1;
  }
  if (config.HasClientSentConnectionOption(kTLPR, perspective_)) {
    enable_half_rtt_tail_loss_probe_ = true;
  }
  if (config.HasClientSentConnectionOption(kNRTO, perspective_)) {
    use_new_rto_ = true;
  }
  // Configure loss detection.
  if (config.HasClientRequestedIndependentOption(kTIME, perspective_)) {
    general_loss_algorithm_.SetLossDetectionType(kTime);
  }
  if (config.HasClientRequestedIndependentOption(kATIM, perspective_)) {
    general_loss_algorithm_.SetLossDetectionType(kAdaptiveTime);
  }
  if (config.HasClientRequestedIndependentOption(kLFAK, perspective_)) {
    general_loss_algorithm_.SetLossDetectionType(kLazyFack);
  }
  if (config.HasClientSentConnectionOption(kCONH, perspective_)) {
    conservative_handshake_retransmits_ = true;
  }
  send_algorithm_->SetFromConfig(config, perspective_);

  if (network_change_visitor_ != nullptr) {
    network_change_visitor_->OnCongestionChange();
  }
}

void QuicSentPacketManager::ResumeConnectionState(
    const CachedNetworkParameters& cached_network_params,
    bool max_bandwidth_resumption) {
  QuicBandwidth bandwidth = QuicBandwidth::FromBytesPerSecond(
      max_bandwidth_resumption
          ? cached_network_params.max_bandwidth_estimate_bytes_per_second()
          : cached_network_params.bandwidth_estimate_bytes_per_second());
  QuicTime::Delta rtt =
      QuicTime::Delta::FromMilliseconds(cached_network_params.min_rtt_ms());
  AdjustNetworkParameters(bandwidth, rtt);
}

void QuicSentPacketManager::AdjustNetworkParameters(QuicBandwidth bandwidth,
                                                    QuicTime::Delta rtt) {
  if (!rtt.IsZero()) {
    SetInitialRtt(rtt);
  }
  send_algorithm_->AdjustNetworkParameters(bandwidth, rtt);
  if (debug_delegate_ != nullptr) {
    debug_delegate_->OnAdjustNetworkParameters(bandwidth, rtt);
  }
}

void QuicSentPacketManager::SetNumOpenStreams(size_t num_streams) {
  if (n_connection_simulation_) {
    // Ensure the number of connections is between 1 and 5.
    send_algorithm_->SetNumEmulatedConnections(
        std::min<size_t>(5, std::max<size_t>(1, num_streams)));
  }
}

void QuicSentPacketManager::PostProcessAfterMarkingPacketHandled(
    const QuicAckFrame& ack_frame,
    QuicTime ack_receive_time,
    bool rtt_updated,
    QuicByteCount prior_bytes_in_flight) {
  if (aggregate_acked_stream_frames_ && session_decides_what_to_write()) {
    unacked_packets_.NotifyAggregatedStreamFrameAcked(
        last_ack_frame_.ack_delay_time);
  }
  InvokeLossDetection(ack_receive_time);
  // Ignore losses in RTO mode.
  if (consecutive_rto_count_ > 0 && !use_new_rto_) {
    packets_lost_.clear();
  }
  MaybeInvokeCongestionEvent(rtt_updated, prior_bytes_in_flight,
                             ack_receive_time);
  unacked_packets_.RemoveObsoletePackets();

  sustained_bandwidth_recorder_.RecordEstimate(
      send_algorithm_->InRecovery(), send_algorithm_->InSlowStart(),
      send_algorithm_->BandwidthEstimate(), ack_receive_time, clock_->WallNow(),
      rtt_stats_.smoothed_rtt());

  // Anytime we are making forward progress and have a new RTT estimate, reset
  // the backoff counters.
  if (rtt_updated) {
    if (consecutive_rto_count_ > 0) {
      // If the ack acknowledges data sent prior to the RTO,
      // the RTO was spurious.
      if (LargestAcked(ack_frame) < first_rto_transmission_) {
        // Replace SRTT with latest_rtt and increase the variance to prevent
        // a spurious RTO from happening again.
        rtt_stats_.ExpireSmoothedMetrics();
      } else {
        if (!use_new_rto_) {
          send_algorithm_->OnRetransmissionTimeout(true);
        }
      }
    }
    // Reset all retransmit counters any time a new packet is acked.
    consecutive_rto_count_ = 0;
    consecutive_tlp_count_ = 0;
    consecutive_crypto_retransmission_count_ = 0;
  }

  if (debug_delegate_ != nullptr) {
    debug_delegate_->OnIncomingAck(ack_frame, ack_receive_time,
                                   unacked_packets_.largest_acked(),
                                   rtt_updated, GetLeastUnacked());
  }
  // Remove packets below least unacked from all_packets_acked_ and
  // last_ack_frame_.
  last_ack_frame_.packets.RemoveUpTo(unacked_packets_.GetLeastUnacked());
  last_ack_frame_.received_packet_times.clear();
}

void QuicSentPacketManager::MaybeInvokeCongestionEvent(
    bool rtt_updated,
    QuicByteCount prior_in_flight,
    QuicTime event_time) {
  if (!rtt_updated && packets_acked_.empty() && packets_lost_.empty()) {
    return;
  }
  if (using_pacing_) {
    pacing_sender_.OnCongestionEvent(rtt_updated, prior_in_flight, event_time,
                                     packets_acked_, packets_lost_);
  } else {
    send_algorithm_->OnCongestionEvent(rtt_updated, prior_in_flight, event_time,
                                       packets_acked_, packets_lost_);
  }
  packets_acked_.clear();
  packets_lost_.clear();
  if (network_change_visitor_ != nullptr) {
    network_change_visitor_->OnCongestionChange();
  }
}

void QuicSentPacketManager::RetransmitUnackedPackets(
    TransmissionType retransmission_type) {
  DCHECK(retransmission_type == ALL_UNACKED_RETRANSMISSION ||
         retransmission_type == ALL_INITIAL_RETRANSMISSION);
  QuicPacketNumber packet_number = unacked_packets_.GetLeastUnacked();
  for (QuicUnackedPacketMap::const_iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it, ++packet_number) {
    if ((retransmission_type == ALL_UNACKED_RETRANSMISSION ||
         it->encryption_level == ENCRYPTION_INITIAL) &&
        unacked_packets_.HasRetransmittableFrames(*it)) {
      MarkForRetransmission(packet_number, retransmission_type);
    }
  }
}

void QuicSentPacketManager::NeuterUnencryptedPackets() {
  QuicPacketNumber packet_number = unacked_packets_.GetLeastUnacked();
  if (session_decides_what_to_write()) {
    for (QuicUnackedPacketMap::const_iterator it = unacked_packets_.begin();
         it != unacked_packets_.end(); ++it, ++packet_number) {
      if (!it->retransmittable_frames.empty() &&
          it->encryption_level == ENCRYPTION_NONE) {
        // Once the connection swithes to forward secure, no unencrypted packets
        // will be sent. The data has been abandoned in the cryto stream. Remove
        // it from in flight.
        unacked_packets_.RemoveFromInFlight(packet_number);
      }
    }
    return;
  }
  for (QuicUnackedPacketMap::const_iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it, ++packet_number) {
    if (it->encryption_level == ENCRYPTION_NONE &&
        unacked_packets_.HasRetransmittableFrames(*it)) {
      // Once you're forward secure, no unencrypted packets will be sent, crypto
      // or otherwise. Unencrypted packets are neutered and abandoned, to ensure
      // they are not retransmitted or considered lost from a congestion control
      // perspective.
      pending_retransmissions_.erase(packet_number);
      unacked_packets_.RemoveFromInFlight(packet_number);
      unacked_packets_.RemoveRetransmittability(packet_number);
    }
  }
}

void QuicSentPacketManager::MarkForRetransmission(
    QuicPacketNumber packet_number,
    TransmissionType transmission_type) {
  QuicTransmissionInfo* transmission_info =
      unacked_packets_.GetMutableTransmissionInfo(packet_number);
  // When session decides what to write, a previous RTO retransmission may cause
  // connection close; packets without retransmittable frames can be marked for
  // loss retransmissions.
  QUIC_BUG_IF((transmission_type != LOSS_RETRANSMISSION &&
               (!session_decides_what_to_write() ||
                transmission_type != RTO_RETRANSMISSION)) &&
              !unacked_packets_.HasRetransmittableFrames(*transmission_info))
      << "transmission_type: "
      << QuicUtils::TransmissionTypeToString(transmission_type);
  // Handshake packets should never be sent as probing retransmissions.
  DCHECK(!transmission_info->has_crypto_handshake ||
         transmission_type != PROBING_RETRANSMISSION);
  if (!RetransmissionLeavesBytesInFlight(transmission_type)) {
    unacked_packets_.RemoveFromInFlight(transmission_info);
  }

  if (!session_decides_what_to_write()) {
    if (fix_mark_for_loss_retransmission_ &&
        !unacked_packets_.HasRetransmittableFrames(*transmission_info)) {
      return;
    }
    if (!QuicContainsKey(pending_retransmissions_, packet_number)) {
      pending_retransmissions_[packet_number] = transmission_type;
    }
    return;
  }

  HandleRetransmission(transmission_type, transmission_info);

  // Update packet state according to transmission type.
  transmission_info->state =
      QuicUtils::RetransmissionTypeToPacketState(transmission_type);
}

void QuicSentPacketManager::HandleRetransmission(
    TransmissionType transmission_type,
    QuicTransmissionInfo* transmission_info) {
  DCHECK(session_decides_what_to_write());
  if (ShouldForceRetransmission(transmission_type)) {
    // TODO(fayang): Consider to make RTO and PROBING retransmission
    // strategies be configurable by applications. Today, TLP, RTO and PROBING
    // retransmissions are handled similarly, i.e., always retranmist the
    // oldest outstanding data. This is not ideal in general because different
    // applications may want different strategies. For example, some
    // applications may want to use higher priority stream data for bandwidth
    // probing, and some applications want to consider RTO is an indication of
    // loss, etc.
    unacked_packets_.RetransmitFrames(*transmission_info, transmission_type);
    return;
  }

  unacked_packets_.NotifyFramesLost(*transmission_info, transmission_type);
  if (!unacked_packets_.fix_is_useful_for_retransmission() ||
      transmission_info->retransmittable_frames.empty()) {
    return;
  }

  if (transmission_type == LOSS_RETRANSMISSION) {
    // Record the first packet sent after loss, which allows to wait 1
    // more RTT before giving up on this lost packet.
    transmission_info->retransmission =
        unacked_packets_.largest_sent_packet() + 1;
  } else {
    // Clear the recorded first packet sent after loss when version or
    // encryption changes.
    transmission_info->retransmission = 0;
  }
}

void QuicSentPacketManager::RecordOneSpuriousRetransmission(
    const QuicTransmissionInfo& info) {
  stats_->bytes_spuriously_retransmitted += info.bytes_sent;
  ++stats_->packets_spuriously_retransmitted;
  if (debug_delegate_ != nullptr) {
    debug_delegate_->OnSpuriousPacketRetransmission(info.transmission_type,
                                                    info.bytes_sent);
  }
}

void QuicSentPacketManager::RecordSpuriousRetransmissions(
    const QuicTransmissionInfo& info,
    QuicPacketNumber acked_packet_number) {
  if (session_decides_what_to_write()) {
    RecordOneSpuriousRetransmission(info);
    if (info.transmission_type == LOSS_RETRANSMISSION) {
      // Only inform the loss detection of spurious retransmits it caused.
      loss_algorithm_->SpuriousRetransmitDetected(
          unacked_packets_, clock_->Now(), rtt_stats_, acked_packet_number);
    }
    return;
  }
  QuicPacketNumber retransmission = info.retransmission;
  while (retransmission != 0) {
    const QuicTransmissionInfo& retransmit_info =
        unacked_packets_.GetTransmissionInfo(retransmission);
    retransmission = retransmit_info.retransmission;
    RecordOneSpuriousRetransmission(retransmit_info);
  }
  // Only inform the loss detection of spurious retransmits it caused.
  if (unacked_packets_.GetTransmissionInfo(info.retransmission)
          .transmission_type == LOSS_RETRANSMISSION) {
    loss_algorithm_->SpuriousRetransmitDetected(
        unacked_packets_, clock_->Now(), rtt_stats_, info.retransmission);
  }
}

QuicPendingRetransmission QuicSentPacketManager::NextPendingRetransmission() {
  QUIC_BUG_IF(pending_retransmissions_.empty())
      << "Unexpected call to NextPendingRetransmission() with empty pending "
      << "retransmission list. Corrupted memory usage imminent.";
  QUIC_BUG_IF(session_decides_what_to_write())
      << "Unexpected call to NextPendingRetransmission() when session handles "
         "retransmissions";
  QuicPacketNumber packet_number = pending_retransmissions_.begin()->first;
  TransmissionType transmission_type = pending_retransmissions_.begin()->second;
  if (unacked_packets_.HasPendingCryptoPackets()) {
    // Ensure crypto packets are retransmitted before other packets.
    for (const auto& pair : pending_retransmissions_) {
      if (HasCryptoHandshake(
              unacked_packets_.GetTransmissionInfo(pair.first))) {
        packet_number = pair.first;
        transmission_type = pair.second;
        break;
      }
    }
  }
  DCHECK(unacked_packets_.IsUnacked(packet_number)) << packet_number;
  const QuicTransmissionInfo& transmission_info =
      unacked_packets_.GetTransmissionInfo(packet_number);
  DCHECK(unacked_packets_.HasRetransmittableFrames(transmission_info));

  return QuicPendingRetransmission(packet_number, transmission_type,
                                   transmission_info);
}

QuicPacketNumber QuicSentPacketManager::GetNewestRetransmission(
    QuicPacketNumber packet_number,
    const QuicTransmissionInfo& transmission_info) const {
  if (unacked_packets_.fix_is_useful_for_retransmission() &&
      session_decides_what_to_write()) {
    return packet_number;
  }
  QuicPacketNumber retransmission = transmission_info.retransmission;
  while (retransmission != 0) {
    packet_number = retransmission;
    retransmission =
        unacked_packets_.GetTransmissionInfo(retransmission).retransmission;
  }
  return packet_number;
}

void QuicSentPacketManager::MarkPacketHandled(QuicPacketNumber packet_number,
                                              QuicTransmissionInfo* info,
                                              QuicTime::Delta ack_delay_time) {
  QuicPacketNumber newest_transmission =
      GetNewestRetransmission(packet_number, *info);
  // Remove the most recent packet, if it is pending retransmission.
  pending_retransmissions_.erase(newest_transmission);

  if (newest_transmission == packet_number) {
    // Try to aggregate acked stream frames if acked packet is not a
    // retransmission.
    const bool fast_path = aggregate_acked_stream_frames_ &&
                           session_decides_what_to_write() &&
                           info->transmission_type == NOT_RETRANSMISSION;
    if (fast_path) {
      unacked_packets_.MaybeAggregateAckedStreamFrame(*info, ack_delay_time);
    } else {
      if (aggregate_acked_stream_frames_ && session_decides_what_to_write()) {
        unacked_packets_.NotifyAggregatedStreamFrameAcked(ack_delay_time);
      }
      const bool new_data_acked =
          unacked_packets_.NotifyFramesAcked(*info, ack_delay_time);
      if (session_decides_what_to_write() && !new_data_acked &&
          info->transmission_type != NOT_RETRANSMISSION) {
        // Record as a spurious retransmission if this packet is a
        // retransmission and no new data gets acked.
        QUIC_DVLOG(1) << "Detect spurious retransmitted packet "
                      << packet_number << " transmission type: "
                      << QuicUtils::TransmissionTypeToString(
                             info->transmission_type);
        RecordSpuriousRetransmissions(*info, packet_number);
      }
    }
  } else {
    DCHECK(!session_decides_what_to_write());
    RecordSpuriousRetransmissions(*info, packet_number);
    // Remove the most recent packet from flight if it's a crypto handshake
    // packet, since they won't be acked now that one has been processed.
    // Other crypto handshake packets won't be in flight, only the newest
    // transmission of a crypto packet is in flight at once.
    // TODO(ianswett): Instead of handling all crypto packets special,
    // only handle nullptr encrypted packets in a special way.
    const QuicTransmissionInfo& newest_transmission_info =
        unacked_packets_.GetTransmissionInfo(newest_transmission);
    unacked_packets_.NotifyFramesAcked(newest_transmission_info,
                                       ack_delay_time);
    if (HasCryptoHandshake(newest_transmission_info)) {
      unacked_packets_.RemoveFromInFlight(newest_transmission);
    }
  }

  if (network_change_visitor_ != nullptr &&
      info->bytes_sent > largest_mtu_acked_) {
    largest_mtu_acked_ = info->bytes_sent;
    network_change_visitor_->OnPathMtuIncreased(largest_mtu_acked_);
  }
  unacked_packets_.RemoveFromInFlight(info);
  unacked_packets_.RemoveRetransmittability(info);
  info->state = ACKED;
}

bool QuicSentPacketManager::OnPacketSent(
    SerializedPacket* serialized_packet,
    QuicPacketNumber original_packet_number,
    QuicTime sent_time,
    TransmissionType transmission_type,
    HasRetransmittableData has_retransmittable_data) {
  QuicPacketNumber packet_number = serialized_packet->packet_number;
  DCHECK_LT(0u, packet_number);
  DCHECK(!unacked_packets_.IsUnacked(packet_number));
  QUIC_BUG_IF(serialized_packet->encrypted_length == 0)
      << "Cannot send empty packets.";

  if (original_packet_number != 0) {
    pending_retransmissions_.erase(original_packet_number);
  }

  if (pending_timer_transmission_count_ > 0) {
    --pending_timer_transmission_count_;
  }

  bool in_flight = has_retransmittable_data == HAS_RETRANSMITTABLE_DATA;
  if (using_pacing_) {
    pacing_sender_.OnPacketSent(
        sent_time, unacked_packets_.bytes_in_flight(), packet_number,
        serialized_packet->encrypted_length, has_retransmittable_data);
  } else {
    send_algorithm_->OnPacketSent(
        sent_time, unacked_packets_.bytes_in_flight(), packet_number,
        serialized_packet->encrypted_length, has_retransmittable_data);
  }

  unacked_packets_.AddSentPacket(serialized_packet, original_packet_number,
                                 transmission_type, sent_time, in_flight);
  // Reset the retransmission timer anytime a pending packet is sent.
  return in_flight;
}

void QuicSentPacketManager::OnRetransmissionTimeout() {
  DCHECK(unacked_packets_.HasInFlightPackets());
  DCHECK_EQ(0u, pending_timer_transmission_count_);
  // Handshake retransmission, timer based loss detection, TLP, and RTO are
  // implemented with a single alarm. The handshake alarm is set when the
  // handshake has not completed, the loss alarm is set when the loss detection
  // algorithm says to, and the TLP and  RTO alarms are set after that.
  // The TLP alarm is always set to run for under an RTO.
  switch (GetRetransmissionMode()) {
    case HANDSHAKE_MODE:
      ++stats_->crypto_retransmit_count;
      RetransmitCryptoPackets();
      return;
    case LOSS_MODE: {
      ++stats_->loss_timeout_count;
      QuicByteCount prior_in_flight = unacked_packets_.bytes_in_flight();
      const QuicTime now = clock_->Now();
      InvokeLossDetection(now);
      MaybeInvokeCongestionEvent(false, prior_in_flight, now);
      return;
    }
    case TLP_MODE:
      ++stats_->tlp_count;
      ++consecutive_tlp_count_;
      pending_timer_transmission_count_ = 1;
      // TLPs prefer sending new data instead of retransmitting data, so
      // give the connection a chance to write before completing the TLP.
      return;
    case RTO_MODE:
      ++stats_->rto_count;
      RetransmitRtoPackets();
      return;
  }
}

void QuicSentPacketManager::RetransmitCryptoPackets() {
  DCHECK_EQ(HANDSHAKE_MODE, GetRetransmissionMode());
  ++consecutive_crypto_retransmission_count_;
  bool packet_retransmitted = false;
  QuicPacketNumber packet_number = unacked_packets_.GetLeastUnacked();
  std::vector<QuicPacketNumber> crypto_retransmissions;
  for (QuicUnackedPacketMap::const_iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it, ++packet_number) {
    // Only retransmit frames which are in flight, and therefore have been sent.
    if (!it->in_flight ||
        (session_decides_what_to_write() && it->state != OUTSTANDING) ||
        !it->has_crypto_handshake ||
        !unacked_packets_.HasRetransmittableFrames(*it)) {
      continue;
    }
    packet_retransmitted = true;
    if (session_decides_what_to_write()) {
      crypto_retransmissions.push_back(packet_number);
    } else {
      MarkForRetransmission(packet_number, HANDSHAKE_RETRANSMISSION);
    }
    ++pending_timer_transmission_count_;
  }
  DCHECK(packet_retransmitted) << "No crypto packets found to retransmit.";
  if (session_decides_what_to_write()) {
    for (QuicPacketNumber retransmission : crypto_retransmissions) {
      MarkForRetransmission(retransmission, HANDSHAKE_RETRANSMISSION);
    }
  }
}

bool QuicSentPacketManager::MaybeRetransmitTailLossProbe() {
  if (pending_timer_transmission_count_ == 0) {
    return false;
  }
  if (!MaybeRetransmitOldestPacket(TLP_RETRANSMISSION)) {
    // If no tail loss probe can be sent, because there are no retransmittable
    // packets, execute a conventional RTO to abandon old packets.
    if (GetQuicReloadableFlag(quic_optimize_inflight_check)) {
      QUIC_FLAG_COUNT(quic_reloadable_flag_quic_optimize_inflight_check);
      pending_timer_transmission_count_ = 0;
      RetransmitRtoPackets();
    }
    return false;
  }
  return true;
}

bool QuicSentPacketManager::MaybeRetransmitOldestPacket(TransmissionType type) {
  QuicPacketNumber packet_number = unacked_packets_.GetLeastUnacked();
  for (QuicUnackedPacketMap::const_iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it, ++packet_number) {
    // Only retransmit frames which are in flight, and therefore have been sent.
    if (!it->in_flight ||
        (session_decides_what_to_write() && it->state != OUTSTANDING) ||
        !unacked_packets_.HasRetransmittableFrames(*it)) {
      continue;
    }
    MarkForRetransmission(packet_number, type);
    return true;
  }
  QUIC_DVLOG(1)
      << "No retransmittable packets, so RetransmitOldestPacket failed.";
  return false;
}

void QuicSentPacketManager::RetransmitRtoPackets() {
  QUIC_BUG_IF(pending_timer_transmission_count_ > 0)
      << "Retransmissions already queued:" << pending_timer_transmission_count_;
  // Mark two packets for retransmission.
  QuicPacketNumber packet_number = unacked_packets_.GetLeastUnacked();
  std::vector<QuicPacketNumber> retransmissions;
  for (QuicUnackedPacketMap::const_iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it, ++packet_number) {
    if ((!session_decides_what_to_write() || it->state == OUTSTANDING) &&
        unacked_packets_.HasRetransmittableFrames(*it) &&
        pending_timer_transmission_count_ < max_rto_packets_) {
      if (session_decides_what_to_write()) {
        retransmissions.push_back(packet_number);
      } else {
        MarkForRetransmission(packet_number, RTO_RETRANSMISSION);
      }
      ++pending_timer_transmission_count_;
    }
    // Abandon non-retransmittable data that's in flight to ensure it doesn't
    // fill up the congestion window.
    bool has_retransmissions = it->retransmission != 0;
    if (session_decides_what_to_write()) {
      has_retransmissions = it->state != OUTSTANDING;
    }
    if (it->in_flight && !has_retransmissions &&
        !unacked_packets_.HasRetransmittableFrames(*it)) {
      // Log only for non-retransmittable data.
      // Retransmittable data is marked as lost during loss detection, and will
      // be logged later.
      unacked_packets_.RemoveFromInFlight(packet_number);
      if (debug_delegate_ != nullptr) {
        debug_delegate_->OnPacketLoss(packet_number, RTO_RETRANSMISSION,
                                      clock_->Now());
      }
    }
  }
  if (pending_timer_transmission_count_ > 0) {
    if (consecutive_rto_count_ == 0) {
      first_rto_transmission_ = unacked_packets_.largest_sent_packet() + 1;
    }
    ++consecutive_rto_count_;
  }
  if (session_decides_what_to_write()) {
    for (QuicPacketNumber retransmission : retransmissions) {
      MarkForRetransmission(retransmission, RTO_RETRANSMISSION);
    }
  }
}

QuicSentPacketManager::RetransmissionTimeoutMode
QuicSentPacketManager::GetRetransmissionMode() const {
  DCHECK(unacked_packets_.HasInFlightPackets());
  if (!handshake_confirmed_ && unacked_packets_.HasPendingCryptoPackets()) {
    return HANDSHAKE_MODE;
  }
  if (loss_algorithm_->GetLossTimeout() != QuicTime::Zero()) {
    return LOSS_MODE;
  }
  if (consecutive_tlp_count_ < max_tail_loss_probes_) {
    if (GetQuicReloadableFlag(quic_optimize_inflight_check) ||
        unacked_packets_.HasUnackedRetransmittableFrames()) {
      return TLP_MODE;
    }
  }
  return RTO_MODE;
}

void QuicSentPacketManager::InvokeLossDetection(QuicTime time) {
  if (!packets_acked_.empty()) {
    DCHECK_LE(packets_acked_.front().packet_number,
              packets_acked_.back().packet_number);
    largest_newly_acked_ = packets_acked_.back().packet_number;
  }
  loss_algorithm_->DetectLosses(unacked_packets_, time, rtt_stats_,
                                largest_newly_acked_, packets_acked_,
                                &packets_lost_);
  for (const LostPacket& packet : packets_lost_) {
    ++stats_->packets_lost;
    if (debug_delegate_ != nullptr) {
      debug_delegate_->OnPacketLoss(packet.packet_number, LOSS_RETRANSMISSION,
                                    time);
    }

    if (fix_mark_for_loss_retransmission_ ||
        unacked_packets_.HasRetransmittableFrames(packet.packet_number)) {
      if (fix_mark_for_loss_retransmission_) {
        QUIC_FLAG_COUNT(
            quic_reloadable_flag_quic_fix_mark_for_loss_retransmission);
      }
      MarkForRetransmission(packet.packet_number, LOSS_RETRANSMISSION);
    } else {
      // Since we will not retransmit this, we need to remove it from
      // unacked_packets_.   This is either the current transmission of
      // a packet whose previous transmission has been acked or a packet that
      // has been TLP retransmitted.
      unacked_packets_.RemoveFromInFlight(packet.packet_number);
    }
  }
}

bool QuicSentPacketManager::MaybeUpdateRTT(QuicPacketNumber largest_acked,
                                           QuicTime::Delta ack_delay_time,
                                           QuicTime ack_receive_time) {
  // We rely on ack_delay_time to compute an RTT estimate, so we
  // only update rtt when the largest observed gets acked.
  if (!unacked_packets_.IsUnacked(largest_acked)) {
    return false;
  }
  // We calculate the RTT based on the highest ACKed packet number, the lower
  // packet numbers will include the ACK aggregation delay.
  const QuicTransmissionInfo& transmission_info =
      unacked_packets_.GetTransmissionInfo(largest_acked);
  // Ensure the packet has a valid sent time.
  if (transmission_info.sent_time == QuicTime::Zero()) {
    QUIC_BUG << "Acked packet has zero sent time, largest_acked:"
             << largest_acked;
    return false;
  }

  QuicTime::Delta send_delta = ack_receive_time - transmission_info.sent_time;
  rtt_stats_.UpdateRtt(send_delta, ack_delay_time, ack_receive_time);

  return true;
}

QuicTime::Delta QuicSentPacketManager::TimeUntilSend(QuicTime now) const {
  // The TLP logic is entirely contained within QuicSentPacketManager, so the
  // send algorithm does not need to be consulted.
  if (pending_timer_transmission_count_ > 0) {
    return QuicTime::Delta::Zero();
  }

  if (using_pacing_) {
    return pacing_sender_.TimeUntilSend(now,
                                        unacked_packets_.bytes_in_flight());
  }

  return send_algorithm_->CanSend(unacked_packets_.bytes_in_flight())
             ? QuicTime::Delta::Zero()
             : QuicTime::Delta::Infinite();
}

const QuicTime QuicSentPacketManager::GetRetransmissionTime() const {
  // Don't set the timer if there is nothing to retransmit or we've already
  // queued a tlp transmission and it hasn't been sent yet.
  if (!unacked_packets_.HasInFlightPackets() ||
      pending_timer_transmission_count_ > 0) {
    return QuicTime::Zero();
  }
  if (!GetQuicReloadableFlag(quic_optimize_inflight_check) &&
      !unacked_packets_.HasUnackedRetransmittableFrames()) {
    return QuicTime::Zero();
  }
  switch (GetRetransmissionMode()) {
    case HANDSHAKE_MODE:
      return unacked_packets_.GetLastCryptoPacketSentTime() +
             GetCryptoRetransmissionDelay();
    case LOSS_MODE:
      return loss_algorithm_->GetLossTimeout();
    case TLP_MODE: {
      // TODO(ianswett): When CWND is available, it would be preferable to
      // set the timer based on the earliest retransmittable packet.
      // Base the updated timer on the send time of the last packet.
      const QuicTime sent_time = unacked_packets_.GetLastPacketSentTime();
      const QuicTime tlp_time = sent_time + GetTailLossProbeDelay();
      // Ensure the TLP timer never gets set to a time in the past.
      return std::max(clock_->ApproximateNow(), tlp_time);
    }
    case RTO_MODE: {
      // The RTO is based on the first outstanding packet.
      const QuicTime sent_time = unacked_packets_.GetLastPacketSentTime();
      QuicTime rto_time = sent_time + GetRetransmissionDelay();
      // Wait for TLP packets to be acked before an RTO fires.
      QuicTime tlp_time =
          unacked_packets_.GetLastPacketSentTime() + GetTailLossProbeDelay();
      return std::max(tlp_time, rto_time);
    }
  }
  DCHECK(false);
  return QuicTime::Zero();
}

const QuicTime::Delta QuicSentPacketManager::GetPathDegradingDelay() const {
  QuicTime::Delta delay = QuicTime::Delta::Zero();
  for (size_t i = 0; i < max_tail_loss_probes_; ++i) {
    delay = delay + GetTailLossProbeDelay(i);
  }
  for (size_t i = 0; i < kNumRetransmissionDelaysForPathDegradingDelay; ++i) {
    delay = delay + GetRetransmissionDelay(i);
  }
  return delay;
}

const QuicTime::Delta QuicSentPacketManager::GetCryptoRetransmissionDelay()
    const {
  // This is equivalent to the TailLossProbeDelay, but slightly more aggressive
  // because crypto handshake messages don't incur a delayed ack time.
  QuicTime::Delta srtt = rtt_stats_.SmoothedOrInitialRtt();
  int64_t delay_ms;
  if (conservative_handshake_retransmits_) {
    // Using the delayed ack time directly could cause conservative handshake
    // retransmissions to actually be more aggressive than the default.
    delay_ms = std::max(delayed_ack_time_.ToMilliseconds(),
                        static_cast<int64_t>(2 * srtt.ToMilliseconds()));
  } else {
    delay_ms = std::max(kMinHandshakeTimeoutMs,
                        static_cast<int64_t>(1.5 * srtt.ToMilliseconds()));
  }
  return QuicTime::Delta::FromMilliseconds(
      delay_ms << consecutive_crypto_retransmission_count_);
}

const QuicTime::Delta QuicSentPacketManager::GetTailLossProbeDelay(
    size_t consecutive_tlp_count) const {
  QuicTime::Delta srtt = rtt_stats_.SmoothedOrInitialRtt();
  if (enable_half_rtt_tail_loss_probe_ && consecutive_tlp_count == 0u) {
    return std::max(min_tlp_timeout_, srtt * 0.5);
  }
  if (ietf_style_tlp_) {
    return std::max(min_tlp_timeout_, 1.5 * srtt + rtt_stats_.max_ack_delay());
  }
  if (ietf_style_2x_tlp_) {
    return std::max(min_tlp_timeout_, 2 * srtt + rtt_stats_.max_ack_delay());
  }
  if (!unacked_packets_.HasMultipleInFlightPackets()) {
    // This expression really should be using the delayed ack time, but in TCP
    // MinRTO was traditionally set to 2x the delayed ack timer and this
    // expression assumed QUIC did the same.
    return std::max(2 * srtt, 1.5 * srtt + (min_rto_timeout_ * 0.5));
  }
  return std::max(min_tlp_timeout_, 2 * srtt);
}

const QuicTime::Delta QuicSentPacketManager::GetRetransmissionDelay(
    size_t consecutive_rto_count) const {
  QuicTime::Delta retransmission_delay = QuicTime::Delta::Zero();
  if (rtt_stats_.smoothed_rtt().IsZero()) {
    // We are in the initial state, use default timeout values.
    retransmission_delay =
        QuicTime::Delta::FromMilliseconds(kDefaultRetransmissionTimeMs);
  } else {
    retransmission_delay =
        rtt_stats_.smoothed_rtt() + 4 * rtt_stats_.mean_deviation();
    if (retransmission_delay < min_rto_timeout_) {
      retransmission_delay = min_rto_timeout_;
    }
  }

  // Calculate exponential back off.
  retransmission_delay =
      retransmission_delay *
      (1 << std::min<size_t>(consecutive_rto_count, kMaxRetransmissions));

  if (retransmission_delay.ToMilliseconds() > kMaxRetransmissionTimeMs) {
    return QuicTime::Delta::FromMilliseconds(kMaxRetransmissionTimeMs);
  }
  return retransmission_delay;
}

QuicString QuicSentPacketManager::GetDebugState() const {
  return send_algorithm_->GetDebugState();
}

void QuicSentPacketManager::CancelRetransmissionsForStream(
    QuicStreamId stream_id) {
  if (session_decides_what_to_write()) {
    return;
  }
  unacked_packets_.CancelRetransmissionsForStream(stream_id);
  auto it = pending_retransmissions_.begin();
  while (it != pending_retransmissions_.end()) {
    if (unacked_packets_.HasRetransmittableFrames(it->first)) {
      ++it;
      continue;
    }
    it = pending_retransmissions_.erase(it);
  }
}

void QuicSentPacketManager::SetSendAlgorithm(
    CongestionControlType congestion_control_type) {
  SetSendAlgorithm(SendAlgorithmInterface::Create(
      clock_, &rtt_stats_, &unacked_packets_, congestion_control_type,
      QuicRandom::GetInstance(), stats_, initial_congestion_window_));
}

void QuicSentPacketManager::SetSendAlgorithm(
    SendAlgorithmInterface* send_algorithm) {
  send_algorithm_.reset(send_algorithm);
  pacing_sender_.set_sender(send_algorithm);
}

void QuicSentPacketManager::OnConnectionMigration(AddressChangeType type) {
  if (type == PORT_CHANGE || type == IPV4_SUBNET_CHANGE) {
    // Rtt and cwnd do not need to be reset when the peer address change is
    // considered to be caused by NATs.
    return;
  }
  consecutive_rto_count_ = 0;
  consecutive_tlp_count_ = 0;
  rtt_stats_.OnConnectionMigration();
  send_algorithm_->OnConnectionMigration();
}

void QuicSentPacketManager::OnAckFrameStart(QuicPacketNumber largest_acked,
                                            QuicTime::Delta ack_delay_time,
                                            QuicTime ack_receive_time) {
  DCHECK(packets_acked_.empty());
  DCHECK_LE(largest_acked, unacked_packets_.largest_sent_packet());
  rtt_updated_ =
      MaybeUpdateRTT(largest_acked, ack_delay_time, ack_receive_time);
  DCHECK_GE(largest_acked, unacked_packets_.largest_acked());
  last_ack_frame_.ack_delay_time = ack_delay_time;
  acked_packets_iter_ = last_ack_frame_.packets.rbegin();
}

void QuicSentPacketManager::OnAckRange(QuicPacketNumber start,
                                       QuicPacketNumber end) {
  if (end > last_ack_frame_.largest_acked + 1) {
    // Largest acked increases.
    unacked_packets_.IncreaseLargestAcked(end - 1);
    last_ack_frame_.largest_acked = end - 1;
  }
  // Drop ack ranges which ack packets below least_unacked.
  QuicPacketNumber least_unacked = unacked_packets_.GetLeastUnacked();
  if (end <= least_unacked) {
    return;
  }
  start = std::max(start, least_unacked);
  do {
    QuicPacketNumber newly_acked_start = start;
    if (acked_packets_iter_ != last_ack_frame_.packets.rend()) {
      newly_acked_start = std::max(start, acked_packets_iter_->max());
    }
    for (QuicPacketNumber acked = end - 1; acked >= newly_acked_start;
         --acked) {
      // Check if end is above the current range. If so add newly acked packets
      // in descending order.
      packets_acked_.push_back(AckedPacket(acked, 0, QuicTime::Zero()));
    }
    if (acked_packets_iter_ == last_ack_frame_.packets.rend() ||
        start > acked_packets_iter_->min()) {
      // Finish adding all newly acked packets.
      return;
    }
    end = std::min(end, acked_packets_iter_->min());
    ++acked_packets_iter_;
  } while (start < end);
}

void QuicSentPacketManager::OnAckTimestamp(QuicPacketNumber packet_number,
                                           QuicTime timestamp) {
  last_ack_frame_.received_packet_times.push_back({packet_number, timestamp});
  for (AckedPacket& packet : packets_acked_) {
    if (packet.packet_number == packet_number) {
      packet.receive_timestamp = timestamp;
      return;
    }
  }
}

bool QuicSentPacketManager::OnAckFrameEnd(QuicTime ack_receive_time) {
  QuicByteCount prior_bytes_in_flight = unacked_packets_.bytes_in_flight();
  // Reverse packets_acked_ so that it is in ascending order.
  reverse(packets_acked_.begin(), packets_acked_.end());
  for (AckedPacket& acked_packet : packets_acked_) {
    QuicTransmissionInfo* info =
        unacked_packets_.GetMutableTransmissionInfo(acked_packet.packet_number);
    if (!QuicUtils::IsAckable(info->state)) {
      if (info->state == ACKED) {
        QUIC_BUG << "Trying to ack an already acked packet: "
                 << acked_packet.packet_number
                 << ", last_ack_frame_: " << last_ack_frame_
                 << ", least_unacked: " << unacked_packets_.GetLeastUnacked()
                 << ", packets_acked_: " << packets_acked_;
      } else {
        QUIC_PEER_BUG << "Received ack for unackable packet: "
                      << acked_packet.packet_number << " with state: "
                      << QuicUtils::SentPacketStateToString(info->state);
      }
      continue;
    }
    QUIC_DVLOG(1) << ENDPOINT << "Got an ack for packet "
                  << acked_packet.packet_number;
    last_ack_frame_.packets.Add(acked_packet.packet_number);
    if (info->largest_acked > 0) {
      largest_packet_peer_knows_is_acked_ =
          std::max(largest_packet_peer_knows_is_acked_, info->largest_acked);
    }
    // If data is associated with the most recent transmission of this
    // packet, then inform the caller.
    if (info->in_flight) {
      acked_packet.bytes_acked = info->bytes_sent;
    } else {
      // Unackable packets are skipped earlier.
      largest_newly_acked_ = acked_packet.packet_number;
    }
    MarkPacketHandled(acked_packet.packet_number, info,
                      last_ack_frame_.ack_delay_time);
  }
  const bool acked_new_packet = !packets_acked_.empty();
  PostProcessAfterMarkingPacketHandled(last_ack_frame_, ack_receive_time,
                                       rtt_updated_, prior_bytes_in_flight);

  return acked_new_packet;
}

void QuicSentPacketManager::SetDebugDelegate(DebugDelegate* debug_delegate) {
  debug_delegate_ = debug_delegate;
}

void QuicSentPacketManager::OnApplicationLimited() {
  if (using_pacing_) {
    pacing_sender_.OnApplicationLimited();
  }
  send_algorithm_->OnApplicationLimited(unacked_packets_.bytes_in_flight());
  if (debug_delegate_ != nullptr) {
    debug_delegate_->OnApplicationLimited();
  }
}

QuicTime QuicSentPacketManager::GetNextReleaseTime() const {
  return using_pacing_ ? pacing_sender_.ideal_next_packet_send_time()
                       : QuicTime::Zero();
}

void QuicSentPacketManager::SetInitialRtt(QuicTime::Delta rtt) {
  const QuicTime::Delta min_rtt =
      QuicTime::Delta::FromMicroseconds(kMinInitialRoundTripTimeUs);
  const QuicTime::Delta max_rtt =
      QuicTime::Delta::FromMicroseconds(kMaxInitialRoundTripTimeUs);
  rtt_stats_.set_initial_rtt(std::max(min_rtt, std::min(max_rtt, rtt)));
}

#undef ENDPOINT  // undef for jumbo builds
}  // namespace quic
