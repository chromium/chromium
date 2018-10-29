// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/logging/encoding_event_subscriber.h"

#include <stdint.h>

#include <algorithm>
#include <cstring>
#include <utility>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "media/cast/logging/proto/proto_utils.h"

using google::protobuf::RepeatedPtrField;
using media::cast::proto::AggregatedFrameEvent;
using media::cast::proto::AggregatedPacketEvent;
using media::cast::proto::BasePacketEvent;
using media::cast::proto::LogMetadata;

namespace {

// A size limit on maps to keep lookups fast.
const size_t kMaxMapSize = 200;

// The smallest (oredered by RTP timestamp) |kNumMapEntriesToTransfer| entries
// will be moved when the map size reaches |kMaxMapSize|.
// Must be smaller than |kMaxMapSize|.
const size_t kNumMapEntriesToTransfer = 100;

template <typename ProtoPtr>
bool IsRtpTimestampLessThan(const ProtoPtr& lhs, const ProtoPtr& rhs) {
  return lhs->relative_rtp_timestamp() < rhs->relative_rtp_timestamp();
}

BasePacketEvent* GetNewBasePacketEvent(AggregatedPacketEvent* event_proto,
    int packet_id, int size) {
  BasePacketEvent* base = event_proto->add_base_packet_event();
  base->set_packet_id(packet_id);
  base->set_size(size);
  return base;
}

}  // namespace

namespace media {
namespace cast {

EncodingEventSubscriber::EncodingEventSubscriber(
    EventMediaType event_media_type,
    size_t max_frames)
    : event_media_type_(event_media_type), max_frames_(max_frames) {
  Reset();
}

EncodingEventSubscriber::~EncodingEventSubscriber() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void EncodingEventSubscriber::OnReceiveFrameEvent(
    const FrameEvent& frame_event) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (event_media_type_ != frame_event.media_type)
    return;

  const RtpTimeDelta relative_rtp_timestamp =
      GetRelativeRtpTimestamp(frame_event.rtp_timestamp);
  const uint32_t lower_32_bits = relative_rtp_timestamp.lower_32_bits();
  AggregatedFrameEvent* event_proto_ptr = nullptr;

  // Look up existing entry. If not found, create a new entry and add to map.
  auto it = frame_event_map_.find(relative_rtp_timestamp);
  if (it == frame_event_map_.end()) {
    if (!ShouldCreateNewProto(lower_32_bits))
      return;

    IncrementStoredProtoCount(lower_32_bits);
    auto event_proto = std::make_unique<AggregatedFrameEvent>();
    event_proto->set_relative_rtp_timestamp(lower_32_bits);
    event_proto_ptr = event_proto.get();
    frame_event_map_.insert(
        std::make_pair(relative_rtp_timestamp, std::move(event_proto)));
  } else {
    if (it->second->event_type_size() >= kMaxEventsPerProto) {
      DVLOG(2) << "Too many events in frame " << frame_event.rtp_timestamp
               << ". Using new frame event proto.";
      AddFrameEventToStorage(std::move(it->second));
      if (!ShouldCreateNewProto(lower_32_bits)) {
        frame_event_map_.erase(it);
        return;
      }

      IncrementStoredProtoCount(lower_32_bits);
      it->second = std::make_unique<AggregatedFrameEvent>();
      it->second->set_relative_rtp_timestamp(lower_32_bits);
    }
    event_proto_ptr = it->second.get();
  }

  event_proto_ptr->add_event_type(ToProtoEventType(frame_event.type));
  event_proto_ptr->add_event_timestamp_ms(
      (frame_event.timestamp - base::TimeTicks()).InMilliseconds());

  if (frame_event.type == FRAME_CAPTURE_END) {
    if (frame_event.media_type == VIDEO_EVENT &&
        frame_event.width > 0 && frame_event.height > 0) {
      event_proto_ptr->set_width(frame_event.width);
      event_proto_ptr->set_height(frame_event.height);
    }
  } else if (frame_event.type == FRAME_ENCODED) {
    event_proto_ptr->set_encoded_frame_size(frame_event.size);
    if (frame_event.encoder_cpu_utilization >= 0.0) {
      event_proto_ptr->set_encoder_cpu_percent_utilized(
          base::saturated_cast<int32_t>(
              frame_event.encoder_cpu_utilization * 100.0 + 0.5));
    }
    if (frame_event.idealized_bitrate_utilization >= 0.0) {
      event_proto_ptr->set_idealized_bitrate_percent_utilized(
          base::saturated_cast<int32_t>(
              frame_event.idealized_bitrate_utilization * 100.0 + 0.5));
    }
    if (frame_event.media_type == VIDEO_EVENT) {
      event_proto_ptr->set_key_frame(frame_event.key_frame);
      event_proto_ptr->set_target_bitrate(frame_event.target_bitrate);
    }
  } else if (frame_event.type == FRAME_PLAYOUT) {
    event_proto_ptr->set_delay_millis(frame_event.delay_delta.InMilliseconds());
  }

