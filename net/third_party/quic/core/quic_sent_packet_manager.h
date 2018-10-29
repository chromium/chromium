// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_QUIC_SENT_PACKET_MANAGER_H_
#define NET_THIRD_PARTY_QUIC_CORE_QUIC_SENT_PACKET_MANAGER_H_

#include <cstddef>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "net/third_party/quic/core/congestion_control/general_loss_algorithm.h"
#include "net/third_party/quic/core/congestion_control/loss_detection_interface.h"
#include "net/third_party/quic/core/congestion_control/pacing_sender.h"
#include "net/third_party/quic/core/congestion_control/rtt_stats.h"
#include "net/third_party/quic/core/congestion_control/send_algorithm_interface.h"
#include "net/third_party/quic/core/proto/cached_network_parameters.pb.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/quic/core/quic_pending_retransmission.h"
#include "net/third_party/quic/core/quic_sustained_bandwidth_recorder.h"
#include "net/third_party/quic/core/quic_transmission_info.h"
#include "net/third_party/quic/core/quic_types.h"
#include "net/third_party/quic/core/quic_unacked_packet_map.h"
#include "net/third_party/quic/platform/api/quic_containers.h"
#include "net/third_party/quic/platform/api/quic_export.h"
#include "net/third_party/quic/platform/api/quic_string.h"

namespace quic {

namespace test {
class QuicConnectionPeer;
class QuicSentPacketManagerPeer;
}  // namespace test

class QuicClock;
class QuicConfig;
struct QuicConnectionStats;

// Class which tracks the set of packets sent on a QUIC connection and contains
// a send algorithm to decide when to send new packets.  It keeps track of any
// retransmittable data associated with each packet. If a packet is
// retransmitted, it will keep track of each version of a packet so that if a
// previous transmission is acked, the data will not be retransmitted.
class QUIC_EXPORT_PRIVATE QuicSentPacketManager {
 public:
  // Interface which gets callbacks from the QuicSentPacketManager at
  // interesting points.  Implementations must not mutate the state of
  // the packet manager or connection as a result of these callbacks.
  class QUIC_EXPORT_PRIVATE DebugDelegate {
   public:
    virtual ~DebugDelegate() {}

    // Called when a spurious retransmission is detected.
    virtual void OnSpuriousPacketRetransmission(
        TransmissionType transmission_type,
        QuicByteCount byte_size) {}

    virtual void OnIncomingAck(const QuicAckFrame& ack_frame,
                               QuicTime ack_receive_time,
                               QuicPacketNumber largest_observed,
                               bool rtt_updated,
                               QuicPacketNumber least_unacked_sent_packet) {}

    virtual void OnPacketLoss(QuicPacketNumber lost_packet_number,
                              TransmissionType transmission_type,
                              QuicTime detection_time) {}

    virtual void OnApplicationLimited() {}

    virtual void OnAdjustNetworkParameters(QuicBandwidth bandwidth,
                                           QuicTime::Delta rtt) {}
  };

  // Interface which gets callbacks from the QuicSentPacketManager when
  // network-related state changes. Implementations must not mutate the
  // state of the packet manager as a result of these callbacks.
  class QUIC_EXPORT_PRIVATE NetworkChangeVisitor {
   public:
    virtual ~NetworkChangeVisitor() {}

    // Called when congestion window or RTT may have changed.
    virtual void OnCongestionChange() = 0;

    // Called when the Path MTU may have increased.
    virtual void OnPathMtuIncreased(QuicPacketLength packet_size) = 0;
  };

  QuicSentPacketManager(Perspective perspective,
                        const QuicClock* clock,
                        QuicConnectionStats* stats,
                        CongestionControlType congestion_control_type,
                        LossDetectionType loss_type);
  QuicSentPacketManager(const QuicSentPacketManager&) = delete;
  QuicSentPacketManager& operator=(const QuicSentPacketManager&) = delete;
  virtual ~QuicSentPacketManager();

  virtual void SetFromConfig(const QuicConfig& config);

