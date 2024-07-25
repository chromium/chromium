// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_YOUTUBE_MUSIC_YOUTUBE_MUSIC_API_REQUEST_TYPES_H_
#define GOOGLE_APIS_YOUTUBE_MUSIC_YOUTUBE_MUSIC_API_REQUEST_TYPES_H_

#include <optional>
#include <string>

#include "base/time/time.h"
#include "base/values.h"

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

// Payload used as a request body for the API request that reports the playback.
struct ReportPlaybackRequestPayload {
  enum class PlaybackState {
    kUnspecified,
    kPlaying,
    kPaused,
    kCompleted,
  };

  enum class ConnectionType {
    kUnspecified,
    kActiveUncategorized,
    kNone,
    kWifi,
    kCellular2g,
    kCellular3g,
    kCellular4g,
    kCellularUnknown,
    kDisco,
    kWifiMetered,
    kCellular5gSa,
    kCellular5gNsa,
    kWired,
    kInvalid,
  };

  struct WatchTimeSegment {
    base::TimeDelta media_time_start;

    base::TimeDelta media_time_end;

    base::Time client_start_time;
  };

  struct Params {
    std::string playback_reporting_token;

    base::Time client_current_time;

    base::TimeDelta playback_start_offset;

    base::TimeDelta media_time_current;

    ConnectionType connection_type;

    PlaybackState playback_state;

    std::optional<WatchTimeSegment> watch_time_segment;
  };

  explicit ReportPlaybackRequestPayload(const Params& params);
  ReportPlaybackRequestPayload(const ReportPlaybackRequestPayload&);
  ReportPlaybackRequestPayload& operator=(const ReportPlaybackRequestPayload&);
  ~ReportPlaybackRequestPayload();

  std::string ToJson() const;

  Params params;
};

}  // namespace google_apis::youtube_music

#endif  // GOOGLE_APIS_YOUTUBE_MUSIC_YOUTUBE_MUSIC_API_REQUEST_TYPES_H_