  if (frame_event_map_.size() > kMaxMapSize)
    TransferFrameEvents(kNumMapEntriesToTransfer);

  DCHECK(frame_event_map_.size() <= kMaxMapSize);
  DCHECK(frame_event_storage_.size() <= max_frames_);
}

void EncodingEventSubscriber::OnReceivePacketEvent(
    const PacketEvent& packet_event) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (event_media_type_ != packet_event.media_type)
    return;

  const RtpTimeDelta relative_rtp_timestamp =
      GetRelativeRtpTimestamp(packet_event.rtp_timestamp);
  uint32_t lower_32_bits = relative_rtp_timestamp.lower_32_bits();
  auto it = packet_event_map_.find(relative_rtp_timestamp);
  BasePacketEvent* base_packet_event_proto = nullptr;

  // Look up existing entry. If not found, create a new entry and add to map.
  if (it == packet_event_map_.end()) {
    if (!ShouldCreateNewProto(lower_32_bits))
      return;

    IncrementStoredProtoCount(lower_32_bits);
    auto event_proto = std::make_unique<AggregatedPacketEvent>();
    event_proto->set_relative_rtp_timestamp(lower_32_bits);
    base_packet_event_proto = GetNewBasePacketEvent(
        event_proto.get(), packet_event.packet_id, packet_event.size);
    packet_event_map_.insert(
        std::make_pair(relative_rtp_timestamp, std::move(event_proto)));
  } else {
    // Found existing entry, now look up existing BasePacketEvent using packet
    // ID. If not found, create a new entry and add to proto.
    RepeatedPtrField<BasePacketEvent>* field =
        it->second->mutable_base_packet_event();
    for (RepeatedPtrField<BasePacketEvent>::pointer_iterator base_it =
             field->pointer_begin();
         base_it != field->pointer_end();
         ++base_it) {
      if ((*base_it)->packet_id() == packet_event.packet_id) {
        base_packet_event_proto = *base_it;
        break;
      }
    }
    if (!base_packet_event_proto) {
      if (it->second->base_packet_event_size() >= kMaxPacketsPerFrame) {
        DVLOG(3) << "Too many packets in AggregatedPacketEvent "
                 << packet_event.rtp_timestamp << ". "
                 << "Using new packet event proto.";
        AddPacketEventToStorage(std::move(it->second));
        if (!ShouldCreateNewProto(lower_32_bits)) {
          packet_event_map_.erase(it);
          return;
        }

        IncrementStoredProtoCount(lower_32_bits);
        it->second = std::make_unique<AggregatedPacketEvent>();
        it->second->set_relative_rtp_timestamp(lower_32_bits);
      }

      base_packet_event_proto = GetNewBasePacketEvent(
          it->second.get(), packet_event.packet_id, packet_event.size);
    } else if (base_packet_event_proto->event_type_size() >=
               kMaxEventsPerProto) {
      DVLOG(3) << "Too many events in packet "
               << packet_event.rtp_timestamp << ", "
               << packet_event.packet_id << ". Using new packet event proto.";
      AddPacketEventToStorage(std::move(it->second));
      if (!ShouldCreateNewProto(lower_32_bits)) {
        packet_event_map_.erase(it);
        return;
      }

      IncrementStoredProtoCount(lower_32_bits);
      it->second = std::make_unique<AggregatedPacketEvent>();
      it->second->set_relative_rtp_timestamp(lower_32_bits);
      base_packet_event_proto = GetNewBasePacketEvent(
          it->second.get(), packet_event.packet_id, packet_event.size);
    }
  }

  base_packet_event_proto->add_event_type(
      ToProtoEventType(packet_event.type));
  base_packet_event_proto->add_event_timestamp_ms(
      (packet_event.timestamp - base::TimeTicks()).InMilliseconds());

  // |base_packet_event_proto| could have been created with a receiver event
  // which does not have the packet size and we would need to overwrite it when
  // we see a sender event, which does have the packet size.
  if (packet_event.size > 0) {
    base_packet_event_proto->set_size(packet_event.size);
  }

  if (packet_event_map_.size() > kMaxMapSize)
    TransferPacketEvents(kNumMapEntriesToTransfer);

  DCHECK(packet_event_map_.size() <= kMaxMapSize);
  DCHECK(packet_event_storage_.size() <= max_frames_);
}

