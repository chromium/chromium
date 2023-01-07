// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_MEDIA_PLAYER_WATCH_TIME_H_
#define CONTENT_PUBLIC_BROWSER_MEDIA_PLAYER_WATCH_TIME_H_

#include "base/time/time.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

struct CONTENT_EXPORT MediaPlayerWatchTime {
  MediaPlayerWatchTime(GURL url,
                       GURL origin,
                       base::TimeDelta cumulative_watch_time,
                       base::TimeDelta last_timestamp,
                       bool has_video,
                       bool has_audio);
  MediaPlayerWatchTime(const MediaPlayerWatchTime& other);
  ~MediaPlayerWatchTime() = default;

  GURL url;
  GURL origin;
  base::TimeDelta cumulative_watch_time;
  base::TimeDelta last_timestamp;
  bool has_video;
  bool has_audio;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MEDIA_PLAYER_WATCH_TIME_H_
