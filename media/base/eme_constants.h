// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_EME_CONSTANTS_H_
#define MEDIA_BASE_EME_CONSTANTS_H_

#include <stdint.h>

#include "media/media_buildflags.h"

namespace media {

// Defines values that specify registered Initialization Data Types used
// in Encrypted Media Extensions (EME).
// http://w3c.github.io/encrypted-media/initdata-format-registry.html#registry
enum class EmeInitDataType { UNKNOWN, WEBM, CENC, KEYIDS, MAX = KEYIDS };

// Defines bitmask values that specify codecs used in Encrypted Media Extensions
// (EME). Generally codec profiles are not specified and it is assumed that the
// profile support for encrypted playback is the same as for clear playback.
// The only exception is VP9 where we have older CDMs only supporting profile 0,
// while new CDMs could support profile 2. Profile 1 and 3 are not supported by
// EME, see https://crbug.com/898298.
enum EmeCodec : uint32_t {
  EME_CODEC_NONE = 0,
  EME_CODEC_OPUS = 1 << 0,
  EME_CODEC_VORBIS = 1 << 1,
  EME_CODEC_VP8 = 1 << 2,
  EME_CODEC_VP9_PROFILE0 = 1 << 3,
  EME_CODEC_AAC = 1 << 4,
  EME_CODEC_AVC1 = 1 << 5,
  EME_CODEC_VP9_PROFILE2 = 1 << 6,  // VP9 profiles 2
  EME_CODEC_HEVC = 1 << 7,
  EME_CODEC_DOLBY_VISION_AVC = 1 << 8,
  EME_CODEC_DOLBY_VISION_HEVC = 1 << 9,
  EME_CODEC_AC3 = 1 << 10,
  EME_CODEC_EAC3 = 1 << 11,
  EME_CODEC_MPEG_H_AUDIO = 1 << 12,
  EME_CODEC_FLAC = 1 << 13,
  EME_CODEC_AV1 = 1 << 14,
};

// *_ALL values should only be used for masking, do not use them to specify
// codec support because they may be extended to include more codecs.

using SupportedCodecs = uint32_t;

namespace {

constexpr SupportedCodecs GetMp4AudioCodecs() {
  SupportedCodecs codecs = EME_CODEC_OPUS | EME_CODEC_FLAC;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  codecs |= EME_CODEC_AAC;
#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
  codecs |= EME_CODEC_AC3 | EME_CODEC_EAC3;
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
#if BUILDFLAG(ENABLE_PLATFORM_MPEG_H_AUDIO)
  codecs |= EME_CODEC_MPEG_H_AUDIO;
#endif  // BUILDFLAG(ENABLE_PLATFORM_MPEG_H_AUDIO)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
  return codecs;
}

constexpr SupportedCodecs GetMp4VideoCodecs() {
  // VP9 codec can be in MP4. Legacy VP9 codec strings ("vp9" and "vp9.0") can
  // not be in "video/mp4" mime type, but that is enforced by media::MimeUtil.
  SupportedCodecs codecs = EME_CODEC_VP9_PROFILE0 | EME_CODEC_VP9_PROFILE2;
  codecs |= EME_CODEC_AV1;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  codecs |= EME_CODEC_AVC1;
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  codecs |= EME_CODEC_HEVC;
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
#if BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
  codecs |= EME_CODEC_DOLBY_VISION_AVC;
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  codecs |= EME_CODEC_DOLBY_VISION_HEVC;
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
#endif  // BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
  return codecs;
}

}  // namespace

constexpr SupportedCodecs EME_CODEC_WEBM_AUDIO_ALL =
    EME_CODEC_OPUS | EME_CODEC_VORBIS;

constexpr SupportedCodecs EME_CODEC_WEBM_VIDEO_ALL =
    EME_CODEC_VP8 | EME_CODEC_VP9_PROFILE0 | EME_CODEC_VP9_PROFILE2 |
    EME_CODEC_AV1;

constexpr SupportedCodecs EME_CODEC_WEBM_ALL =
    EME_CODEC_WEBM_AUDIO_ALL | EME_CODEC_WEBM_VIDEO_ALL;

constexpr SupportedCodecs EME_CODEC_MP4_AUDIO_ALL = GetMp4AudioCodecs();
constexpr SupportedCodecs EME_CODEC_MP4_VIDEO_ALL = GetMp4VideoCodecs();

constexpr SupportedCodecs EME_CODEC_MP4_ALL =
    EME_CODEC_MP4_AUDIO_ALL | EME_CODEC_MP4_VIDEO_ALL;

constexpr SupportedCodecs EME_CODEC_AUDIO_ALL =
    EME_CODEC_WEBM_AUDIO_ALL | EME_CODEC_MP4_AUDIO_ALL;

constexpr SupportedCodecs EME_CODEC_VIDEO_ALL =
    EME_CODEC_WEBM_VIDEO_ALL | EME_CODEC_MP4_VIDEO_ALL;

constexpr SupportedCodecs EME_CODEC_ALL =
    EME_CODEC_WEBM_ALL | EME_CODEC_MP4_ALL;

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#if BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)
constexpr SupportedCodecs EME_CODEC_MP2T_VIDEO_ALL = EME_CODEC_AVC1;
static_assert(
    (EME_CODEC_MP2T_VIDEO_ALL & EME_CODEC_VIDEO_ALL) ==
        EME_CODEC_MP2T_VIDEO_ALL,
    "EME_CODEC_MP2T_VIDEO_ALL should be a subset of EME_CODEC_MP4_ALL");
#endif  // BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

enum class EmeSessionTypeSupport {
  // Invalid default value.
  INVALID,
  // The session type is not supported.
  NOT_SUPPORTED,
  // The session type is supported if a distinctive identifier is available.
  SUPPORTED_WITH_IDENTIFIER,
  // The session type is always supported.
  SUPPORTED,
};

// Used to declare support for distinctive identifier and persistent state.
// These are purposefully limited to not allow one to require the other, so that
// transitive requirements are not possible. Non-trivial refactoring would be
// required to support transitive requirements.
enum class EmeFeatureSupport {
  // Invalid default value.
  INVALID,
  // Access to the feature is not supported at all.
  NOT_SUPPORTED,
  // Access to the feature may be requested.
  REQUESTABLE,
  // Access to the feature cannot be blocked.
  ALWAYS_ENABLED,
};

enum class EmeMediaType {
  AUDIO,
  VIDEO,
};

// Configuration rules indicate the configuration state required to support a
// configuration option (note: a configuration option may be disallowing a
// feature). Configuration rules are used to answer queries about distinctive
// identifier, persistent state, and robustness requirements, as well as to
// describe support for different session types.
//
// If in the future there are reasons to request user permission other than
// access to a distinctive identifier, then additional rules should be added.
// Rules are implemented in ConfigState and are otherwise opaque.
enum class EmeConfigRule {
  // The configuration option is not supported.
  NOT_SUPPORTED,

