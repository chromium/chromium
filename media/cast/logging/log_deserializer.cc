// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/logging/log_deserializer.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/big_endian.h"
#include "third_party/zlib/zlib.h"

using media::cast::FrameEventMap;
using media::cast::PacketEventMap;
using media::cast::RtpTimeDelta;
using media::cast::RtpTimeTicks;
using media::cast::proto::AggregatedFrameEvent;
using media::cast::proto::AggregatedPacketEvent;
using media::cast::proto::BasePacketEvent;
using media::cast::proto::LogMetadata;

namespace {

// Use 60MB of temp buffer to hold uncompressed data if |compress| is true.
// This is double the size of temp buffer used during compression (30MB)
// since the there are two streams in the blob.
// Keep in sync with media/cast/logging/log_serializer.cc.
const int kMaxUncompressedBytes = 60 * 1000 * 1000;

void MergePacketEvent(const AggregatedPacketEvent& from,
    linked_ptr<AggregatedPacketEvent> to) {
  for (int i = 0; i < from.base_packet_event_size(); i++) {
    const BasePacketEvent& from_base_event = from.base_packet_event(i);
    bool merged = false;
    for (int j = 0; j < to->base_packet_event_size(); j++) {
      BasePacketEvent* to_base_event = to->mutable_base_packet_event(j);
      if (from_base_event.packet_id() == to_base_event->packet_id()) {
        int packet_size = std::max(
            from_base_event.size(), to_base_event->size());
        // Need special merge logic here because we need to prevent a valid
        // packet size (> 0) from being overwritten with an invalid one (= 0).
        to_base_event->MergeFrom(from_base_event);
        to_base_event->set_size(packet_size);
        merged = true;
        break;
      }
    }
    if (!merged) {
      BasePacketEvent* to_base_event = to->add_base_packet_event();
      to_base_event->CopyFrom(from_base_event);
    }
  }
}

void MergeFrameEvent(const AggregatedFrameEvent& from,
    linked_ptr<AggregatedFrameEvent> to) {
  to->mutable_event_type()->MergeFrom(from.event_type());
  to->mutable_event_timestamp_ms()->MergeFrom(from.event_timestamp_ms());
  if (!to->has_encoded_frame_size() && from.has_encoded_frame_size())
    to->set_encoded_frame_size(from.encoded_frame_size());
  if (!to->has_delay_millis() && from.has_delay_millis())
    to->set_delay_millis(from.delay_millis());
  if (!to->has_key_frame() && from.has_key_frame())
    to->set_key_frame(from.key_frame());
  if (!to->has_target_bitrate() && from.has_target_bitrate())
    to->set_target_bitrate(from.target_bitrate());
}

bool PopulateDeserializedLog(base::BigEndianReader* reader,
                             media::cast::DeserializedLog* log) {
  FrameEventMap frame_event_map;
  PacketEventMap packet_event_map;

  int num_frame_events = log->metadata.num_frame_events();
  RtpTimeTicks relative_rtp_timestamp;
  uint16_t proto_size = 0;
  for (int i = 0; i < num_frame_events; i++) {
    if (!reader->ReadU16(&proto_size))
      return false;

    linked_ptr<AggregatedFrameEvent> frame_event(new AggregatedFrameEvent);
    if (!frame_event->ParseFromArray(reader->ptr(), proto_size))
      return false;
    if (!reader->Skip(proto_size))
      return false;

    // During serialization the RTP timestamp in proto is relative to previous
    // frame.
    // Adjust RTP timestamp back to value relative to first RTP timestamp.
    relative_rtp_timestamp +=
        RtpTimeDelta::FromTicks(frame_event->relative_rtp_timestamp());
    frame_event->set_relative_rtp_timestamp(
        relative_rtp_timestamp.lower_32_bits());

    auto it = frame_event_map.find(relative_rtp_timestamp);
    if (it == frame_event_map.end()) {
      frame_event_map.insert(
          std::make_pair(relative_rtp_timestamp, frame_event));
    } else {
      // Events for the same frame might have been split into more than one
      // proto. Merge them.
      MergeFrameEvent(*frame_event, it->second);
    }
  }

  log->frame_events.swap(frame_event_map);

  int num_packet_events = log->metadata.num_packet_events();
  relative_rtp_timestamp = RtpTimeTicks();
  for (int i = 0; i < num_packet_events; i++) {
    if (!reader->ReadU16(&proto_size))
      return false;

    linked_ptr<AggregatedPacketEvent> packet_event(new AggregatedPacketEvent);
    if (!packet_event->ParseFromArray(reader->ptr(), proto_size))
      return false;
    if (!reader->Skip(proto_size))
      return false;

    relative_rtp_timestamp +=
        RtpTimeDelta::FromTicks(packet_event->relative_rtp_timestamp());
    packet_event->set_relative_rtp_timestamp(
        relative_rtp_timestamp.lower_32_bits());

    auto it = packet_event_map.find(relative_rtp_timestamp);
    if (it == packet_event_map.end()) {
      packet_event_map.insert(
          std::make_pair(relative_rtp_timestamp, packet_event));
    } else {
      // Events for the same frame might have been split into more than one
      // proto. Merge them.
      MergePacketEvent(*packet_event, it->second);
    }
  }

  log->packet_events.swap(packet_event_map);

  return true;
}

bool DoDeserializeEvents(const char* data,
                         int data_bytes,
                         media::cast::DeserializedLog* audio_log,
                         media::cast::DeserializedLog* video_log) {
  bool got_audio = false;
  bool got_video = false;
  base::BigEndianReader reader(data, data_bytes);

  LogMetadata metadata;
  uint16_t proto_size = 0;
  while (reader.remaining() > 0) {
    if (!reader.ReadU16(&proto_size))
      return false;
    if (!metadata.ParseFromArray(reader.ptr(), proto_size))
      return false;
    reader.Skip(proto_size);

    if (metadata.is_audio()) {
      if (got_audio) {
        VLOG(1) << "Got audio data twice.";
        return false;
      }

      got_audio = true;
      audio_log->metadata = metadata;
      if (!PopulateDeserializedLog(&reader, audio_log))
        return false;
    } else {
      if (got_video) {
        VLOG(1) << "Got duplicate video log.";
        return false;
      }

      got_video = true;
      video_log->metadata = metadata;
      if (!PopulateDeserializedLog(&reader, video_log))
        return false;
    }
  }
  return true;
}

bool Uncompress(const char* data,
                int data_bytes,
                int max_uncompressed_bytes,
                char* uncompressed,
                int* uncompressed_bytes) {
  z_stream stream = {0};

  stream.next_in = reinterpret_cast<uint8_t*>(const_cast<char*>(data));
  stream.avail_in = data_bytes;
  stream.next_out = reinterpret_cast<uint8_t*>(uncompressed);
  stream.avail_out = max_uncompressed_bytes;

  bool success = false;
  while (stream.avail_in > 0 && stream.avail_out > 0) {
    // 16 is added to read in gzip format.
    int result = inflateInit2(&stream, MAX_WBITS + 16);
    DCHECK_EQ(Z_OK, result);

    result = inflate(&stream, Z_FINISH);
    success = (result == Z_STREAM_END);
    if (!success) {
      DVLOG(2) << "inflate() failed. Result: " << result;
      break;
    }

    result = inflateEnd(&stream);
    DCHECK(result == Z_OK);
  }

  if (stream.avail_in == 0) {
    success = true;
    *uncompressed_bytes = max_uncompressed_bytes - stream.avail_out;
  }
  return success;
}

}  // namespace

namespace media {
namespace cast {

bool DeserializeEvents(const char* data,
                       int data_bytes,
                       bool compressed,
                       DeserializedLog* audio_log,
                       DeserializedLog* video_log) {
  DCHECK_GT(data_bytes, 0);

  if (compressed) {
    std::unique_ptr<char[]> uncompressed(new char[kMaxUncompressedBytes]);
    int uncompressed_bytes = 0;
    if (!Uncompress(data,
                    data_bytes,
                    kMaxUncompressedBytes,
                    uncompressed.get(),
                    &uncompressed_bytes))
      return false;

    return DoDeserializeEvents(
        uncompressed.get(), uncompressed_bytes, audio_log, video_log);
  } else {
    return DoDeserializeEvents(data, data_bytes, audio_log, video_log);
  }
}

DeserializedLog::DeserializedLog() = default;
DeserializedLog::~DeserializedLog() = default;

}  // namespace cast
}  // namespace media
