// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_LOGGING_ENCODING_EVENT_SUBSCRIBER_H_
#define MEDIA_CAST_LOGGING_ENCODING_EVENT_SUBSCRIBER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <vector>

#include "base/threading/thread_checker.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/proto/raw_events.pb.h"
#include "media/cast/logging/raw_event_subscriber.h"

namespace media {
namespace cast {

// Number of packets per frame recorded by the subscriber.
// Once the max number of packets has been reached, a new aggregated proto
// will be created.
static const int kMaxPacketsPerFrame = 256;
// Number of events per frame/packet proto recorded by the subscriber.
// Once the max number of events has been reached, a new aggregated proto
// will be created.
static const int kMaxEventsPerProto = 16;
// Max number of AggregatedFrameEvent / AggregatedPacketEvent protos stored for
// a frame. Once the max number of protos has been reached for that frame,
// further events for that frame will be dropped.
static const int kMaxProtosPerFrame = 10;

using FrameEventList =
    std::vector<std::unique_ptr<proto::AggregatedFrameEvent>>;
using PacketEventList =
    std::vector<std::unique_ptr<proto::AggregatedPacketEvent>>;

// A RawEventSubscriber implementation that subscribes to events,
// encodes them in protocol buffer format, and aggregates them into a more
// compact structure. Aggregation is per-frame, and uses a map with RTP
// timestamp as key. Periodically, old entries in the map will be transferred
// to a storage vector. This helps keep the size of the map small and
// lookup times fast. The storage itself is a circular buffer that will
// overwrite old entries once it has reached the size configured by user.
class EncodingEventSubscriber final : public RawEventSubscriber {
 public:
  // |event_media_type|: The subscriber will only process events that
  // corresponds to this type.
  // |max_frames|: How many events to keep in the frame / packet storage.
  // This helps keep memory usage bounded.
  // Every time one of |OnReceive[Frame,Packet]Event()| is
  // called, it will check if the respective map size has exceeded |max_frames|.
  // If so, it will remove the oldest aggregated entry (ordered by RTP
  // timestamp).
  EncodingEventSubscriber(EventMediaType event_media_type, size_t max_frames);

  EncodingEventSubscriber(const EncodingEventSubscriber&) = delete;
  EncodingEventSubscriber& operator=(const EncodingEventSubscriber&) = delete;

  ~EncodingEventSubscriber() final;

  // RawReventSubscriber implementations.
  void OnReceiveFrameEvent(const FrameEvent& frame_event) final;
  void OnReceivePacketEvent(const PacketEvent& packet_event) final;

  // Assigns frame events and packet events received so far to |frame_events|
  // and |packet_events| and resets the internal state.
  // In addition, assign metadata associated with these events to |metadata|.
  // The protos in |frame_events| and |packets_events| are sorted in
  // ascending RTP timestamp order.
  void GetEventsAndReset(proto::LogMetadata* metadata,
                         FrameEventList* frame_events,
                         PacketEventList* packet_events);

 private:
  using FrameEventMap =
      std::map<RtpTimeDelta, std::unique_ptr<proto::AggregatedFrameEvent>>;
  using PacketEventMap =
      std::map<RtpTimeDelta, std::unique_ptr<proto::AggregatedPacketEvent>>;

  // Transfer up to |max_num_entries| smallest entries from |frame_event_map_|
  // to |frame_event_storage_|. This helps keep size of |frame_event_map_| small
  // and lookup speed fast.
  void TransferFrameEvents(size_t max_num_entries);
  // See above.
  void TransferPacketEvents(size_t max_num_entries);

  void AddFrameEventToStorage(
      std::unique_ptr<proto::AggregatedFrameEvent> frame_event_proto);
  void AddPacketEventToStorage(
      std::unique_ptr<proto::AggregatedPacketEvent> packet_event_proto);

  bool ShouldCreateNewProto(
      uint32_t relative_rtp_timestamp_lower_32_bits) const;
  void IncrementStoredProtoCount(uint32_t relative_rtp_timestamp_lower_32_bits);
  void DecrementStoredProtoCount(uint32_t relative_rtp_timestamp_lower_32_bits);

  // Returns the difference between |rtp_timestamp| and |first_rtp_timestamp_|.
  // Sets |first_rtp_timestamp_| if it is not already set.
  RtpTimeDelta GetRelativeRtpTimestamp(RtpTimeTicks rtp_timestamp);

  // Clears the maps and first RTP timestamp seen.
  void Reset();

  const EventMediaType event_media_type_;
  const size_t max_frames_;

  FrameEventMap frame_event_map_;
  FrameEventList frame_event_storage_;
  int frame_event_storage_index_;

  PacketEventMap packet_event_map_;
  PacketEventList packet_event_storage_;
  int packet_event_storage_index_;

  // Maps from the lower 32 bits of a RTP timestamp to the number of
  // AggregatedFrameEvent / AggregatedPacketEvent protos that have been stored
  // for that frame.
  std::map<uint32_t, int> stored_proto_counts_;

  // All functions must be called on the main thread.
  base::ThreadChecker thread_checker_;

  // Set to true on first event encountered after a |Reset()|.
  bool seen_first_rtp_timestamp_;

  // Set to RTP timestamp of first event encountered after a |Reset()|.
  RtpTimeTicks first_rtp_timestamp_;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_LOGGING_ENCODING_EVENT_SUBSCRIBER_H_
