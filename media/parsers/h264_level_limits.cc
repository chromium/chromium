// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/parsers/h264_level_limits.h"

#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "media/parsers/h264_parser.h"

namespace media {
namespace {
struct LevelLimits {
  // All names and abbreviations are as in table A-1 in spec.
  // MaxMBPS, Max. macroblock processing rate (MB/s)
  uint32_t max_mbps;
  // MaxFS, Max. frame size (MBs)
  uint32_t max_fs;
  // MaxDpbMbs, Max. decoded picture buffer size (MBs)
  uint32_t max_dpb_mbs;
  // MaxBR, Max. video bitrate for Baseline and Main Profiles (kbit/s)
  uint32_t max_main_br;
};

LevelLimits LevelToLevelLimits(uint8_t level) {
  // See table A-1 in spec
  // { MaxMBPS, MaxFS, MaxDpbMbs, MaxBR}
  switch (level) {
    case H264SPS::kLevelIDC1p0:
      return {1485, 99, 396, 64};  // Level 1.0
    case H264SPS::kLevelIDC1B:
      return {1485, 99, 396, 128};  // Level 1b
    case H264SPS::kLevelIDC1p1:
      return {3000, 396, 900, 192};  // Level 1.1
    case H264SPS::kLevelIDC1p2:
      return {6000, 396, 2376, 384};  // Level 1.2
    case H264SPS::kLevelIDC1p3:
      return {11800, 396, 2376, 768};  // Level 1.3
    case H264SPS::kLevelIDC2p0:
      return {11880, 396, 2376, 2000};  // Level 2.0
    case H264SPS::kLevelIDC2p1:
      return {19800, 792, 4752, 4000};  // Level 2.1
    case H264SPS::kLevelIDC2p2:
      return {20250, 1620, 8100, 4000};  // Level 2.2
    case H264SPS::kLevelIDC3p0:
      return {40500, 1620, 8100, 10000};  // Level 3.0
    case H264SPS::kLevelIDC3p1:
      return {108000, 3600, 18000, 14000};  // Level 3.1
    case H264SPS::kLevelIDC3p2:
      return {216000, 5120, 20480, 20000};  // Level 3.2
    case H264SPS::kLevelIDC4p0:
      return {245760, 8192, 32768, 20000};  // Level 4.0
    case H264SPS::kLevelIDC4p1:
      return {245760, 8192, 32768, 50000};  // Level 4.1
    case H264SPS::kLevelIDC4p2:
      return {522240, 8704, 34816, 50000};  // Level 4.2
    case H264SPS::kLevelIDC5p0:
      return {589824, 22080, 110400, 135000};  // Level 5.0
    case H264SPS::kLevelIDC5p1:
      return {983040, 36864, 184320, 240000};  // Level 5.1
    case H264SPS::kLevelIDC5p2:
      return {2073600, 36864, 184320, 240000};  // Level 5.2
    case H264SPS::kLevelIDC6p0:
      return {4177920, 139264, 696320, 240000};  // Level 6.0
    case H264SPS::kLevelIDC6p1:
      return {8355840, 139264, 696320, 480000};  // Level 6.1
    case H264SPS::kLevelIDC6p2:
      return {16711680, 139264, 696320, 800000};  // Level 6.2
    default:
      DVLOG(1) << "Invalid codec level (" << static_cast<int>(level) << ")";
      return {0, 0, 0, 0};
  }
}
}  // namespace

uint32_t H264LevelToMaxMBPS(uint8_t level) {
  return LevelToLevelLimits(level).max_mbps;
}

uint32_t H264LevelToMaxFS(uint8_t level) {
  return LevelToLevelLimits(level).max_fs;
}

uint32_t H264LevelToMaxDpbMbs(uint8_t level) {
  return LevelToLevelLimits(level).max_dpb_mbs;
}

uint32_t H264ProfileLevelToMaxBR(VideoCodecProfile profile, uint8_t level) {
  uint32_t max_main_br = LevelToLevelLimits(level).max_main_br;

  // See table A-2 in spec
  // The maximum bit rate for High Profile is 1.25 times that of the
  // Base/Extended/Main Profiles, 3 times for Hi10P, and 4 times for
  // Hi422P/Hi444PP.
  switch (profile) {
    case H264PROFILE_BASELINE:
    case H264PROFILE_MAIN:
    case H264PROFILE_EXTENDED:
      return max_main_br;
    case H264PROFILE_HIGH:
      return max_main_br * 5 / 4;
    case H264PROFILE_HIGH10PROFILE:
      return max_main_br * 3;
    case H264PROFILE_HIGH422PROFILE:
    case H264PROFILE_HIGH444PREDICTIVEPROFILE:
      return max_main_br * 4;
    default:
      DVLOG(1) << "Failed to query MaxBR for profile: "
               << GetProfileName(profile);
      return 0;
  }
}

bool CheckH264LevelLimits(VideoCodecProfile profile,
                          uint8_t level,
                          uint32_t bitrate,
                          uint32_t framerate,
                          uint32_t framesize_in_mbs) {
  uint32_t max_bitrate_kbs = H264ProfileLevelToMaxBR(profile, level);
  DCHECK(base::IsValueInRangeForNumericType<uint32_t>(max_bitrate_kbs * 1000));

  uint32_t max_bitrate = max_bitrate_kbs * 1000;
  if (bitrate > max_bitrate) {
    DVLOG(1) << "Target bitrate: " << bitrate << " exceeds Max: " << max_bitrate
             << " bit/s";
    return false;
  }

  if (framesize_in_mbs > H264LevelToMaxFS(level)) {
    DVLOG(1) << "Target frame size: " << framesize_in_mbs
             << " exceeds Max: " << H264LevelToMaxFS(level) << " Macroblocks";
    return false;
  }

  uint32_t mbps = framesize_in_mbs * framerate;
  if (mbps > H264LevelToMaxMBPS(level)) {
    DVLOG(1) << "Target macroblock processing rate: " << mbps
             << " exceeds Max: " << H264LevelToMaxMBPS(level) << "Macroblock/s";
    return false;
  }

  return true;
}

std::optional<uint8_t> FindValidH264Level(VideoCodecProfile profile,
                                          uint32_t bitrate,
                                          uint32_t framerate,
                                          uint32_t framesize_in_mbs) {
  constexpr uint8_t kH264Levels[] = {
      H264SPS::kLevelIDC1p0, H264SPS::kLevelIDC1B,  H264SPS::kLevelIDC1p1,
      H264SPS::kLevelIDC1p2, H264SPS::kLevelIDC1p3, H264SPS::kLevelIDC2p0,
      H264SPS::kLevelIDC2p1, H264SPS::kLevelIDC2p2, H264SPS::kLevelIDC3p0,
      H264SPS::kLevelIDC3p1, H264SPS::kLevelIDC3p2, H264SPS::kLevelIDC4p0,
      H264SPS::kLevelIDC4p1, H264SPS::kLevelIDC4p2, H264SPS::kLevelIDC5p0,
      H264SPS::kLevelIDC5p1, H264SPS::kLevelIDC5p2, H264SPS::kLevelIDC6p0,
      H264SPS::kLevelIDC6p1, H264SPS::kLevelIDC6p2,
  };

  for (const uint8_t level : kH264Levels) {
    if (CheckH264LevelLimits(profile, level, bitrate, framerate,
                             framesize_in_mbs)) {
      return level;
    }
  }
  return std::nullopt;
}

}  // namespace media