  // Pass the CachedNetworkParameters to the send algorithm.
  void ResumeConnectionState(
      const CachedNetworkParameters& cached_network_params,
      bool max_bandwidth_resumption);

  void SetNumOpenStreams(size_t num_streams);

  void SetMaxPacingRate(QuicBandwidth max_pacing_rate) {
    pacing_sender_.set_max_pacing_rate(max_pacing_rate);
  }

  QuicBandwidth MaxPacingRate() const {
    return pacing_sender_.max_pacing_rate();
  }

  void SetHandshakeConfirmed() { handshake_confirmed_ = true; }

  // Requests retransmission of all unacked packets of |retransmission_type|.
  // The behavior of this method depends on the value of |retransmission_type|:
  // ALL_UNACKED_RETRANSMISSION - All unacked packets will be retransmitted.
  // This can happen, for example, after a version negotiation packet has been
  // received and all packets needs to be retransmitted with the new version.
  // ALL_INITIAL_RETRANSMISSION - Only initially encrypted packets will be
  // retransmitted. This can happen, for example, when a CHLO has been rejected
  // and the previously encrypted data needs to be encrypted with a new key.
  void RetransmitUnackedPackets(TransmissionType retransmission_type);

  // Notify the sent packet manager of an external network measurement or
  // prediction for either |bandwidth| or |rtt|; either can be empty.
  void AdjustNetworkParameters(QuicBandwidth bandwidth, QuicTime::Delta rtt);

  // Retransmits the oldest pending packet there is still a tail loss probe
  // pending.  Invoked after OnRetransmissionTimeout.
  bool MaybeRetransmitTailLossProbe();

  // Retransmits the oldest pending packet.
  bool MaybeRetransmitOldestPacket(TransmissionType type);

  // Removes the retransmittable frames from all unencrypted packets to ensure
  // they don't get retransmitted.
  void NeuterUnencryptedPackets();

  // Returns true if there are pending retransmissions.
  // Not const because retransmissions may be cancelled before returning.
  bool HasPendingRetransmissions() const {
    return !pending_retransmissions_.empty();
  }

  // Retrieves the next pending retransmission.  You must ensure that
  // there are pending retransmissions prior to calling this function.
  QuicPendingRetransmission NextPendingRetransmission();

  // Returns true if there's outstanding crypto data.
  bool HasUnackedCryptoPackets() const {
    return unacked_packets_.HasPendingCryptoPackets();
  }

  // Returns true if there are packets in flight expecting to be acknowledged.
  bool HasInFlightPackets() const {
    return unacked_packets_.HasInFlightPackets();
  }

  // Returns the smallest packet number of a serialized packet which has not
  // been acked by the peer.
  QuicPacketNumber GetLeastUnacked() const {
    return unacked_packets_.GetLeastUnacked();
  }

  // Called when we have sent bytes to the peer.  This informs the manager both
  // the number of bytes sent and if they were retransmitted.  Returns true if
  // the sender should reset the retransmission timer.
  bool OnPacketSent(SerializedPacket* serialized_packet,
                    QuicPacketNumber original_packet_number,
                    QuicTime sent_time,
                    TransmissionType transmission_type,
                    HasRetransmittableData has_retransmittable_data);

  // Called when the retransmission timer expires.
  void OnRetransmissionTimeout();

  // Calculate the time until we can send the next packet to the wire.
  // Note 1: When kUnknownWaitTime is returned, there is no need to poll
  // TimeUntilSend again until we receive an OnIncomingAckFrame event.
  // Note 2: Send algorithms may or may not use |retransmit| in their
  // calculations.
  QuicTime::Delta TimeUntilSend(QuicTime now) const;

  // Returns the current delay for the retransmission timer, which may send
  // either a tail loss probe or do a full RTO.  Returns QuicTime::Zero() if
  // there are no retransmittable packets.
  const QuicTime GetRetransmissionTime() const;

  // Returns the current delay for the path degrading timer, which is used to
  // notify the session that this connection is degrading.
  const QuicTime::Delta GetPathDegradingDelay() const;

