// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_YOUTUBE_MUSIC_YOUTUBE_MUSIC_API_REQUEST_TYPES_H_
#define GOOGLE_APIS_YOUTUBE_MUSIC_YOUTUBE_MUSIC_API_REQUEST_TYPES_H_

#include <optional>
#include <string>

namespace google_apis::youtube_music {

// Payload used as a request body for the API request that prepares the playback
// queue.
struct PlaybackQueuePrepareRequestPayload {
  enum class ExplicitFilter {
    kNone,
    kBestEffort,
  };

  enum class ShuffleMode {
    kUnspecified,
    kOff,
    kOn,
  };

  PlaybackQueuePrepareRequestPayload(
      std::string playable_id,
      std::optional<ExplicitFilter> explicit_filter,
      std::optional<ShuffleMode> shuffle_mode);
  PlaybackQueuePrepareRequestPayload(const PlaybackQueuePrepareRequestPayload&);
  PlaybackQueuePrepareRequestPayload& operator=(
      const PlaybackQueuePrepareRequestPayload&);
  ~PlaybackQueuePrepareRequestPayload();

  std::string ToJson() const;

  std::string playable_id;

  std::optional<ExplicitFilter> explicit_filter;

  std::optional<ShuffleMode> shuffle_mode;
};

}  // namespace google_apis::youtube_music

#endif  // GOOGLE_APIS_YOUTUBE_MUSIC_YOUTUBE_MUSIC_API_REQUEST_TYPES_H_
