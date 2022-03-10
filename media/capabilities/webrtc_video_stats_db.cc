// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capabilities/webrtc_video_stats_db.h"

#include "base/check_op.h"
#include "base/format_macros.h"
#include "base/metrics/field_trial_params.h"
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

// Validates the codec profile enum in case it's been compromised. Returns
// VIDEO_CODEC_PROFILE_UNKNOWN if `codec_profile` is outside the valid range.
VideoCodecProfile ValidateVideoCodecProfile(VideoCodecProfile codec_profile) {
  if (codec_profile > VIDEO_CODEC_PROFILE_MIN &&
      codec_profile <= VIDEO_CODEC_PROFILE_MAX) {
    return codec_profile;
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
  return VideoDescKey(is_decode_stats, ValidateVideoCodecProfile(codec_profile),
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

std::string WebrtcVideoStatsDB::VideoDescKey::ToLogStringForDebug() const {
  return "Key {" + Serialize() + "}";
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
int WebrtcVideoStatsDB::GetMaxDaysToKeepStats() {
  return base::GetFieldTrialParamByFeatureAsInt(
      kWebrtcMediaCapabilitiesParameters, kMaxDaysToKeepStatsParamName,
      kMaxDaysToKeepStatsDefault);
}

// static
int WebrtcVideoStatsDB::GetMaxEntriesPerConfig() {
  return base::GetFieldTrialParamByFeatureAsInt(
      kWebrtcMediaCapabilitiesParameters, kMaxEntriesPerConfigParamName,
      kMaxEntriesPerConfigDefault);
}

}  // namespace media