  const RttStats* GetRttStats() const { return &rtt_stats_; }

  // Returns the estimated bandwidth calculated by the congestion algorithm.
  QuicBandwidth BandwidthEstimate() const {
    return send_algorithm_->BandwidthEstimate();
  }

  const QuicSustainedBandwidthRecorder* SustainedBandwidthRecorder() const {
    return &sustained_bandwidth_recorder_;
  }

  // Returns the size of the current congestion window in number of
  // kDefaultTCPMSS-sized segments. Note, this is not the *available* window.
  // Some send algorithms may not use a congestion window and will return 0.
  QuicPacketCount GetCongestionWindowInTcpMss() const {
    return send_algorithm_->GetCongestionWindow() / kDefaultTCPMSS;
  }

  // Returns the number of packets of length |max_packet_length| which fit in
  // the current congestion window. More packets may end up in flight if the
  // congestion window has been recently reduced, of if non-full packets are
  // sent.
  QuicPacketCount EstimateMaxPacketsInFlight(
      QuicByteCount max_packet_length) const {
    return send_algorithm_->GetCongestionWindow() / max_packet_length;
  }

  // Returns the size of the current congestion window size in bytes.
  QuicByteCount GetCongestionWindowInBytes() const {
    return send_algorithm_->GetCongestionWindow();
  }

  // Returns the size of the slow start congestion window in nume of 1460 byte
  // TCP segments, aka ssthresh.  Some send algorithms do not define a slow
  // start threshold and will return 0.
  QuicPacketCount GetSlowStartThresholdInTcpMss() const {
    return send_algorithm_->GetSlowStartThreshold() / kDefaultTCPMSS;
  }

  // Returns debugging information about the state of the congestion controller.
  QuicString GetDebugState() const;

  // Returns the number of bytes that are considered in-flight, i.e. not lost or
  // acknowledged.
  QuicByteCount GetBytesInFlight() const {
    return unacked_packets_.bytes_in_flight();
  }

  // No longer retransmit data for |stream_id|.
  void CancelRetransmissionsForStream(QuicStreamId stream_id);

  // Called when peer address changes and the connection migrates.
  void OnConnectionMigration(AddressChangeType type);

  // Called when an ack frame is initially parsed.
  void OnAckFrameStart(QuicPacketNumber largest_acked,
                       QuicTime::Delta ack_delay_time,
                       QuicTime ack_receive_time);

  // Called when ack range [start, end) is received. Populates packets_acked_
  // with newly acked packets.
  void OnAckRange(QuicPacketNumber start, QuicPacketNumber end);

  // Called when a timestamp is processed.  If it's present in packets_acked_,
  // the timestamp field is set.  Otherwise, the timestamp is ignored.
  void OnAckTimestamp(QuicPacketNumber packet_number, QuicTime timestamp);

  // Called when an ack frame is parsed completely. Returns true if a previously
  // -unacked packet is acked.
  bool OnAckFrameEnd(QuicTime ack_receive_time);

  // Called to enable/disable letting session decide what to write.
  void SetSessionDecideWhatToWrite(bool session_decides_what_to_write) {
    unacked_packets_.SetSessionDecideWhatToWrite(session_decides_what_to_write);
  }

  void SetDebugDelegate(DebugDelegate* debug_delegate);

  void SetPacingAlarmGranularity(QuicTime::Delta alarm_granularity) {
    pacing_sender_.set_alarm_granularity(alarm_granularity);
  }

  QuicPacketNumber GetLargestObserved() const {
    return unacked_packets_.largest_acked();
  }

  QuicPacketNumber GetLargestSentPacket() const {
    return unacked_packets_.largest_sent_packet();
  }

  void SetNetworkChangeVisitor(NetworkChangeVisitor* visitor) {
    DCHECK(!network_change_visitor_);
    DCHECK(visitor);
    network_change_visitor_ = visitor;
  }

  bool InSlowStart() const { return send_algorithm_->InSlowStart(); }

