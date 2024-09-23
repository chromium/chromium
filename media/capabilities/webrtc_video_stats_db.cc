// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/capabilities/webrtc_video_stats_db.h"

#include "base/check_op.h"
#include "base/format_macros.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "media/base/media_switches.h"
#include "media/capabilities/bucket_utility.h"

namespace media {

namespace {
// Default value for maximum number of days until a stats entry is considered
// expired.
constexpr int kMaxDaysToKeepStatsDefault = 30;
// Default value for maximum number of stats entries per config.
constexpr int kMaxEntriesPerConfigDefault = 10;
// Field trial parameter names.
constexpr char kMaxDaysToKeepStatsParamName[] = "db_days_to_keep_stats";
constexpr char kMaxEntriesPerConfigParamName[] = "db_max_entries_per_cpnfig";

// Group similar codec profiles to the same entry. Returns
// VIDEO_CODEC_PROFILE_UNKNOWN if `codec_profile` is outside the valid range or
// not tracked. The reason to not track all codec profiles is to limit the
// fingerprinting surface.
VideoCodecProfile GetWebrtcCodecProfileBucket(VideoCodecProfile codec_profile) {
  if (codec_profile >= H264PROFILE_MIN && codec_profile <= H264PROFILE_MAX) {
    return H264PROFILE_MIN;
  }
  if (codec_profile >= VP8PROFILE_MIN && codec_profile <= VP8PROFILE_MAX) {
    return VP8PROFILE_MIN;
  }
  if (codec_profile == VP9PROFILE_PROFILE0) {
    return VP9PROFILE_PROFILE0;
  }
  if (codec_profile == VP9PROFILE_PROFILE2) {
    return VP9PROFILE_PROFILE2;
  }
  if (codec_profile >= AV1PROFILE_MIN && codec_profile <= AV1PROFILE_MAX) {
    return AV1PROFILE_MIN;
  }
  return VIDEO_CODEC_PROFILE_UNKNOWN;
}

}  // namespace

// static
WebrtcVideoStatsDB::VideoDescKey
WebrtcVideoStatsDB::VideoDescKey::MakeBucketedKey(
    bool is_decode_stats,
    VideoCodecProfile codec_profile,
    bool hardware_accelerated,
    int pixels) {
  // Bucket pixel size to prevent an explosion of one-off values in the
  // database and add basic guards against fingerprinting.
  return VideoDescKey(is_decode_stats,
                      GetWebrtcCodecProfileBucket(codec_profile),
                      hardware_accelerated, GetWebrtcPixelsBucket(pixels));
}

WebrtcVideoStatsDB::VideoDescKey::VideoDescKey(bool is_decode_stats,
                                               VideoCodecProfile codec_profile,
                                               bool hardware_accelerated,
                                               int pixels)
    : is_decode_stats(is_decode_stats),
      codec_profile(codec_profile),
      hardware_accelerated(hardware_accelerated),
      pixels(pixels) {}

std::string WebrtcVideoStatsDB::VideoDescKey::Serialize() const {
  std::string video_part = base::StringPrintf("%d|%d|%d|%d", is_decode_stats,
                                              static_cast<int>(codec_profile),
                                              hardware_accelerated, pixels);

  return video_part;
}

std::string WebrtcVideoStatsDB::VideoDescKey::SerializeWithoutPixels() const {
  std::string video_part =
      base::StringPrintf("%d|%d|%d|", is_decode_stats,
                         static_cast<int>(codec_profile), hardware_accelerated);

  return video_part;
}

std::string WebrtcVideoStatsDB::VideoDescKey::ToLogStringForDebug() const {
  return "Key {" + Serialize() + "}";
}

// static
std::optional<int> WebrtcVideoStatsDB::VideoDescKey::ParsePixelsFromKey(
    std::string key) {
  constexpr size_t kMinimumIndexOfLastSeparator = 5;
  size_t last_separator_index = key.rfind("|");
  if (last_separator_index != std::string::npos &&
      last_separator_index >= kMinimumIndexOfLastSeparator &&
      (last_separator_index + 1) < key.size()) {
    int parsed_pixels;
    if (base::StringToInt(&key.c_str()[last_separator_index + 1],
                          &parsed_pixels))
      return parsed_pixels;
  }
  return std::nullopt;
}

WebrtcVideoStatsDB::VideoStats::VideoStats(double timestamp,
                                           uint32_t frames_processed,
                                           uint32_t key_frames_processed,
                                           float p99_processing_time_ms)
    : timestamp(timestamp),
      frames_processed(frames_processed),
      key_frames_processed(key_frames_processed),
      p99_processing_time_ms(p99_processing_time_ms) {
  DCHECK_GE(frames_processed, 0u);
  DCHECK_GE(key_frames_processed, 0u);
  DCHECK_GE(p99_processing_time_ms, 0);
}

WebrtcVideoStatsDB::VideoStats::VideoStats(uint32_t frames_processed,
                                           uint32_t key_frames_processed,
                                           float p99_processing_time_ms)
    : VideoStats(/*timestamp=*/0.0,
                 frames_processed,
                 key_frames_processed,
                 p99_processing_time_ms) {}

WebrtcVideoStatsDB::VideoStats::VideoStats(const VideoStats& entry) = default;

WebrtcVideoStatsDB::VideoStats& WebrtcVideoStatsDB::VideoStats::operator=(
    const VideoStats& entry) = default;

std::string WebrtcVideoStatsDB::VideoStats::ToLogString() const {
  return base::StringPrintf(
      "VideoStats {Frames processed:%u, key frames processed:%u, p99 "
      "processing time:%.2f}",
      frames_processed, key_frames_processed, p99_processing_time_ms);
}

bool operator==(const WebrtcVideoStatsDB::VideoDescKey& x,
                const WebrtcVideoStatsDB::VideoDescKey& y) {
  return x.is_decode_stats == y.is_decode_stats &&
         x.codec_profile == y.codec_profile &&
         x.hardware_accelerated == y.hardware_accelerated &&
         x.pixels == y.pixels;
}
bool operator!=(const WebrtcVideoStatsDB::VideoDescKey& x,
                const WebrtcVideoStatsDB::VideoDescKey& y) {
  return !(x == y);
}

bool operator==(const WebrtcVideoStatsDB::VideoStats& x,
                const WebrtcVideoStatsDB::VideoStats& y) {
  return x.timestamp == y.timestamp &&
         x.frames_processed == y.frames_processed &&
         x.key_frames_processed == y.key_frames_processed &&
         x.p99_processing_time_ms == y.p99_processing_time_ms;
}
bool operator!=(const WebrtcVideoStatsDB::VideoStats& x,
                const WebrtcVideoStatsDB::VideoStats& y) {
  return !(x == y);
}

// static
base::TimeDelta WebrtcVideoStatsDB::GetMaxTimeToKeepStats() {
  return base::Days(base::GetFieldTrialParamByFeatureAsInt(
      kWebrtcMediaCapabilitiesParameters, kMaxDaysToKeepStatsParamName,
      kMaxDaysToKeepStatsDefault));
}

// static
int WebrtcVideoStatsDB::GetMaxEntriesPerConfig() {
  return base::GetFieldTrialParamByFeatureAsInt(
      kWebrtcMediaCapabilitiesParameters, kMaxEntriesPerConfigParamName,
      kMaxEntriesPerConfigDefault);
}

}  // namespace media
