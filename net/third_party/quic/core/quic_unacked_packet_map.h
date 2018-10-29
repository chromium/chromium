// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_QUIC_UNACKED_PACKET_MAP_H_
#define NET_THIRD_PARTY_QUIC_CORE_QUIC_UNACKED_PACKET_MAP_H_

#include <cstddef>
#include <deque>

#include "base/macros.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/quic/core/quic_transmission_info.h"
#include "net/third_party/quic/core/session_notifier_interface.h"
#include "net/third_party/quic/platform/api/quic_export.h"

namespace quic {

namespace test {
class QuicUnackedPacketMapPeer;
}  // namespace test

// Class which tracks unacked packets for three purposes:
// 1) Track retransmittable data, including multiple transmissions of frames.
// 2) Track packets and bytes in flight for congestion control.
// 3) Track sent time of packets to provide RTT measurements from acks.
class QUIC_EXPORT_PRIVATE QuicUnackedPacketMap {
 public:
  QuicUnackedPacketMap();
  QuicUnackedPacketMap(const QuicUnackedPacketMap&) = delete;
  QuicUnackedPacketMap& operator=(const QuicUnackedPacketMap&) = delete;
  ~QuicUnackedPacketMap();

  // Adds |serialized_packet| to the map and marks it as sent at |sent_time|.
  // Marks the packet as in flight if |set_in_flight| is true.
  // Packets marked as in flight are expected to be marked as missing when they
  // don't arrive, indicating the need for retransmission.
  // |old_packet_number| is the packet number of the previous transmission,
  // or 0 if there was none.
  // Any AckNotifierWrappers in |serialized_packet| are swapped from the
  // serialized packet into the QuicTransmissionInfo.
  void AddSentPacket(SerializedPacket* serialized_packet,
                     QuicPacketNumber old_packet_number,
                     TransmissionType transmission_type,
                     QuicTime sent_time,
                     bool set_in_flight);

  // Returns true if the packet |packet_number| is unacked.
  bool IsUnacked(QuicPacketNumber packet_number) const;

  // Notifies session_notifier that frames have been acked. Returns true if any
  // new data gets acked, returns false otherwise.
  bool NotifyFramesAcked(const QuicTransmissionInfo& info,
                         QuicTime::Delta ack_delay);

  // Notifies session_notifier that frames in |info| are considered as lost.
  void NotifyFramesLost(const QuicTransmissionInfo& info,
                        TransmissionType type);

  // Notifies session_notifier to retransmit frames in |info| with
  // |transmission_type|.
  void RetransmitFrames(const QuicTransmissionInfo& info,
                        TransmissionType type);

  // Marks |info| as no longer in flight.
  void RemoveFromInFlight(QuicTransmissionInfo* info);

  // Marks |packet_number| as no longer in flight.
  void RemoveFromInFlight(QuicPacketNumber packet_number);

  // No longer retransmit data for |stream_id|.
  void CancelRetransmissionsForStream(QuicStreamId stream_id);

  // Returns true if |packet_number| has retransmittable frames. This will
  // return false if all frames of this packet are either non-retransmittable or
  // have been acked.
  bool HasRetransmittableFrames(QuicPacketNumber packet_number) const;

  // Returns true if |info| has retransmittable frames. This will return false
  // if all frames of this packet are either non-retransmittable or have been
  // acked.
  bool HasRetransmittableFrames(const QuicTransmissionInfo& info) const;

  // Returns true if there are any unacked packets which have retransmittable
  // frames.
  bool HasUnackedRetransmittableFrames() const;

  // Returns true if there are no packets present in the unacked packet map.
  bool empty() const { return unacked_packets_.empty(); }

  // Returns the largest packet number that has been sent.
  QuicPacketNumber largest_sent_packet() const { return largest_sent_packet_; }

  // Returns the largest retransmittable packet number that has been sent.
  QuicPacketNumber largest_sent_retransmittable_packet() const {
    return largest_sent_retransmittable_packet_;
  }

  QuicPacketNumber largest_sent_largest_acked() const {
    return largest_sent_largest_acked_;
  }

  // Returns the largest packet number that has been acked.
  QuicPacketNumber largest_acked() const { return largest_acked_; }

  // Returns the sum of bytes from all packets in flight.
  QuicByteCount bytes_in_flight() const { return bytes_in_flight_; }

  // Returns the smallest packet number of a serialized packet which has not
  // been acked by the peer.  If there are no unacked packets, returns 0.
  QuicPacketNumber GetLeastUnacked() const;

  // This can not be a QuicDeque since pointers into this are
  // assumed to be stable.
  typedef std::deque<QuicTransmissionInfo> UnackedPacketMap;

  typedef UnackedPacketMap::const_iterator const_iterator;
  typedef UnackedPacketMap::iterator iterator;

  const_iterator begin() const { return unacked_packets_.begin(); }
  const_iterator end() const { return unacked_packets_.end(); }
  iterator begin() { return unacked_packets_.begin(); }
  iterator end() { return unacked_packets_.end(); }

  // Returns true if there are unacked packets that are in flight.
  bool HasInFlightPackets() const;

  // Returns the QuicTransmissionInfo associated with |packet_number|, which
  // must be unacked.
  const QuicTransmissionInfo& GetTransmissionInfo(
      QuicPacketNumber packet_number) const;

  // Returns mutable QuicTransmissionInfo associated with |packet_number|, which
  // must be unacked.
  QuicTransmissionInfo* GetMutableTransmissionInfo(
      QuicPacketNumber packet_number);

