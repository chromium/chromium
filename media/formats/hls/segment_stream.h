// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_SEGMENT_STREAM_H_
#define MEDIA_FORMATS_HLS_SEGMENT_STREAM_H_

#include <tuple>

#include "base/containers/queue.h"
#include "media/formats/hls/media_playlist.h"
#include "media/formats/hls/media_segment.h"

namespace media::hls {

// Represents a segment, its start time, and its end time.
using SegmentInfo =
    std::tuple<scoped_refptr<MediaSegment>, base::TimeDelta, base::TimeDelta>;

// A segment stream represents the queue of segments which should be downloaded
// in order. It supports configurable seeking as well as checks for size and
// any segments. Most importantly, it can accept a new media playlist, and
// replace segments that have not yet been fetched with the corresponding
// segment from the new playlist. This allows HlsRendition implementations to
// support stream adaptation. All methods are accessed from the same sequence as
// this class is destroyed and created on.
class MEDIA_EXPORT SegmentStream {
 public:
  ~SegmentStream();
  SegmentStream(const SegmentStream&) = delete;
  SegmentStream(scoped_refptr<MediaPlaylist> playlist, bool seekable);

  // Removes the next segment from the head of the queue and returns it.
  SegmentInfo GetNextSegment();

  // Moves the head of the queue to the provided time, and returns if the seek
  // was successful. unseekable playlists return false always.
  bool Seek(base::TimeDelta seek_time);

  // Sets a new playlist, which will either:
  //  - regenerate a new sequence of segments if the playlist is seekable
  //  - append all new segments to the existing queue if the playlist is not
  //    seekable.
  // Either way, the `active_playlist_` member will be set.
  void SetNewPlaylist(scoped_refptr<MediaPlaylist> playlist);

  // Gets the upper limit in duration for any segment in the queue.
  base::TimeDelta GetMaxDuration() const;

  // Is this underlying container empty?
  bool Exhausted() const;

  // Get the start time of the next segment in the queue.
  base::TimeDelta NextSegmentStartTime() const;

  // Does the playlist have any segments at all?
  bool PlaylistHasSegments() const;

  // How many segments are left to play?
  size_t QueueSize() const;

  // Erase all segments, and set the `next_segment_start_` time to
  // `new_start_time`. This is used when a live playlist has been paused for
  // a long time, and the new content it needs to fetch will be off in the
  // future somewhere.
  void ResetExpectingFutureManifest(base::TimeDelta new_start_time);

 private:
  class SegmentIndex {
   public:
    explicit SegmentIndex(const MediaSegment& segment);
    SegmentIndex(types::DecimalInteger discontinuity,
                 types::DecimalInteger media);

    bool operator<(const SegmentIndex& other) const;
    bool operator<=(const SegmentIndex& other) const;
    bool operator==(const SegmentIndex& other) const;
    bool operator>(const SegmentIndex& other) const;

    SegmentIndex Next() const;

    SegmentIndex MaxOf(const MediaSegment& other) const;

    types::DecimalInteger media_sequence_;
    types::DecimalInteger discontinuity_sequence_;
  };

  const bool seekable_;
  base::TimeDelta next_segment_start_;

  base::queue<scoped_refptr<MediaSegment>> segments_;
  scoped_refptr<MediaPlaylist> active_playlist_;

  SegmentIndex highest_segment_index_ = {0, 0};

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media::hls

#endif  // MEDIA_FORMATS_HLS_SEGMENT_STREAM_H_
