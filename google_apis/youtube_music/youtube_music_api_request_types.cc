// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/youtube_music/youtube_music_api_request_types.h"

#include <string>

#include "base/check.h"
#include "base/json/json_writer.h"
#include "base/notreached.h"

namespace google_apis::youtube_music {

namespace {

constexpr char kPlayableIdKey[] = "playableId";
constexpr char kExplicitFilterKey[] = "explicitFilter";
constexpr char kShuffleModeKey[] = "shuffleMode";
const char kExplicitFilterNone[] = "none";
const char kExplicitFilterBestEffort[] = "besteffort";
const char kShuffleModeUnspecified[] = "SHUFFLE_MODE_UNSPECIFIED";
const char kShuffleModeOff[] = "OFF";
const char kShuffleModeOn[] = "ON";

const char* GetExplicitFilterValue(
    const PlaybackQueuePrepareRequestPayload::ExplicitFilter& explicit_filter) {
  switch (explicit_filter) {
    case PlaybackQueuePrepareRequestPayload::ExplicitFilter::kNone:
      return kExplicitFilterNone;
    case PlaybackQueuePrepareRequestPayload::ExplicitFilter::kBestEffort:
      return kExplicitFilterBestEffort;
  }
  NOTREACHED_NORETURN();
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
  NOTREACHED_NORETURN();
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

}  // namespace google_apis::youtube_music
