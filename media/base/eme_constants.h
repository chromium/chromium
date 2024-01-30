// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_EME_CONSTANTS_H_
#define MEDIA_BASE_EME_CONSTANTS_H_

#include <stdint.h>

#include <optional>

#include "media/base/media_export.h"
#include "media/media_buildflags.h"

namespace media {

// Defines values that specify registered Initialization Data Types used
// in Encrypted Media Extensions (EME).
// http://w3c.github.io/encrypted-media/initdata-format-registry.html#registry
enum class EmeInitDataType { UNKNOWN, WEBM, CENC, KEYIDS, MAX = KEYIDS };

// Defines bitmask values that specify codecs used in Encrypted Media Extensions
// (EME). Generally codec profiles are not specified and it is assumed that the
// profile support for encrypted playback is the same as for clear playback.
// For VP9 we have older CDMs only supporting profile 0, while new CDMs could
// support profile 2. Profile 1 and 3 are not supported by EME, see
// https://crbug.com/898298.
enum EmeCodec : uint32_t {
  EME_CODEC_NONE = 0,
  EME_CODEC_OPUS = 1 << 0,
  EME_CODEC_VORBIS = 1 << 1,
  EME_CODEC_VP8 = 1 << 2,
  EME_CODEC_VP9_PROFILE0 = 1 << 3,
  EME_CODEC_AAC = 1 << 4,
  EME_CODEC_AVC1 = 1 << 5,
  EME_CODEC_VP9_PROFILE2 = 1 << 6,  // VP9 profiles 2
  EME_CODEC_HEVC_PROFILE_MAIN = 1 << 7,
  EME_CODEC_DOLBY_VISION_PROFILE0 = 1 << 8,
  EME_CODEC_DOLBY_VISION_PROFILE5 = 1 << 9,
  EME_CODEC_DOLBY_VISION_PROFILE7 = 1 << 10,
  EME_CODEC_DOLBY_VISION_PROFILE8 = 1 << 11,
  EME_CODEC_DOLBY_VISION_PROFILE9 = 1 << 12,
  EME_CODEC_AC3 = 1 << 13,
  EME_CODEC_EAC3 = 1 << 14,
  EME_CODEC_MPEG_H_AUDIO = 1 << 15,
  EME_CODEC_FLAC = 1 << 16,
  EME_CODEC_AV1 = 1 << 17,
  EME_CODEC_HEVC_PROFILE_MAIN10 = 1 << 18,
  EME_CODEC_DTS = 1 << 19,
  EME_CODEC_DTSXP2 = 1 << 20,
  EME_CODEC_DTSE = 1 << 21,
  EME_CODEC_AC4 = 1 << 22,
  EME_CODEC_IAMF = 1 << 23,
};

// *_ALL values should only be used for masking, do not use them to specify
// codec support because they may be extended to include more codecs.

using SupportedCodecs = uint32_t;

// Dolby Vision profile 0 and 9 are based on AVC while profile 4, 5, 7 and 8 are
// based on HEVC.
constexpr SupportedCodecs EME_CODEC_DOLBY_VISION_AVC =
    EME_CODEC_DOLBY_VISION_PROFILE0 | EME_CODEC_DOLBY_VISION_PROFILE9;
constexpr SupportedCodecs EME_CODEC_DOLBY_VISION_HEVC =
    EME_CODEC_DOLBY_VISION_PROFILE5 | EME_CODEC_DOLBY_VISION_PROFILE7 |
    EME_CODEC_DOLBY_VISION_PROFILE8;

namespace {

constexpr SupportedCodecs GetMp4AudioCodecs() {
  SupportedCodecs codecs = EME_CODEC_OPUS | EME_CODEC_FLAC;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  codecs |= EME_CODEC_AAC;
#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
  codecs |= EME_CODEC_AC3 | EME_CODEC_EAC3;
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
#if BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
  codecs |= EME_CODEC_AC4;
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
#if BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
  codecs |= EME_CODEC_DTS | EME_CODEC_DTSXP2 | EME_CODEC_DTSE;
#endif  // BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
#if BUILDFLAG(ENABLE_PLATFORM_MPEG_H_AUDIO)
  codecs |= EME_CODEC_MPEG_H_AUDIO;
#endif  // BUILDFLAG(ENABLE_PLATFORM_MPEG_H_AUDIO)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
#if BUILDFLAG(ENABLE_PLATFORM_IAMF_AUDIO)
  codecs |= EME_CODEC_IAMF;
#endif  // BUILDFLAG(ENABLE_PLATFORM_IAMF_AUDIO)
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
  codecs |= EME_CODEC_HEVC_PROFILE_MAIN;
  codecs |= EME_CODEC_HEVC_PROFILE_MAIN10;
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

enum class EmeConfigRuleState {
  // To correctly identify the EmeConfig as Supported, we use the enum value
  // kUnset for each of the rules so that it is easy to check for, and cannot be
  // confused.
  kUnset,

  // Not Allowed represents when the rule in the collection of EmeConfigRules is
  // not allowed by the current system.
  kNotAllowed,

  // Recommended represents when the rule in the collection of EmeConfigRules is
  // recommended by the current system. In our design, the recommended takes a
  // second priority and cannot override the NotAllowed or Required value.
  kRecommended,

  // Required represents when the rule in the collection of EmeConfigRules is
  // required by the current system.
  kRequired,
};

struct MEDIA_EXPORT EmeConfig {
  using Rule = std::optional<EmeConfig>;

  // Refer to the EME spec for definitions on what identifier, persistence, and
  // hw_secure_codecs represent.
  EmeConfigRuleState identifier = EmeConfigRuleState::kUnset;
  EmeConfigRuleState persistence = EmeConfigRuleState::kUnset;
  EmeConfigRuleState hw_secure_codecs = EmeConfigRuleState::kUnset;

  // To represent an EmeConfig::Rule where the feature is supported without any
  // special requirements. This type adds nothing during the AddRule() function.
  // Internally, we represent Supported as all the States set to kUnset.
  static EmeConfig::Rule SupportedRule() { return EmeConfig(); }

  // To represent an EmeConfig::Rule where the feature is not supported.
  // Internally, we represent Unsupported as std::nullopt.
  static EmeConfig::Rule UnsupportedRule() { return std::nullopt; }
};

inline bool operator==(EmeConfig const& lhs, EmeConfig const& rhs) {
  return lhs.persistence == rhs.persistence &&
         lhs.identifier == rhs.identifier &&
         lhs.hw_secure_codecs == rhs.hw_secure_codecs;
}

}  // namespace media

#endif  // MEDIA_BASE_EME_CONSTANTS_H_
