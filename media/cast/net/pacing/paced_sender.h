// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_NET_PACING_PACED_SENDER_H_
#define MEDIA_CAST_NET_PACING_PACED_SENDER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <tuple>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/net/cast_transport_config.h"

namespace media {
namespace cast {

// Meant to use as defaults for pacer construction.
static const size_t kTargetBurstSize = 10;
static const size_t kMaxBurstSize = 20;

// The PacketKey is designed to meet two criteria:
// 1. When we re-send the same packet again, we can use the packet key
//    to identify it so that we can de-duplicate packets in the queue.
// 2. The sort order of the PacketKey determines the order that packets
//    are sent out.
// 3. The PacketKey is unique for each RTP (frame) packet.
struct PacketKey {
  base::TimeTicks capture_time;
  uint32_t ssrc;
  FrameId frame_id;
  uint16_t packet_id;

  PacketKey();  // Do not use.  This is for STL containers.
  PacketKey(base::TimeTicks capture_time,
            uint32_t ssrc,
            FrameId frame_id,
            uint16_t packet_id);
  PacketKey(const PacketKey& other);

  ~PacketKey();

  bool operator==(const PacketKey& key) const {
    return std::tie(capture_time, ssrc, frame_id, packet_id) ==
           std::tie(key.capture_time, key.ssrc, key.frame_id, key.packet_id);
  }

  bool operator<(const PacketKey& key) const {
    return std::tie(capture_time, ssrc, frame_id, packet_id) <
           std::tie(key.capture_time, key.ssrc, key.frame_id, key.packet_id);
  }
};

typedef std::vector<std::pair<PacketKey, PacketRef> > SendPacketVector;

// Information used to deduplicate retransmission packets.
// There are two criteria for deduplication.
//
// 1. Using another muxed stream.
//    Suppose there are multiple streams muxed and sent via the same
//    socket. When there is a retransmission request for packet X, we
//    will reject the retransmission if there is a packet sent from
//    another stream just before X but not acked. Typically audio stream
//    is used for this purpose. |last_byte_acked_for_audio| provides this
//    information.
//
// 2. Using a time interval.
//    Time between sending the same packet must be greater than
//    |resend_interval|.
struct DedupInfo {
  DedupInfo();
  base::TimeDelta resend_interval;
  int64_t last_byte_acked_for_audio;
};

// We have this pure virtual class to enable mocking.
class PacedPacketSender {
 public:
  virtual bool SendPackets(const SendPacketVector& packets) = 0;
  virtual bool ResendPackets(const SendPacketVector& packets,
                             const DedupInfo& dedup_info) = 0;
  virtual bool SendRtcpPacket(uint32_t ssrc, PacketRef packet) = 0;
  virtual void CancelSendingPacket(const PacketKey& packet_key) = 0;

  virtual ~PacedPacketSender() {}
};

class PacedSender : public PacedPacketSender {
 public:
  // |recent_packet_events| is an externally-owned vector where PacedSender will
  // add PacketEvents related to sending, retransmission, and rejection.  The
  // |external_transport| should only be used by the Cast receiver and for
  // testing.
  PacedSender(
      size_t target_burst_size,  // Should normally be kTargetBurstSize.
      size_t max_burst_size,     // Should normally be kMaxBurstSize.
      const base::TickClock* clock,
      std::vector<PacketEvent>* recent_packet_events,
      PacketTransport* external_transport,
      const scoped_refptr<base::SingleThreadTaskRunner>& transport_task_runner);

  ~PacedSender() final;

  // These must be called before non-RTCP packets are sent.
  void RegisterSsrc(uint32_t ssrc, bool is_audio);

  // Register SSRC that has a higher priority for sending. Multiple SSRCs can
  // be registered.
  // Note that it is not expected to register many SSRCs with this method.
  // Because IsHigherPriority() is determined in linear time.
  void RegisterPrioritySsrc(uint32_t ssrc);

  // Returns the total number of bytes sent to the socket when the specified
  // packet was just sent.
  // Returns 0 if the packet cannot be found or not yet sent.
  // This function is currently only used by unittests.
  int64_t GetLastByteSentForPacket(const PacketKey& packet_key);

  // Returns the total number of bytes sent to the socket when the last payload
  // identified by SSRC is just sent. Returns 0 for an unknown ssrc.
  // This function is currently only used by unittests.
  int64_t GetLastByteSentForSsrc(uint32_t ssrc);