  size_t GetConsecutiveRtoCount() const { return consecutive_rto_count_; }

  size_t GetConsecutiveTlpCount() const { return consecutive_tlp_count_; }

  void OnApplicationLimited();

  const SendAlgorithmInterface* GetSendAlgorithm() const {
    return send_algorithm_.get();
  }

  void SetSessionNotifier(SessionNotifierInterface* session_notifier) {
    unacked_packets_.SetSessionNotifier(session_notifier);
  }

  QuicTime GetNextReleaseTime() const;

  QuicPacketCount initial_congestion_window() const {
    return initial_congestion_window_;
  }

  QuicPacketNumber largest_packet_peer_knows_is_acked() const {
    return largest_packet_peer_knows_is_acked_;
  }

  bool handshake_confirmed() const { return handshake_confirmed_; }

  bool session_decides_what_to_write() const {
    return unacked_packets_.session_decides_what_to_write();
  }

  size_t pending_timer_transmission_count() const {
    return pending_timer_transmission_count_;
  }

  QuicTime::Delta delayed_ack_time() const { return delayed_ack_time_; }

  void set_delayed_ack_time(QuicTime::Delta delayed_ack_time) {
    // The delayed ack time should never be more than one half the min RTO time.
    DCHECK_LE(delayed_ack_time, (min_rto_timeout_ * 0.5));
    delayed_ack_time_ = delayed_ack_time;
  }

  const QuicUnackedPacketMap& unacked_packets() const {
    return unacked_packets_;
  }

  // Sets the send algorithm to the given congestion control type and points the
  // pacing sender at |send_algorithm_|. Can be called any number of times.
  void SetSendAlgorithm(CongestionControlType congestion_control_type);

  // Sets the send algorithm to |send_algorithm| and points the pacing sender at
  // |send_algorithm_|. Takes ownership of |send_algorithm|. Can be called any
  // number of times.
  // Setting the send algorithm once the connection is underway is dangerous.
  void SetSendAlgorithm(SendAlgorithmInterface* send_algorithm);

 private:
  friend class test::QuicConnectionPeer;
  friend class test::QuicSentPacketManagerPeer;

  // The retransmission timer is a single timer which switches modes depending
  // upon connection state.
  enum RetransmissionTimeoutMode {
    // A conventional TCP style RTO.
    RTO_MODE,
    // A tail loss probe.  By default, QUIC sends up to two before RTOing.
    TLP_MODE,
    // Retransmission of handshake packets prior to handshake completion.
    HANDSHAKE_MODE,
    // Re-invoke the loss detection when a packet is not acked before the
    // loss detection algorithm expects.
    LOSS_MODE,
  };

  typedef QuicLinkedHashMap<QuicPacketNumber, TransmissionType>
      PendingRetransmissionMap;

  // Returns the current retransmission mode.
  RetransmissionTimeoutMode GetRetransmissionMode() const;

  // Retransmits all crypto stream packets.
  void RetransmitCryptoPackets();

  // Retransmits two packets for an RTO and removes any non-retransmittable
  // packets from flight.
  void RetransmitRtoPackets();

  // Returns the timeout for retransmitting crypto handshake packets.
  const QuicTime::Delta GetCryptoRetransmissionDelay() const;

  // Returns the timeout for a new tail loss probe. |consecutive_tlp_count| is
  // the number of consecutive tail loss probes that have already been sent.
  const QuicTime::Delta GetTailLossProbeDelay(
      size_t consecutive_tlp_count) const;

  // Calls GetTailLossProbeDelay() with values from the current state of this
  // packet manager as its params.
  const QuicTime::Delta GetTailLossProbeDelay() const {
    return GetTailLossProbeDelay(consecutive_tlp_count_);
  }

  // Returns the retransmission timeout, after which a full RTO occurs.
  // |consecutive_rto_count| is the number of consecutive RTOs that have already
  // occurred.
  const QuicTime::Delta GetRetransmissionDelay(
      size_t consecutive_rto_count) const;

