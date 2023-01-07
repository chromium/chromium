// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/media_player_watch_time.h"

namespace content {

MediaPlayerWatchTime::MediaPlayerWatchTime(
    GURL url,
    GURL origin,
    base::TimeDelta cumulative_watch_time,
    base::TimeDelta last_timestamp,
    bool has_video,
    bool has_audio)
    : url(url),
      origin(origin),
      cumulative_watch_time(cumulative_watch_time),
      last_timestamp(last_timestamp),
      has_video(has_video),
      has_audio(has_audio) {}

MediaPlayerWatchTime::MediaPlayerWatchTime(const MediaPlayerWatchTime& other) =
    default;

}  // namespace content
