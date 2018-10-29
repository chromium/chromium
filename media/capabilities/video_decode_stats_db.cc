// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capabilities/video_decode_stats_db.h"

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "media/capabilities/bucket_utility.h"

namespace media {

// static
VideoDecodeStatsDB::VideoDescKey
VideoDecodeStatsDB::VideoDescKey::MakeBucketedKey(
    VideoCodecProfile codec_profile,
    const gfx::Size& size,
    int frame_rate) {
  // Bucket size and framerate to prevent an explosion of one-off values in the
  // database and add basic guards against fingerprinting.
  return VideoDescKey(codec_profile, GetSizeBucket(size),
                      GetFpsBucket(frame_rate));
}

VideoDecodeStatsDB::VideoDescKey::VideoDescKey(VideoCodecProfile codec_profile,
                                               const gfx::Size& size,
                                               int frame_rate)
    : codec_profile(codec_profile), size(size), frame_rate(frame_rate) {}

std::string VideoDecodeStatsDB::VideoDescKey::Serialize() const {
  return base::StringPrintf("%d|%s|%d", static_cast<int>(codec_profile),
                            size.ToString().c_str(), frame_rate);
}

std::string VideoDecodeStatsDB::VideoDescKey::ToLogString() const {
  return "Key {" + Serialize() + "}";
}

VideoDecodeStatsDB::DecodeStatsEntry::DecodeStatsEntry(
    uint64_t frames_decoded,
    uint64_t frames_dropped,
    uint64_t frames_power_efficient)
    : frames_decoded(frames_decoded),
      frames_dropped(frames_dropped),
      frames_power_efficient(frames_power_efficient) {
  DCHECK_GE(frames_decoded, 0u);
  DCHECK_GE(frames_dropped, 0u);
  DCHECK_GE(frames_power_efficient, 0u);
}

VideoDecodeStatsDB::DecodeStatsEntry::DecodeStatsEntry(
    const DecodeStatsEntry& entry)
    : frames_decoded(entry.frames_decoded),
      frames_dropped(entry.frames_dropped),
      frames_power_efficient(entry.frames_power_efficient) {}

std::string VideoDecodeStatsDB::DecodeStatsEntry::ToLogString() const {
  return base::StringPrintf(
      "DecodeStatsEntry {frames decoded:%" PRIu64 ", dropped:%" PRIu64
      ", power efficient:%" PRIu64 "}",
      frames_decoded, frames_dropped, frames_power_efficient);
}

VideoDecodeStatsDB::DecodeStatsEntry& VideoDecodeStatsDB::DecodeStatsEntry::
operator+=(const DecodeStatsEntry& right) {
  DCHECK_GE(right.frames_decoded, 0u);
  DCHECK_GE(right.frames_dropped, 0u);
  DCHECK_GE(right.frames_power_efficient, 0u);

  frames_decoded += right.frames_decoded;
  frames_dropped += right.frames_dropped;
  frames_power_efficient += right.frames_power_efficient;
  return *this;
}

bool operator==(const VideoDecodeStatsDB::VideoDescKey& x,
                const VideoDecodeStatsDB::VideoDescKey& y) {
  return x.codec_profile == y.codec_profile && x.size == y.size &&
         x.frame_rate == y.frame_rate;
}
bool operator!=(const VideoDecodeStatsDB::VideoDescKey& x,
                const VideoDecodeStatsDB::VideoDescKey& y) {
  return !(x == y);
}

bool operator==(const VideoDecodeStatsDB::DecodeStatsEntry& x,
                const VideoDecodeStatsDB::DecodeStatsEntry& y) {
  return x.frames_decoded == y.frames_decoded &&
         x.frames_dropped == y.frames_dropped &&
         x.frames_power_efficient == y.frames_power_efficient;
}

bool operator!=(const VideoDecodeStatsDB::DecodeStatsEntry& x,
                const VideoDecodeStatsDB::DecodeStatsEntry& y) {
  return !(x == y);
}

VideoDecodeStatsDB::~VideoDecodeStatsDB() {
  // Tracking down crash. See https://crbug/865321.
  CHECK(!dependent_db_) << __func__ << " Destroying before dependent_db_!";
}

}  // namespace media
