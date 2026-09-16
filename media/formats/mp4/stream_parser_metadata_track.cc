// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/stream_parser_metadata_track.h"

#include <iterator>
#include <vector>

#include "media/base/decoder_buffer_side_data.h"
#include "media/base/stream_parser_buffer.h"

namespace media {

StreamParserMetadataTrack::StreamParserMetadataTrack(
    StreamParser::TrackId metadata_track_id,
    MetadataTrack::IT35PrefixType prefix_type,
    base::span<const StreamParser::TrackId> render_track_ids)
    : metadata_track_id_(metadata_track_id) {
  for (const auto& render_track_id : render_track_ids) {
    render_tracks_.emplace(render_track_id, RenderTrack(prefix_type));
  }
}

StreamParserMetadataTrack::~StreamParserMetadataTrack() = default;

StreamParserMetadataTrack::RenderTrack::RenderTrack(
    MetadataTrack::IT35PrefixType prefix_type)
    : metadata_track(prefix_type) {}
StreamParserMetadataTrack::RenderTrack::RenderTrack(RenderTrack&&) = default;
StreamParserMetadataTrack::RenderTrack&
StreamParserMetadataTrack::RenderTrack::operator=(RenderTrack&&) = default;
StreamParserMetadataTrack::RenderTrack::~RenderTrack() = default;

void StreamParserMetadataTrack::AttachMetadataOrHoldBuffers(
    StreamParser::BufferQueueMap* buffers,
    bool flush_all_buffers) {
  for (auto buffers_it = buffers->begin(); buffers_it != buffers->end();) {
    const auto& buffer_track_id = buffers_it->first;
    auto& buffer_queue = buffers_it->second;

    // If this is the metadata track, insert its buffers into all
    // `MetadataTrack` instances.
    if (buffer_track_id == metadata_track_id_) {
      for (const auto& buf : buffer_queue) {
        for (auto& [track_id, render_track] : render_tracks_) {
          render_track.metadata_track.InsertMetadataBuffer(*buf);
        }
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

      bool metadata_attached =
          render_track.metadata_track.TryAttachMetadata(*buf);
      if (!metadata_attached) {
        if (flush_all_buffers) {
          // If all buffers have been received, then continue to check remaining
          // buffers.
          continue;
        } else {
          // Otherwise, stop now and wait for more metadata.
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
    for (auto& [track_id, render_track] : render_tracks_) {
      render_track.metadata_track.Reset();
    }
  }
}

void StreamParserMetadataTrack::Reset() {
  for (auto& [track_id, render_track] : render_tracks_) {
    render_track.held_buffers.clear();
    render_track.metadata_track.Reset();
  }
}

}  // namespace media