  // Returns the time that the last unacked packet was sent.
  QuicTime GetLastPacketSentTime() const;

  // Returns the time that the last unacked crypto packet was sent.
  QuicTime GetLastCryptoPacketSentTime() const;

  // Returns the number of unacked packets.
  size_t GetNumUnackedPacketsDebugOnly() const;

  // Returns true if there are multiple packets in flight.
  bool HasMultipleInFlightPackets() const;

  // Returns true if there are any pending crypto packets.
  // TODO(fayang): Remove this method and call session_notifier_'s
  // HasUnackedCryptoData() when session_decides_what_to_write_ is default true.
  bool HasPendingCryptoPackets() const;

  // Removes any retransmittable frames from this transmission or an associated
  // transmission.  It removes now useless transmissions, and disconnects any
  // other packets from other transmissions.
  void RemoveRetransmittability(QuicTransmissionInfo* info);

  // Looks up the QuicTransmissionInfo by |packet_number| and calls
  // RemoveRetransmittability.
  void RemoveRetransmittability(QuicPacketNumber packet_number);

  // Increases the largest acked.  Any packets less or equal to
  // |largest_acked| are discarded if they are only for the RTT purposes.
  void IncreaseLargestAcked(QuicPacketNumber largest_acked);

  // Remove any packets no longer needed for retransmission, congestion, or
  // RTT measurement purposes.
  void RemoveObsoletePackets();

  // Try to aggregate acked contiguous stream frames. For noncontiguous stream
  // frames or control frames, notify the session notifier they get acked
  // immediately.
  void MaybeAggregateAckedStreamFrame(const QuicTransmissionInfo& info,
                                      QuicTime::Delta ack_delay);

  // Notify the session notifier of any stream data aggregated in
  // aggregated_stream_frame_.  No effect if the stream frame has an invalid
  // stream id.
  void NotifyAggregatedStreamFrameAcked(QuicTime::Delta ack_delay);

  // Called to start/stop letting session decide what to write.
  void SetSessionDecideWhatToWrite(bool session_decides_what_to_write);

  void SetSessionNotifier(SessionNotifierInterface* session_notifier);

  bool session_decides_what_to_write() const {
    return session_decides_what_to_write_;
  }

  bool fix_is_useful_for_retransmission() const {
    return fix_is_useful_for_retransmission_;
  }

 private:
  friend class test::QuicUnackedPacketMapPeer;

  // Called when a packet is retransmitted with a new packet number.
  // |old_packet_number| will remain unacked, but will have no
  // retransmittable data associated with it. Retransmittable frames will be
  // transferred to |info| and all_transmissions will be populated.
  void TransferRetransmissionInfo(QuicPacketNumber old_packet_number,
                                  QuicPacketNumber new_packet_number,
                                  TransmissionType transmission_type,
                                  QuicTransmissionInfo* info);

  // Returns true if packet may be useful for an RTT measurement.
  bool IsPacketUsefulForMeasuringRtt(QuicPacketNumber packet_number,
                                     const QuicTransmissionInfo& info) const;

  // Returns true if packet may be useful for congestion control purposes.
  bool IsPacketUsefulForCongestionControl(
      const QuicTransmissionInfo& info) const;

  // Returns true if packet may be associated with retransmittable data
  // directly or through retransmissions.
  bool IsPacketUsefulForRetransmittableData(
      const QuicTransmissionInfo& info) const;

  // Returns true if the packet no longer has a purpose in the map.
  bool IsPacketUseless(QuicPacketNumber packet_number,
                       const QuicTransmissionInfo& info) const;

  QuicPacketNumber largest_sent_packet_;
  // The largest sent packet we expect to receive an ack for.
  QuicPacketNumber largest_sent_retransmittable_packet_;
  // The largest sent largest_acked in an ACK frame.
  QuicPacketNumber largest_sent_largest_acked_;
  // The largest received largest_acked from an ACK frame.
  QuicPacketNumber largest_acked_;

  // Newly serialized retransmittable packets are added to this map, which
  // contains owning pointers to any contained frames.  If a packet is
  // retransmitted, this map will contain entries for both the old and the new
  // packet. The old packet's retransmittable frames entry will be nullptr,
  // while the new packet's entry will contain the frames to retransmit.
  // If the old packet is acked before the new packet, then the old entry will
  // be removed from the map and the new entry's retransmittable frames will be
  // set to nullptr.
  UnackedPacketMap unacked_packets_;
  // The packet at the 0th index of unacked_packets_.
  QuicPacketNumber least_unacked_;

  QuicByteCount bytes_in_flight_;
  // Number of retransmittable crypto handshake packets.
  size_t pending_crypto_packet_count_;

  // Time that the last unacked crypto packet was sent.
  QuicTime last_crypto_packet_sent_time_;

  // Aggregates acked stream data across multiple acked sent packets to save CPU
  // by reducing the number of calls to the session notifier.
  QuicStreamFrame aggregated_stream_frame_;

  // Receives notifications of frames being retransmitted or acknowledged.
  SessionNotifierInterface* session_notifier_;

  // If true, let session decides what to write.
  bool session_decides_what_to_write_;

  // Latched value of quic_reloadable_flag_quic_fix_is_useful_for_retrans.
  const bool fix_is_useful_for_retransmission_;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_QUIC_UNACKED_PACKET_MAP_H_