  // Calls GetRetransmissionDelay() with values from the current state of this
  // packet manager as its params.
  const QuicTime::Delta GetRetransmissionDelay() const {
    return GetRetransmissionDelay(consecutive_rto_count_);
  }

  // Returns the newest transmission associated with a packet.
  QuicPacketNumber GetNewestRetransmission(
      QuicPacketNumber packet_number,
      const QuicTransmissionInfo& transmission_info) const;

  // Update the RTT if the ack is for the largest acked packet number.
  // Returns true if the rtt was updated.
  bool MaybeUpdateRTT(QuicPacketNumber largest_acked,
                      QuicTime::Delta ack_delay_time,
                      QuicTime ack_receive_time);

  // Invokes the loss detection algorithm and loses and retransmits packets if
  // necessary.
  void InvokeLossDetection(QuicTime time);

  // Invokes OnCongestionEvent if |rtt_updated| is true, there are pending acks,
  // or pending losses.  Clears pending acks and pending losses afterwards.
  // |prior_in_flight| is the number of bytes in flight before the losses or
  // acks, |event_time| is normally the timestamp of the ack packet which caused
  // the event, although it can be the time at which loss detection was
  // triggered.
  void MaybeInvokeCongestionEvent(bool rtt_updated,
                                  QuicByteCount prior_in_flight,
                                  QuicTime event_time);

  // Removes the retransmittability and in flight properties from the packet at
  // |info| due to receipt by the peer.
  void MarkPacketHandled(QuicPacketNumber packet_number,
                         QuicTransmissionInfo* info,
                         QuicTime::Delta ack_delay_time);

  // Request that |packet_number| be retransmitted after the other pending
  // retransmissions.  Does not add it to the retransmissions if it's already
  // a pending retransmission.
  void MarkForRetransmission(QuicPacketNumber packet_number,
                             TransmissionType transmission_type);

  // Performs whatever work is need to retransmit the data correctly, either
  // by retransmitting the frames directly or by notifying that the frames
  // are lost.
  void HandleRetransmission(TransmissionType transmission_type,
                            QuicTransmissionInfo* transmission_info);

  // Called after packets have been marked handled with last received ack frame.
  void PostProcessAfterMarkingPacketHandled(
      const QuicAckFrame& ack_frame,
      QuicTime ack_receive_time,
      bool rtt_updated,
      QuicByteCount prior_bytes_in_flight);

  // Notify observers that packet with QuicTransmissionInfo |info| is a spurious
  // retransmission. It is caller's responsibility to guarantee the packet with
  // QuicTransmissionInfo |info| is a spurious retransmission before calling
  // this function.
  void RecordOneSpuriousRetransmission(const QuicTransmissionInfo& info);

  // Notify observers about spurious retransmits of packet with
  // QuicTransmissionInfo |info|.
  void RecordSpuriousRetransmissions(const QuicTransmissionInfo& info,
                                     QuicPacketNumber acked_packet_number);

  // Sets the initial RTT of the connection.
  void SetInitialRtt(QuicTime::Delta rtt);

  // Newly serialized retransmittable packets are added to this map, which
  // contains owning pointers to any contained frames.  If a packet is
  // retransmitted, this map will contain entries for both the old and the new
  // packet. The old packet's retransmittable frames entry will be nullptr,
  // while the new packet's entry will contain the frames to retransmit.
  // If the old packet is acked before the new packet, then the old entry will
  // be removed from the map and the new entry's retransmittable frames will be
  // set to nullptr.
  QuicUnackedPacketMap unacked_packets_;

  // Pending retransmissions which have not been packetized and sent yet.
  PendingRetransmissionMap pending_retransmissions_;

  // Tracks if the connection was created by the server or the client.
  Perspective perspective_;

  const QuicClock* clock_;
  QuicConnectionStats* stats_;

