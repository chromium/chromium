// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/youtube_music/youtube_music_api_request_types.h"

#include <string>

#include "base/check.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"

namespace google_apis::youtube_music {

namespace {

constexpr char kPlayableIdKey[] = "playableId";

constexpr char kExplicitFilterKey[] = "explicitFilter";
constexpr char kExplicitFilterNone[] = "none";
constexpr char kExplicitFilterBestEffort[] = "besteffort";

constexpr char kShuffleModeKey[] = "shuffleMode";
constexpr char kShuffleModeUnspecified[] = "SHUFFLE_MODE_UNSPECIFIED";
constexpr char kShuffleModeOff[] = "OFF";
constexpr char kShuffleModeOn[] = "ON";

constexpr char kPlaybackReportingTokenKey[] = "playbackReportingToken";
constexpr char kClientCurrentTimeKey[] = "clientCurrentTime";
constexpr char kPlaybackStartOffsetKey[] = "playbackStartOffset";
constexpr char kMediaTimeCurrentKey[] = "mediaTimeCurrent";
constexpr char kPlaybackStartDataKey[] = "playbackStartData";

constexpr char kConnectionTypeKey[] = "connectionType";
constexpr char kConnectionTypeUnspecified[] = "CONNECTION_TYPE_UNSPECIFIED";
constexpr char kConnectionTypeActiveUncategorized[] =
    "CONNECTION_TYPE_ACTIVE_UNCATEGORIZED";
constexpr char kConnectionTypeNone[] = "CONNECTION_TYPE_NONE";
constexpr char kConnectionTypeWifi[] = "CONNECTION_TYPE_WIFI";
constexpr char kConnectionTypeCellular2g[] = "CONNECTION_TYPE_CELLULAR_2G";
constexpr char kConnectionTypeCellular3g[] = "CONNECTION_TYPE_CELLULAR_3G";
constexpr char kConnectionTypeCellular4g[] = "CONNECTION_TYPE_CELLULAR_4G";
constexpr char kConnectionTypeCellularUnknown[] =
    "CONNECTION_TYPE_CELLULAR_UNKNOWN";
constexpr char kConnectionTypeDisco[] = "CONNECTION_TYPE_DISCO";
constexpr char kConnectionTypeWifiMetered[] = "CONNECTION_TYPE_WIFI_METERED";
constexpr char kConnectionTypeCellular5gSa[] = "CONNECTION_TYPE_CELLULAR_5G_SA";
constexpr char kConnectionTypeCellular5gNsa[] =
    "CONNECTION_TYPE_CELLULAR_5G_NSA";
constexpr char kConnectionTypeWired[] = "CONNECTION_TYPE_WIRED";
constexpr char kConnectionTypeInvalid[] = "CONNECTION_TYPE_INVALID";

constexpr char kWatchTimeSegmentsKey[] = "watchTimeSegments";
constexpr char kMediaTimeStartKey[] = "mediaTimeStart";
constexpr char kMediaTimeEndKey[] = "mediaTimeEnd";
constexpr char kClientStartTimeKey[] = "clientStartTime";

constexpr char kPlaybackStateKey[] = "playbackState";
constexpr char kPlaybackStateUnspecified[] = "PLAYBACK_STATE_UNSPECIFIED";
constexpr char kPlaybackStatePlaying[] = "PLAYBACK_STATE_PLAYING";
constexpr char kPlaybackStatePaused[] = "PLAYBACK_STATE_PAUSED";
constexpr char kPlaybackStateCompleted[] = "PLAYBACK_STATE_COMPLETED";

const char* GetExplicitFilterValue(
    const PlaybackQueuePrepareRequestPayload::ExplicitFilter& explicit_filter) {
  switch (explicit_filter) {
    case PlaybackQueuePrepareRequestPayload::ExplicitFilter::kNone:
      return kExplicitFilterNone;
    case PlaybackQueuePrepareRequestPayload::ExplicitFilter::kBestEffort:
      return kExplicitFilterBestEffort;
  }
}

const char* GetShuffleModeValue(
    const PlaybackQueuePrepareRequestPayload::ShuffleMode& shuffle_mode) {
  switch (shuffle_mode) {
    case PlaybackQueuePrepareRequestPayload::ShuffleMode::kUnspecified:
      return kShuffleModeUnspecified;
    case PlaybackQueuePrepareRequestPayload::ShuffleMode::kOff:
      return kShuffleModeOff;
    case PlaybackQueuePrepareRequestPayload::ShuffleMode::kOn:
      return kShuffleModeOn;
  }
}

std::string GetConnectionTypeValue(
    const ReportPlaybackRequestPayload::ConnectionType& connection_type) {
  switch (connection_type) {
    case ReportPlaybackRequestPayload::ConnectionType::kUnspecified:
      return kConnectionTypeUnspecified;
    case ReportPlaybackRequestPayload::ConnectionType::kActiveUncategorized:
      return kConnectionTypeActiveUncategorized;
    case ReportPlaybackRequestPayload::ConnectionType::kNone:
      return kConnectionTypeNone;
    case ReportPlaybackRequestPayload::ConnectionType::kWifi:
      return kConnectionTypeWifi;
    case ReportPlaybackRequestPayload::ConnectionType::kCellular2g:
      return kConnectionTypeCellular2g;
    case ReportPlaybackRequestPayload::ConnectionType::kCellular3g:
      return kConnectionTypeCellular3g;
    case ReportPlaybackRequestPayload::ConnectionType::kCellular4g:
      return kConnectionTypeCellular4g;
    case ReportPlaybackRequestPayload::ConnectionType::kCellularUnknown:
      return kConnectionTypeCellularUnknown;
    case ReportPlaybackRequestPayload::ConnectionType::kDisco:
      return kConnectionTypeDisco;
    case ReportPlaybackRequestPayload::ConnectionType::kWifiMetered:
      return kConnectionTypeWifiMetered;
    case ReportPlaybackRequestPayload::ConnectionType::kCellular5gSa:
      return kConnectionTypeCellular5gSa;
    case ReportPlaybackRequestPayload::ConnectionType::kCellular5gNsa:
      return kConnectionTypeCellular5gNsa;
    case ReportPlaybackRequestPayload::ConnectionType::kWired:
      return kConnectionTypeWired;
    case ReportPlaybackRequestPayload::ConnectionType::kInvalid:
      return kConnectionTypeInvalid;
  }
}

std::string GetPlaybackStateValue(
    const ReportPlaybackRequestPayload::PlaybackState& playback_state) {
  switch (playback_state) {
    case ReportPlaybackRequestPayload::PlaybackState::kUnspecified:
      return kPlaybackStateUnspecified;
    case ReportPlaybackRequestPayload::PlaybackState::kPlaying:
      return kPlaybackStatePlaying;
    case ReportPlaybackRequestPayload::PlaybackState::kPaused:
      return kPlaybackStatePaused;
    case ReportPlaybackRequestPayload::PlaybackState::kCompleted:
      return kPlaybackStateCompleted;
  }
}

std::string GetTimeDeltaString(const base::TimeDelta& time_delta) {
  return base::NumberToString(time_delta.InSeconds()) + "s";
}

}  // namespace

PlaybackQueuePrepareRequestPayload::PlaybackQueuePrepareRequestPayload(
    std::string playable_id,
    std::optional<ExplicitFilter> explicit_filter,
    std::optional<ShuffleMode> shuffle_mode)
    : playable_id(playable_id),
      explicit_filter(explicit_filter),
      shuffle_mode(shuffle_mode) {}
PlaybackQueuePrepareRequestPayload::PlaybackQueuePrepareRequestPayload(
    const PlaybackQueuePrepareRequestPayload&) = default;
PlaybackQueuePrepareRequestPayload&
PlaybackQueuePrepareRequestPayload::operator=(
    const PlaybackQueuePrepareRequestPayload&) = default;
PlaybackQueuePrepareRequestPayload::~PlaybackQueuePrepareRequestPayload() =
    default;

std::string PlaybackQueuePrepareRequestPayload::ToJson() const {
  base::Value::Dict root;

  CHECK(!playable_id.empty());
  root.Set(kPlayableIdKey, playable_id);
  if (explicit_filter.has_value()) {
    root.Set(kExplicitFilterKey,
             GetExplicitFilterValue(explicit_filter.value()));
  }
  if (shuffle_mode.has_value()) {
    root.Set(kShuffleModeKey, GetShuffleModeValue(shuffle_mode.value()));
  }

  const std::optional<std::string> json = base::WriteJson(root);
  CHECK(json);

  return json.value();
}

PlaybackQueueNextRequestPayload::PlaybackQueueNextRequestPayload() = default;
PlaybackQueueNextRequestPayload::~PlaybackQueueNextRequestPayload() = default;

std::string PlaybackQueueNextRequestPayload::ToJson() const {
  // All fields are optional so this is currently an empty dictionary.
  base::Value::Dict root;

  const std::optional<std::string> json = base::WriteJson(root);
  CHECK(json);

  return json.value();
}

ReportPlaybackRequestPayload::Params::Params(
    const bool initial_report,
    const std::string& playback_reporting_token,
    const base::Time client_current_time,
    const base::TimeDelta playback_start_offset,
    const base::TimeDelta media_time_current,
    const ConnectionType connection_type,
    const PlaybackState playback_state,
    const std::vector<WatchTimeSegment>& watch_time_segments)
    : initial_report(initial_report),
      playback_reporting_token(playback_reporting_token),
      client_current_time(client_current_time),
      playback_start_offset(playback_start_offset),
      media_time_current(media_time_current),
      connection_type(connection_type),
      playback_state(playback_state),
      watch_time_segments(watch_time_segments) {}
ReportPlaybackRequestPayload::Params::Params(const Params&) = default;
ReportPlaybackRequestPayload::Params&
ReportPlaybackRequestPayload::Params::operator=(const Params&) = default;
ReportPlaybackRequestPayload::Params::~Params() = default;

ReportPlaybackRequestPayload::ReportPlaybackRequestPayload(const Params& params)
    : params(params) {
  for (const WatchTimeSegment& watch_time_segment :
       params.watch_time_segments) {
    CHECK_LT(watch_time_segment.media_time_start,
             watch_time_segment.media_time_end);
  }
}
ReportPlaybackRequestPayload::ReportPlaybackRequestPayload(
    const ReportPlaybackRequestPayload&) = default;
ReportPlaybackRequestPayload& ReportPlaybackRequestPayload::operator=(
    const ReportPlaybackRequestPayload&) = default;
ReportPlaybackRequestPayload::~ReportPlaybackRequestPayload() = default;

std::string ReportPlaybackRequestPayload::ToJson() const {
  CHECK(!params.playback_reporting_token.empty());

  base::Value::Dict root;
  root.Set(kPlaybackReportingTokenKey, params.playback_reporting_token);
  root.Set(kClientCurrentTimeKey,
           base::TimeFormatAsIso8601(params.client_current_time));
  root.Set(kPlaybackStartOffsetKey,
           GetTimeDeltaString(params.playback_start_offset));
  root.Set(kMediaTimeCurrentKey, GetTimeDeltaString(params.media_time_current));
  root.Set(kPlaybackStateKey, GetPlaybackStateValue(params.playback_state));

  if (params.initial_report) {
    root.Set(kPlaybackStartDataKey,
             base::Value::Dict().Set(
                 kConnectionTypeKey,
                 GetConnectionTypeValue(params.connection_type)));
  }

  if (!params.watch_time_segments.empty()) {
    base::Value::List segment_list = base::Value::List();
    for (const WatchTimeSegment& watch_time_segment :
         params.watch_time_segments) {
      segment_list.Append(
          base::Value::Dict()
              .Set(kMediaTimeStartKey,
                   GetTimeDeltaString(watch_time_segment.media_time_start))
              .Set(kMediaTimeEndKey,
                   GetTimeDeltaString(watch_time_segment.media_time_end))
              .Set(kClientStartTimeKey,
                   base::TimeFormatAsIso8601(
                       watch_time_segment.client_start_time))
              .Set(kConnectionTypeKey,
                   GetConnectionTypeValue(params.connection_type)));
    }
    root.Set(kWatchTimeSegmentsKey, std::move(segment_list));
  }

  const std::optional<std::string> json = base::WriteJson(root);
  CHECK(json);

  return json.value();
}

SignedRequest::SignedRequest(RequestSender* sender)
    : UrlFetchRequestBase(sender, ProgressCallback(), ProgressCallback()) {}

SignedRequest::~SignedRequest() = default;

void SignedRequest::SetSigningHeaders(std::vector<std::string>&& headers) {
  headers_ = headers;
}

HttpRequestMethod SignedRequest::GetRequestType() const {
  // Signed requests must have a body so they are always POSTs.
  return HttpRequestMethod::kPost;
}

std::vector<std::string> SignedRequest::GetExtraRequestHeaders() const {
  CHECK(!headers_.empty());
  return headers_;
}

std::string ParseErrorJson(const std::string& response_body) {
  const char kErrorKey[] = "error";
  const char kDetailsKey[] = "details";
  const char kTypeKey[] = "@type";
  const char kLocalizedMessage[] =
      "type.googleapis.com/google.rpc.LocalizedMessage";
  const char kMessageKey[] = "message";

  std::unique_ptr<const base::Value> value(
      google_apis::ParseJson(response_body));
  if (!value || !value->is_dict()) {
    return std::string();
  }

  const base::Value::Dict* error = value->GetDict().FindDict(kErrorKey);
  if (!error) {
    return std::string();
  }

  const base::Value::List* details = error->FindList(kDetailsKey);
  if (!details) {
    return std::string();
  }

  for (const base::Value& detail : *details) {
    if (!detail.is_dict()) {
      continue;
    }

    const base::Value::Dict& detail_dict = detail.GetDict();
    const std::string* type = detail_dict.FindString(kTypeKey);
    if (!type || kLocalizedMessage != *type) {
      continue;
    }

    // This is the localized message detail. Try to extract the message.
    const std::string* message = detail_dict.FindString(kMessageKey);
    return !message ? std::string() : *message;
  }

  // No detail with the correct type found.
  return std::string();
}

}  // namespace google_apis::youtube_music
