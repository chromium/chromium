// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_PARSERS_H264_LEVEL_LIMITS_H_
#define MEDIA_PARSERS_H264_LEVEL_LIMITS_H_

#include <stddef.h>

#include <optional>

#include "media/base/media_export.h"
#include "media/base/video_codecs.h"

namespace media {

// Get max macroblock processing rate in macroblocks per second (MaxMBPS) from
// level. The abbreviation is as per spec table A-1.
uint32_t MEDIA_EXPORT H264LevelToMaxMBPS(uint8_t level);

// Get max frame size in macroblocks (MaxFS) from level. The abbreviation is as
// per spec table A-1.
uint32_t MEDIA_EXPORT H264LevelToMaxFS(uint8_t level);

// Get max decoded picture buffer size in macroblocks (MaxDpbMbs) from level.
// The abbreviation is as per spec table A-1.
uint32_t MEDIA_EXPORT H264LevelToMaxDpbMbs(uint8_t level);

// Get max video bitrate in kbit per second (MaxBR) from profile and level. The
// abbreviation is as per spec table A-1.
uint32_t MEDIA_EXPORT H264ProfileLevelToMaxBR(VideoCodecProfile profile,
                                              uint8_t level);

// Check whether |bitrate|, |framerate|, and |framesize_in_mbs| are valid from
// the limits of |profile| and |level| from Table A-1 in spec.
bool MEDIA_EXPORT CheckH264LevelLimits(VideoCodecProfile profile,
                                       uint8_t level,
                                       uint32_t bitrate,
                                       uint32_t framerate,
                                       uint32_t framesize_in_mbs);

// Return a minimum level that comforts Table A-1 in spec with |profile|,
// |bitrate|, |framerate| and |framesize_in_mbs|. If there is no proper level,
// returns std::nullopt.
std::optional<uint8_t> MEDIA_EXPORT
FindValidH264Level(VideoCodecProfile profile,
                   uint32_t bitrate,
                   uint32_t framerate,
                   uint32_t framesize_in_mbs);
}  // namespace media

#endif  // MEDIA_PARSERS_H264_LEVEL_LIMITS_H_
