// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_track_video_stats.h"

namespace blink {

MediaStreamTrackVideoStats::MediaStreamTrackVideoStats() = default;

void MediaStreamTrackVideoStats::setStats(
    MediaStreamTrackPlatform::VideoFrameStats stats) {
  stats_ = stats;
}

uint64_t MediaStreamTrackVideoStats::deliveredFrames() const {
  // TODO(https://crbug.com/1472978): Add a UpdateVideoStatsIfNeeded() method
  // (setting stats from MediaStreamTrackImpl) and call it here.
  return stats_.deliverable_frames;
}

uint64_t MediaStreamTrackVideoStats::discardedFrames() const {
  // TODO(https://crbug.com/1472978): Add a UpdateVideoStatsIfNeeded() method
  // (setting stats from MediaStreamTrackImpl) and call it here.
  return stats_.discarded_frames;
}

uint64_t MediaStreamTrackVideoStats::totalFrames() const {
  // TODO(https://crbug.com/1472978): Add a UpdateVideoStatsIfNeeded() method
  // (setting stats from MediaStreamTrackImpl) and call it here.
  return stats_.deliverable_frames + stats_.discarded_frames +
         stats_.dropped_frames;
}

}  // namespace blink
