// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/hdr_metadata_track.h"

#include <iterator>
#include <vector>

#include "media/base/decoder_buffer_side_data.h"
#include "media/base/stream_parser_buffer.h"

namespace media::mp4 {

HdrMetadataTrack::HdrMetadataTrack(
    StreamParser::TrackId metadata_track_id,
    MetadataIT35SampleEntry::IT35PrefixType prefix_type,
    base::span<const StreamParser::TrackId> render_track_ids)
    : metadata_track_id_(metadata_track_id), it35_prefix_type_(prefix_type) {
  for (const auto& render_track_id : render_track_ids) {
    render_tracks_.emplace(render_track_id, RenderTrack());
  }
}

HdrMetadataTrack::~HdrMetadataTrack() = default;

HdrMetadataTrack::RenderTrack::RenderTrack() = default;
HdrMetadataTrack::RenderTrack::RenderTrack(RenderTrack&&) = default;
HdrMetadataTrack::RenderTrack& HdrMetadataTrack::RenderTrack::operator=(
    RenderTrack&&) = default;
HdrMetadataTrack::RenderTrack::~RenderTrack() = default;

void HdrMetadataTrack::AttachMetadataOrHoldBuffers(
    StreamParser::BufferQueueMap* buffers,
    bool flush_all_buffers) {
  for (auto buffers_it = buffers->begin(); buffers_it != buffers->end();) {
    const auto& buffer_track_id = buffers_it->first;
    auto& buffer_queue = buffers_it->second;

    // If this is the metadata track, move its buffers to `metadata_`.
    if (buffer_track_id == metadata_track_id_) {
      for (const auto& buf : buffer_queue) {
        // Parse the metadata in the sample.
        gfx::HDRMetadata buf_metadata;
        switch (it35_prefix_type_) {
          case MetadataIT35SampleEntry::IT35PrefixType::kSmpteSt2094App5: {
            buf_metadata.SetSerializedAgtm(base::span(*buf));
            break;
          }
          case MetadataIT35SampleEntry::IT35PrefixType::kUnknown:
            break;
        }
        metadata_.SetInterval(buf->timestamp(),
                              buf->timestamp() + buf->duration(), buf_metadata);
      }
      buffer_queue.clear();
      buffers_it = buffers->erase(buffers_it);
      continue;
    }

    // If this a render track, move its buffers to `render_tracks_` for later
    // processing.
    auto it = render_tracks_.find(buffer_track_id);
    if (it != render_tracks_.end()) {
      auto& held_buffers = it->second.held_buffers;
      held_buffers.insert(held_buffers.end(),
                          std::make_move_iterator(buffer_queue.begin()),
                          std::make_move_iterator(buffer_queue.end()));
      buffer_queue.clear();
      buffers_it = buffers->erase(buffers_it);
      continue;
    }

    ++buffers_it;
  }

  for (auto& [track_id, render_track] : render_tracks_) {
    auto& held_buffers = render_track.held_buffers;
    auto first_unprocessed_buffer = held_buffers.begin();
    for (; first_unprocessed_buffer != held_buffers.end();
         ++first_unprocessed_buffer) {
      auto& buf = *first_unprocessed_buffer;

      // Note that IntervalMap::Find always returns a value.
      const auto& metadata = metadata_.find(buf->timestamp()).value();
      if (metadata.has_value()) {
        buf->WritableSideData().hdr_metadata.MergeMetadataFrom(*metadata);
      } else {
        if (flush_all_buffers) {
          // If all buffers have been received, then continue check all buffers
          // for metadata.
          continue;
        } else {
          // Otherwise, stop now.
          break;
        }
      }
    }

    // Move all buffers up to (but not including) `first_unprocessed_buffer`
    // from `render_tracks[track_id].held_buffers` to `buffers[track_id]`.
    if (first_unprocessed_buffer != held_buffers.begin()) {
      auto& dest_queue = (*buffers)[track_id];
      dest_queue.insert(dest_queue.end(),
                        std::make_move_iterator(held_buffers.begin()),
                        std::make_move_iterator(first_unprocessed_buffer));
      held_buffers.erase(held_buffers.begin(), first_unprocessed_buffer);
    }
  }

  if (flush_all_buffers) {
    metadata_.clear();
  }
}

void HdrMetadataTrack::Reset() {
  for (auto& [track_id, render_track] : render_tracks_) {
    render_track.held_buffers.clear();
  }
  metadata_.clear();
}

}  // namespace media::mp4