  // The configuration option prevents use of a distinctive identifier.
  IDENTIFIER_NOT_ALLOWED,

  // The configuration option is supported if a distinctive identifier is
  // available.
  IDENTIFIER_REQUIRED,

  // The configuration option is supported, but the user experience may be
  // improved if a distinctive identifier is available.
  IDENTIFIER_RECOMMENDED,

  // The configuration option prevents use of persistent state.
  PERSISTENCE_NOT_ALLOWED,

  // The configuration option is supported if persistent state is available.
  PERSISTENCE_REQUIRED,

  // The configuration option is supported if both a distinctive identifier and
  // persistent state are available.
  IDENTIFIER_AND_PERSISTENCE_REQUIRED,

  // The configuration option prevents use of hardware-secure codecs.
  // This rule only has meaning on platforms that distinguish hardware-secure
  // codecs (i.e. Android and Windows).
  HW_SECURE_CODECS_NOT_ALLOWED,

  // The configuration option is supported if hardware-secure codecs are used.
  // This rule only has meaning on platforms that distinguish hardware-secure
  // codecs (i.e. Android and Windows).
  HW_SECURE_CODECS_REQUIRED,

  // The configuration option is supported without conditions.
  SUPPORTED,
};

}  // namespace media

#endif  // MEDIA_BASE_EME_CONSTANTS_H_
