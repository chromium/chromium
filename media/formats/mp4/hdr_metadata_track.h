// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP4_HDR_METADATA_TRACK_H_
#define MEDIA_FORMATS_MP4_HDR_METADATA_TRACK_H_

#include <optional>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "media/base/interval_map.h"
#include "media/base/media_export.h"
#include "media/base/stream_parser.h"
#include "media/base/stream_parser_buffer.h"
#include "media/formats/mp4/box_definitions.h"
#include "ui/gfx/hdr_metadata.h"

namespace media::mp4 {

// A HdrMetadataTrack is used to attach metadata from a timed metadata track to
// the StreamParserBuffers for the render track that they reference. This is
// done during parsing, before the StreamParserBuffers are passed to the
// decoder.
class MEDIA_EXPORT HdrMetadataTrack {
 public:
  HdrMetadataTrack(StreamParser::TrackId metadata_track_id,
                   MetadataIT35SampleEntry::IT35PrefixType prefix_type,
                   base::span<const StreamParser::TrackId> render_track_ids);
  ~HdrMetadataTrack();

  // Remove metadata buffers for `metadata_track_id` from `buffers`. For any
  // buffers in `buffers` that are among `render_track_ids`, attach the metadata
  // for the corresponding presentation time stamp to them. If metadata for the
  // corresponding time stamp does not exist, then hold the buffers (and all
  // subsequent buffers) internally. They will be returned to `buffers` in a
  // future call when the corresponding metadata buffers are found. If the
  // `flush_all_buffers` parameter is true, then return all held buffers to
  // `buffers`, even if no metadata has been found, and erase all cached
  // metadata.
  void AttachMetadataOrHoldBuffers(StreamParser::BufferQueueMap* buffers,
                                   bool flush_all_buffers);

  // Release all held buffers (they will never be returned), and all cached
  // metadata.
  void Reset();

 private:
  struct RenderTrack {
    RenderTrack();
    RenderTrack(const RenderTrack&) = delete;
    RenderTrack& operator=(const RenderTrack&) = delete;
    RenderTrack(RenderTrack&&);
    RenderTrack& operator=(RenderTrack&&);
    ~RenderTrack();

    StreamParser::BufferQueue held_buffers;
  };
  base::flat_map<StreamParser::TrackId, RenderTrack> render_tracks_;

  const StreamParser::TrackId metadata_track_id_;
  const MetadataIT35SampleEntry::IT35PrefixType it35_prefix_type_;

  // IntervalMap creates a default interval. In order to distinguish between
  // empty metadata and no metadata, use an optional vector as the data type.
  // This caches all of the metadata for the entire movie fragment (which may
  // need to be revisited for efficiency).
  IntervalMap<base::TimeDelta, std::optional<gfx::HDRMetadata>> metadata_;
};

}  // namespace media::mp4

#endif  // MEDIA_FORMATS_MP4_HDR_METADATA_TRACK_H_