  // PacedPacketSender implementation.
  bool SendPackets(const SendPacketVector& packets) final;
  bool ResendPackets(const SendPacketVector& packets,
                     const DedupInfo& dedup_info) final;
  bool SendRtcpPacket(uint32_t ssrc, PacketRef packet) final;
  void CancelSendingPacket(const PacketKey& packet_key) final;

  void SetTargetBurstSize(int burst_size) {
    target_burst_size_ = current_max_burst_size_ = next_max_burst_size_ =
        next_next_max_burst_size_ = burst_size;
  }

  void SetMaxBurstSize(int burst_size) { max_burst_size_ = burst_size; }

 private:
  // Actually sends the packets to the transport.
  void SendStoredPackets();

  // Convenience method for building a PacketEvent and storing it in the
  // externally-owned container of |recent_packet_events_|.
  void LogPacketEvent(const Packet& packet, CastLoggingEvent event);

  // Returns true if retransmission for packet indexed by |packet_key| is
  // accepted. |dedup_info| contains information to help deduplicate
  // retransmission. |now| is the current time to save on fetching it from the
  // clock multiple times.
  bool ShouldResend(const PacketKey& packet_key,
                    const DedupInfo& dedup_info,
                    const base::TimeTicks& now);

  enum PacketType {
    PacketType_RTCP,
    PacketType_Resend,
    PacketType_Normal
  };
  enum State {
    // In an unblocked state, we can send more packets.
    // We have to check the current time against |burst_end_| to see if we are
    // appending to the current burst or if we can start a new one.
    State_Unblocked,
    // In this state, we are waiting for a callback from the udp transport.
    // This happens when the OS-level buffer is full. Once we receive the
    // callback, we go to State_Unblocked and see if we can write more packets
    // to the current burst. (Or the next burst if enough time has passed.)
    State_TransportBlocked,
    // Once we've written enough packets for a time slice, we go into this
    // state and PostDelayTask a call to ourselves to wake up when we can
    // send more data.
    State_BurstFull
  };

  bool empty() const;
  size_t size() const;

  // Returns the next packet to send. RTCP packets have highest priority, then
  // high-priority RTP packets, then normal-priority RTP packets.  Packets
  // within a frame are selected based on fairness to ensure all have an equal
  // chance of being sent.  Therefore, it is up to client code to ensure that
  // packets acknowledged in NACK messages are removed from PacedSender (see
  // CancelSendingPacket()), to avoid wasteful retransmission.
  PacketRef PopNextPacket(PacketType* packet_type,
                          PacketKey* packet_key);

  // Returns true if the packet should have a higher priority.
  bool IsHighPriority(const PacketKey& packet_key) const;

  // These are externally-owned objects injected via the constructor.
  const base::TickClock* const clock_;
  std::vector<PacketEvent>* const recent_packet_events_;
  PacketTransport* const transport_;

  scoped_refptr<base::SingleThreadTaskRunner> transport_task_runner_;

  // Set of SSRCs that have higher priority. This is a vector instead of a
  // set because there's only very few in it (most likely 1).
  std::vector<uint32_t> priority_ssrcs_;
  typedef std::map<PacketKey, std::pair<PacketType, PacketRef> > PacketList;
  PacketList packet_list_;
  PacketList priority_packet_list_;

  struct PacketSendRecord;
  using PacketSendHistory = std::map<PacketKey, PacketSendRecord>;
  PacketSendHistory send_history_;
  PacketSendHistory send_history_buffer_;

  struct RtpSession;
  using SessionMap = std::map<uint32_t, RtpSession>;
  // Records all the cast sessions with the sender SSRC as the key. These
  // sessions are in sync with those in CastTransportImpl.
  SessionMap sessions_;

  // Records the last byte sent for audio payload.
  int64_t last_byte_sent_for_audio_;

  size_t target_burst_size_;
  size_t max_burst_size_;

  // Maximum burst size for the next three bursts.
  size_t current_max_burst_size_;
  size_t next_max_burst_size_;
  size_t next_next_max_burst_size_;
  // Number of packets already sent in the current burst.
  size_t current_burst_size_;
  // This is when the current burst ends.
  base::TimeTicks burst_end_;

  State state_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<PacedSender> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PacedSender);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_NET_PACING_PACED_SENDER_H_
