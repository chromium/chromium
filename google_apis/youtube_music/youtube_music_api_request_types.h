// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_YOUTUBE_MUSIC_YOUTUBE_MUSIC_API_REQUEST_TYPES_H_
#define GOOGLE_APIS_YOUTUBE_MUSIC_YOUTUBE_MUSIC_API_REQUEST_TYPES_H_

#include <optional>
#include <string>

#include "base/time/time.h"
#include "base/values.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/base_requests.h"

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

// Payload for PlaybackQueueNextRequests. Currently, all fields are optional so
// it's empty;.
struct PlaybackQueueNextRequestPayload {
  PlaybackQueueNextRequestPayload();
  ~PlaybackQueueNextRequestPayload();

  std::string ToJson() const;
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
    Params(const bool initial_report,
           const std::string& playback_reporting_token,
           const base::Time client_current_time,
           const base::TimeDelta playback_start_offset,
           const base::TimeDelta media_time_current,
           const ConnectionType connection_type,
           const PlaybackState playback_state,
           const std::vector<WatchTimeSegment>& watch_time_segments);
    Params(const Params&);
    Params& operator=(const Params&);
    ~Params();

    bool initial_report;

    std::string playback_reporting_token;

    base::Time client_current_time;

    base::TimeDelta playback_start_offset;

    base::TimeDelta media_time_current;

    ConnectionType connection_type;

    PlaybackState playback_state;

    std::vector<WatchTimeSegment> watch_time_segments;
  };

  explicit ReportPlaybackRequestPayload(const Params& params);
  ReportPlaybackRequestPayload(const ReportPlaybackRequestPayload&);
  ReportPlaybackRequestPayload& operator=(const ReportPlaybackRequestPayload&);
  ~ReportPlaybackRequestPayload();

  std::string ToJson() const;

  Params params;
};

// Requests that can have their payload signed with a client certificate.
// Signing is implemented as a series of headers that are computed
// asynchronously so they must be set before the request begins.
class SignedRequest : public UrlFetchRequestBase {
 public:
  SignedRequest(RequestSender* sender);
  ~SignedRequest() override;

  void SetSigningHeaders(std::vector<std::string>&& headers);

 protected:
  HttpRequestMethod GetRequestType() const final;
  std::vector<std::string> GetExtraRequestHeaders() const override;

 private:
  std::vector<std::string> headers_;
};

struct ApiError {
  google_apis::ApiErrorCode error_code;

  // A localized error string if included with the response.
  std::string error_message;
};

// Returns the localized error message from the error JSON if it can be found.
// If a localized error message is not found, returns an empty string.
std::string ParseErrorJson(const std::string& response_body);

}  // namespace google_apis::youtube_music

#endif  // GOOGLE_APIS_YOUTUBE_MUSIC_YOUTUBE_MUSIC_API_REQUEST_TYPES_H_