void EncodingEventSubscriber::GetEventsAndReset(LogMetadata* metadata,
    FrameEventList* frame_events, PacketEventList* packet_events) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Flush all events.
  TransferFrameEvents(frame_event_map_.size());
  TransferPacketEvents(packet_event_map_.size());
  std::sort(frame_event_storage_.begin(), frame_event_storage_.end(),
            &IsRtpTimestampLessThan<std::unique_ptr<AggregatedFrameEvent>>);
  std::sort(packet_event_storage_.begin(), packet_event_storage_.end(),
            &IsRtpTimestampLessThan<std::unique_ptr<AggregatedPacketEvent>>);

  metadata->set_is_audio(event_media_type_ == AUDIO_EVENT);
  metadata->set_first_rtp_timestamp(first_rtp_timestamp_.lower_32_bits());
  metadata->set_num_frame_events(frame_event_storage_.size());
  metadata->set_num_packet_events(packet_event_storage_.size());
  metadata->set_reference_timestamp_ms_at_unix_epoch(
      (base::TimeTicks::UnixEpoch() - base::TimeTicks()).InMilliseconds());
  frame_events->swap(frame_event_storage_);
  packet_events->swap(packet_event_storage_);
  Reset();
}

void EncodingEventSubscriber::TransferFrameEvents(size_t max_num_entries) {
  DCHECK(frame_event_map_.size() >= max_num_entries);

  auto it = frame_event_map_.begin();
  for (size_t i = 0;
       i < max_num_entries && it != frame_event_map_.end();
       i++, ++it) {
    AddFrameEventToStorage(std::move(it->second));
  }

  frame_event_map_.erase(frame_event_map_.begin(), it);
}

void EncodingEventSubscriber::TransferPacketEvents(size_t max_num_entries) {
  auto it = packet_event_map_.begin();
  for (size_t i = 0;
       i < max_num_entries && it != packet_event_map_.end();
       i++, ++it) {
    AddPacketEventToStorage(std::move(it->second));
  }

  packet_event_map_.erase(packet_event_map_.begin(), it);
}

void EncodingEventSubscriber::AddFrameEventToStorage(
    std::unique_ptr<AggregatedFrameEvent> frame_event_proto) {
  if (frame_event_storage_.size() >= max_frames_) {
    auto& entry = frame_event_storage_[frame_event_storage_index_];
    DecrementStoredProtoCount(entry->relative_rtp_timestamp());
    entry = std::move(frame_event_proto);
  } else {
    frame_event_storage_.push_back(std::move(frame_event_proto));
  }

  frame_event_storage_index_ = (frame_event_storage_index_ + 1) % max_frames_;
}

void EncodingEventSubscriber::AddPacketEventToStorage(
    std::unique_ptr<AggregatedPacketEvent> packet_event_proto) {
  if (packet_event_storage_.size() >= max_frames_) {
    auto& entry = packet_event_storage_[packet_event_storage_index_];
    DecrementStoredProtoCount(entry->relative_rtp_timestamp());
    entry = std::move(packet_event_proto);
  } else {
    packet_event_storage_.push_back(std::move(packet_event_proto));
  }

  packet_event_storage_index_ = (packet_event_storage_index_ + 1) % max_frames_;
}

bool EncodingEventSubscriber::ShouldCreateNewProto(
    uint32_t relative_rtp_timestamp_lower_32_bits) const {
  auto it = stored_proto_counts_.find(relative_rtp_timestamp_lower_32_bits);
  int proto_count = it == stored_proto_counts_.end() ? 0 : it->second;
  DVLOG_IF(2, proto_count >= kMaxProtosPerFrame)
      << relative_rtp_timestamp_lower_32_bits
      << " already reached max number of protos.";
  return proto_count < kMaxProtosPerFrame;
}

void EncodingEventSubscriber::IncrementStoredProtoCount(
    uint32_t relative_rtp_timestamp_lower_32_bits) {
  stored_proto_counts_[relative_rtp_timestamp_lower_32_bits]++;
  DCHECK_LE(stored_proto_counts_[relative_rtp_timestamp_lower_32_bits],
            kMaxProtosPerFrame)
      << relative_rtp_timestamp_lower_32_bits
      << " exceeded max number of event protos.";
}

void EncodingEventSubscriber::DecrementStoredProtoCount(
    uint32_t relative_rtp_timestamp_lower_32_bits) {
  auto it = stored_proto_counts_.find(relative_rtp_timestamp_lower_32_bits);
  DCHECK(it != stored_proto_counts_.end())
      << "no event protos for " << relative_rtp_timestamp_lower_32_bits;
  if (it->second > 1)
    it->second--;
  else
    stored_proto_counts_.erase(it);
}

RtpTimeDelta EncodingEventSubscriber::GetRelativeRtpTimestamp(
    RtpTimeTicks rtp_timestamp) {
  if (!seen_first_rtp_timestamp_) {
    seen_first_rtp_timestamp_ = true;
    first_rtp_timestamp_ = rtp_timestamp;
  }

  return rtp_timestamp - first_rtp_timestamp_;
}

void EncodingEventSubscriber::Reset() {
  frame_event_map_.clear();
  frame_event_storage_.clear();
  frame_event_storage_index_ = 0;
  packet_event_map_.clear();
  packet_event_storage_.clear();
  packet_event_storage_index_ = 0;
  stored_proto_counts_.clear();
  seen_first_rtp_timestamp_ = false;
  first_rtp_timestamp_ = RtpTimeTicks();
}

}  // namespace cast
}  // namespace media