  DebugDelegate* debug_delegate_;
  NetworkChangeVisitor* network_change_visitor_;
  QuicPacketCount initial_congestion_window_;
  RttStats rtt_stats_;
  std::unique_ptr<SendAlgorithmInterface> send_algorithm_;
  // Not owned. Always points to |general_loss_algorithm_| outside of tests.
  LossDetectionInterface* loss_algorithm_;
  GeneralLossAlgorithm general_loss_algorithm_;
  bool n_connection_simulation_;

  // Tracks the first RTO packet.  If any packet before that packet gets acked,
  // it indicates the RTO was spurious and should be reversed(F-RTO).
  QuicPacketNumber first_rto_transmission_;
  // Number of times the RTO timer has fired in a row without receiving an ack.
  size_t consecutive_rto_count_;
  // Number of times the tail loss probe has been sent.
  size_t consecutive_tlp_count_;
  // Number of times the crypto handshake has been retransmitted.
  size_t consecutive_crypto_retransmission_count_;
  // Number of pending transmissions of TLP, RTO, or crypto packets.
  size_t pending_timer_transmission_count_;
  // Maximum number of tail loss probes to send before firing an RTO.
  size_t max_tail_loss_probes_;
  // Maximum number of packets to send upon RTO.
  QuicPacketCount max_rto_packets_;
  // If true, send the TLP at 0.5 RTT.
  bool enable_half_rtt_tail_loss_probe_;
  bool using_pacing_;
  // If true, use the new RTO with loss based CWND reduction instead of the send
  // algorithms's OnRetransmissionTimeout to reduce the congestion window.
  bool use_new_rto_;
  // If true, use a more conservative handshake retransmission policy.
  bool conservative_handshake_retransmits_;
  // The minimum TLP timeout.
  QuicTime::Delta min_tlp_timeout_;
  // The minimum RTO.
  QuicTime::Delta min_rto_timeout_;
  // Whether to use IETF style TLP that includes the max ack delay.
  bool ietf_style_tlp_;
  // IETF style TLP, but with a 2x multiplier instead of 1.5x.
  bool ietf_style_2x_tlp_;

  // Vectors packets acked and lost as a result of the last congestion event.
  AckedPacketVector packets_acked_;
  LostPacketVector packets_lost_;
  // Largest newly acknowledged packet.
  QuicPacketNumber largest_newly_acked_;
  // Largest packet in bytes ever acknowledged.
  QuicPacketLength largest_mtu_acked_;

  // Replaces certain calls to |send_algorithm_| when |using_pacing_| is true.
  // Calls into |send_algorithm_| for the underlying congestion control.
  PacingSender pacing_sender_;

  // Set to true after the crypto handshake has successfully completed. After
  // this is true we no longer use HANDSHAKE_MODE, and further frames sent on
  // the crypto stream (i.e. SCUP messages) are treated like normal
  // retransmittable frames.
  bool handshake_confirmed_;

  // Records bandwidth from server to client in normal operation, over periods
  // of time with no loss events.
  QuicSustainedBandwidthRecorder sustained_bandwidth_recorder_;

  // The largest acked value that was sent in an ack, which has then been acked.
  QuicPacketNumber largest_packet_peer_knows_is_acked_;

  // The maximum amount of time to wait before sending an acknowledgement.
  // The recovery code assumes the delayed ack time is the same on both sides.
  QuicTime::Delta delayed_ack_time_;

  // Latest received ack frame.
  QuicAckFrame last_ack_frame_;

  // Record whether RTT gets updated by last largest acked..
  bool rtt_updated_;

  // A reverse iterator of last_ack_frame_.packets. This is reset in
  // OnAckRangeStart, and gradually moves in OnAckRange..
  PacketNumberQueue::const_reverse_iterator acked_packets_iter_;

  // Latched value of quic_reloadable_flag_quic_aggregate_acked_stream_frames_2.
  const bool aggregate_acked_stream_frames_;

  // Latched value of
  // quic_reloadable_flag_quic_fix_mark_for_loss_retransmission.
  const bool fix_mark_for_loss_retransmission_;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_QUIC_SENT_PACKET_MANAGER_H_
