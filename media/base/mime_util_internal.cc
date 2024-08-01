// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mime_util_internal.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "media/base/media.h"
#include "media/base/media_client.h"
#include "media/base/media_switches.h"
#include "media/base/supported_types.h"
#include "media/base/video_codec_string_parsers.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"

// TODO(dalecurtis): This include is not allowed by media/base since
// media/base/android is technically a different component. We should move
// mime_util*.{cc,h} out of media/base to fix this.
#include "media/base/android/media_codec_util.h"  // nogncheck
#endif

namespace media::internal {

// A map from codec string to MimeUtil::Codec.
using StringToCodecMap = base::flat_map<std::string, MimeUtil::Codec>;

// Wrapped to avoid static initializer startup cost.
const StringToCodecMap& GetStringToCodecMap() {
  static const base::NoDestructor<StringToCodecMap> kStringToCodecMap({
      // We only allow this for WAV so it isn't ambiguous.
      {"1", MimeUtil::PCM},
      // avc1/avc3.XXXXXX may be unambiguous; handled by
      // ParseAVCCodecId(). hev1/hvc1.XXXXXX may be unambiguous;
      // handled by ParseHEVCCodecID(). vp9, vp9.0,
      // vp09.xx.xx.xx.xx.xx.xx.xx may be unambiguous; handled by
      // ParseVp9CodecID().
      {"mp3", MimeUtil::MP3},
      // Following is the list of RFC 6381 compliant audio codec
      // strings:
      //   mp4a.66     - MPEG-2 AAC MAIN
      //   mp4a.67     - MPEG-2 AAC LC
      //   mp4a.68     - MPEG-2 AAC SSR
      //   mp4a.69     - MPEG-2 extension to MPEG-1 (MP3)
      //   mp4a.6B     - MPEG-1 audio (MP3)
      //   mp4a.40.2   - MPEG-4 AAC LC
      //   mp4a.40.02  - MPEG-4 AAC LC (leading 0 in aud-oti for
      //                 compatibility)
      //   mp4a.40.5   - MPEG-4 HE-AAC v1 (AAC LC + SBR)
      //   mp4a.40.05  - MPEG-4 HE-AAC v1 (AAC LC + SBR) (leading 0
      //                 in aud-oti for compatibility)
      //   mp4a.40.29  - MPEG-4 HE-AAC v2 (AAC LC + SBR + PS)
      {"mp4a.66", MimeUtil::MPEG2_AAC},
      {"mp4a.67", MimeUtil::MPEG2_AAC},
      {"mp4a.68", MimeUtil::MPEG2_AAC},
      {"mp4a.69", MimeUtil::MP3},
      {"mp4a.6B", MimeUtil::MP3},
      {"mp4a.40.2", MimeUtil::MPEG4_AAC},
      {"mp4a.40.02", MimeUtil::MPEG4_AAC},
      {"mp4a.40.5", MimeUtil::MPEG4_AAC},
      {"mp4a.40.05", MimeUtil::MPEG4_AAC},
      {"mp4a.40.29", MimeUtil::MPEG4_AAC},
      {"mp4a.40.42", MimeUtil::MPEG4_XHE_AAC},
      // TODO(servolk): Strictly speaking only mp4a.A5 and mp4a.A6
      // codec ids are valid according to RFC 6381 section 3.3, 3.4.
      // Lower-case oti (mp4a.a5 and mp4a.a6) should be rejected. But
      // we used to allow those in older versions of Chromecast
      // firmware and some apps (notably MPL) depend on those codec
      // types being supported, so they should be allowed for now
      // (crbug.com/564960).
      {"ac-3", MimeUtil::AC3},
      {"mp4a.a5", MimeUtil::AC3},
      {"mp4a.A5", MimeUtil::AC3},
      {"ec-3", MimeUtil::EAC3},
      {"mp4a.a6", MimeUtil::EAC3},
      {"mp4a.A6", MimeUtil::EAC3},
      {"vorbis", MimeUtil::VORBIS},
      {"opus", MimeUtil::OPUS},
      {"Opus", MimeUtil::OPUS},
      {"flac", MimeUtil::FLAC},
      {"fLaC", MimeUtil::FLAC},
      {"vp8", MimeUtil::VP8},
      {"vp8.0", MimeUtil::VP8},
      {"theora", MimeUtil::THEORA},
      {"dtsc", MimeUtil::DTS},
      {"mp4a.a9", MimeUtil::DTS},
      {"mp4a.A9", MimeUtil::DTS},
      {"dtse", MimeUtil::DTSE},
      {"mp4a.ac", MimeUtil::DTSE},
      {"mp4a.AC", MimeUtil::DTSE},
      {"dtsx", MimeUtil::DTSXP2},
      {"mp4a.b2", MimeUtil::DTSXP2},
      {"mp4a.B2", MimeUtil::DTSXP2},
      {"ac-4", MimeUtil::AC4},
      {"mp4a.ae", MimeUtil::AC4},
      {"mp4a.AE", MimeUtil::AC4},
  });

  return *kStringToCodecMap;
}

static std::optional<VideoType> ParseVp9CodecID(
    std::string_view mime_type_lower_case,
    std::string_view codec_id) {
  if (auto result = ParseNewStyleVp9CodecID(codec_id)) {
    // New style (e.g. vp09.00.10.08) is accepted with any mime type (including
    // empty mime type).
    return result;
  }

  // Legacy style (e.g. "vp9") is ambiguous about codec profile, and is only
  // valid with video/webm for legacy reasons.
  if (mime_type_lower_case == "video/webm") {
    return ParseLegacyVp9CodecID(codec_id);
  }

  return std::nullopt;
}

static bool IsValidH264Level(uint8_t level_idc) {
  // Valid levels taken from Table A-1 in ISO/IEC 14496-10.
  // Level_idc represents the standard level represented as decimal number
  // multiplied by ten, e.g. level_idc==32 corresponds to level==3.2
  return ((level_idc >= 10 && level_idc <= 13) ||
          (level_idc >= 20 && level_idc <= 22) ||
          (level_idc >= 30 && level_idc <= 32) ||
          (level_idc >= 40 && level_idc <= 42) ||
          (level_idc >= 50 && level_idc <= 52) ||
          (level_idc >= 60 && level_idc <= 62));
}

// Make a default ParsedCodecResult. Values should indicate "unspecified"
// where possible. Color space is an exception where we choose a default value
// because most codec strings will not describe a color space.
static MimeUtil::ParsedCodecResult MakeDefaultParsedCodecResult() {
  return {.codec = MimeUtil::INVALID_CODEC, .is_ambiguous = false};
}

MimeUtil::MimeUtil() {
#if BUILDFLAG(IS_ANDROID)
  platform_info_.has_platform_vp8_decoder =
      MediaCodecUtil::IsVp8DecoderAvailable();
  platform_info_.has_platform_vp9_decoder =
      MediaCodecUtil::IsVp9DecoderAvailable();
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  platform_info_.has_platform_hevc_decoder =
      MediaCodecUtil::IsHEVCDecoderAvailable();
#endif
  platform_info_.has_platform_opus_decoder =
      MediaCodecUtil::IsOpusDecoderAvailable();
#endif  // BUILDFLAG(IS_ANDROID)

  InitializeMimeTypeMaps();
}

MimeUtil::~MimeUtil() = default;

AudioCodec MimeUtilToAudioCodec(MimeUtil::Codec codec) {
  switch (codec) {
    case MimeUtil::PCM:
      return AudioCodec::kPCM;
    case MimeUtil::MP3:
      return AudioCodec::kMP3;
    case MimeUtil::AC3:
      return AudioCodec::kAC3;
    case MimeUtil::EAC3:
      return AudioCodec::kEAC3;
    case MimeUtil::MPEG2_AAC:
    case MimeUtil::MPEG4_AAC:
    case MimeUtil::MPEG4_XHE_AAC:
      return AudioCodec::kAAC;
    case MimeUtil::MPEG_H_AUDIO:
      return AudioCodec::kMpegHAudio;
    case MimeUtil::VORBIS:
      return AudioCodec::kVorbis;
    case MimeUtil::OPUS:
      return AudioCodec::kOpus;
    case MimeUtil::FLAC:
      return AudioCodec::kFLAC;
    case MimeUtil::DTS:
      return AudioCodec::kDTS;
    case MimeUtil::DTSXP2:
      return AudioCodec::kDTSXP2;
    case MimeUtil::DTSE:
      return AudioCodec::kDTSE;
    case MimeUtil::AC4:
      return AudioCodec::kAC4;
    case MimeUtil::IAMF:
      return AudioCodec::kIAMF;
    default:
      break;
  }
  return AudioCodec::kUnknown;
}

VideoCodec MimeUtilToVideoCodec(MimeUtil::Codec codec) {
  switch (codec) {
    case MimeUtil::AV1:
      return VideoCodec::kAV1;
    case MimeUtil::H264:
      return VideoCodec::kH264;
    case MimeUtil::HEVC:
      return VideoCodec::kHEVC;
    case MimeUtil::VP8:
      return VideoCodec::kVP8;
    case MimeUtil::VP9:
      return VideoCodec::kVP9;
    case MimeUtil::THEORA:
      return VideoCodec::kTheora;
    case MimeUtil::DOLBY_VISION:
      return VideoCodec::kDolbyVision;
    default:
      break;
  }
  return VideoCodec::kUnknown;
}

SupportsType MimeUtil::AreSupportedCodecs(
    const std::vector<ParsedCodecResult>& parsed_codecs,
    std::string_view mime_type_lower_case,
    bool is_encrypted) const {
  DCHECK(!parsed_codecs.empty());
  DCHECK_EQ(base::ToLowerASCII(mime_type_lower_case), mime_type_lower_case);

  SupportsType combined_result = SupportsType::kSupported;

  for (const auto& parsed_codec : parsed_codecs) {
    // Make conservative guesses to resolve ambiguity before checking platform
    // support. Historically we allowed some ambiguity in H264 and VP9 codec
    // strings, so we must continue to allow going forward. DO NOT ADD NEW
    // SUPPORT FOR MORE AMBIGUOUS STRINGS.
    VideoCodecProfile video_profile = VIDEO_CODEC_PROFILE_UNKNOWN;
    VideoCodecLevel video_level = kNoVideoCodecLevel;
    VideoColorSpace video_color_space;
    if (parsed_codec.video) {
      video_profile = parsed_codec.video->profile;
      video_level = parsed_codec.video->level;
      video_color_space = parsed_codec.video->color_space;
    }
    if (parsed_codec.is_ambiguous) {
      switch (parsed_codec.codec) {
        case MimeUtil::H264:
          if (video_profile == VIDEO_CODEC_PROFILE_UNKNOWN)
            video_profile = H264PROFILE_BASELINE;
          if (!IsValidH264Level(video_level))
            video_level = 10;
          break;
        case MimeUtil::VP9:
          if (video_profile == VIDEO_CODEC_PROFILE_UNKNOWN)
            video_profile = VP9PROFILE_PROFILE0;
          if (video_level == 0)
            video_level = 10;
          break;
        case MimeUtil::MPEG4_AAC:
          // Nothing to do for AAC; no notion of profile / level to guess.
          break;
        default:
          NOTREACHED_IN_MIGRATION()
              << "Only VP9, H264, and AAC codec strings can be ambiguous.";
      }
    }

    // Check platform support.
    SupportsType result = IsCodecSupported(
        mime_type_lower_case, parsed_codec.codec, video_profile, video_level,
        video_color_space, is_encrypted);
    if (result == SupportsType::kNotSupported) {
      DVLOG(2) << __func__ << ": Codec " << parsed_codec.codec
               << " not supported by platform.";
      return SupportsType::kNotSupported;
    }

    // If any codec is "kMaybeSupported", return Maybe for the combined result.
    if (result == SupportsType::kMaybeSupported ||
        // Downgrade to kMaybeSupported if we had to guess the meaning of one of
        // the codec strings. Do not downgrade for VP9 because we historically
        // returned "Probably" for the old "vp9" string and cannot change to
        // returning "Maybe" as this will break sites.
        (result == SupportsType::kSupported && parsed_codec.is_ambiguous &&
         parsed_codec.codec != MimeUtil::VP9)) {
      combined_result = SupportsType::kMaybeSupported;
    }
  }

  return combined_result;
}

void MimeUtil::InitializeMimeTypeMaps() {
  AddSupportedMediaFormats();
}

// Each call to AddContainerWithCodecs() contains a media type
// (https://en.wikipedia.org/wiki/Media_type) and corresponding media codec(s)
// supported by these types/containers.
void MimeUtil::AddSupportedMediaFormats() {
  const CodecSet wav_codecs{PCM};
  const CodecSet ogg_audio_codecs{FLAC, OPUS, VORBIS};

  CodecSet ogg_video_codecs{VP8};
  CodecSet ogg_codecs(ogg_audio_codecs);
  ogg_codecs.insert(ogg_video_codecs.begin(), ogg_video_codecs.end());

  const CodecSet webm_audio_codecs{OPUS, VORBIS};
  CodecSet webm_video_codecs{VP8, VP9};
#if BUILDFLAG(ENABLE_AV1_DECODER)
  webm_video_codecs.emplace(AV1);
#endif

  CodecSet webm_codecs(webm_audio_codecs);
  webm_codecs.insert(webm_video_codecs.begin(), webm_video_codecs.end());

  const CodecSet mp3_codecs{MP3};

  CodecSet mp4_audio_codecs{FLAC, MP3, OPUS};

  // Only VP9 with valid codec string vp09.xx.xx.xx.xx.xx.xx.xx is supported.
  // See ParseVp9CodecID for details.
  CodecSet mp4_video_codecs;
  mp4_video_codecs.emplace(VP9);

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  const CodecSet aac{MPEG2_AAC, MPEG4_AAC, MPEG4_XHE_AAC};
  mp4_audio_codecs.insert(aac.begin(), aac.end());

  CodecSet avc_and_aac(aac);
  avc_and_aac.emplace(H264);

#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
  mp4_audio_codecs.emplace(AC3);
  mp4_audio_codecs.emplace(EAC3);
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)

#if BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
  mp4_audio_codecs.emplace(AC4);
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)

#if BUILDFLAG(ENABLE_PLATFORM_MPEG_H_AUDIO)
  mp4_audio_codecs.emplace(MPEG_H_AUDIO);
#endif  // BUILDFLAG(ENABLE_PLATFORM_MPEG_H_AUDIO)

  mp4_video_codecs.emplace(H264);
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  mp4_video_codecs.emplace(HEVC);
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)

#if BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
  mp4_video_codecs.emplace(DOLBY_VISION);
#endif  // BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
#if BUILDFLAG(ENABLE_AV1_DECODER)
  mp4_video_codecs.emplace(AV1);
#endif

#if BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
  mp4_audio_codecs.emplace(DTS);
  mp4_audio_codecs.emplace(DTSXP2);
  mp4_audio_codecs.emplace(DTSE);
#endif  // BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)

#if BUILDFLAG(ENABLE_PLATFORM_IAMF_AUDIO)
  mp4_audio_codecs.emplace(IAMF);
#endif  // BUILDFLAG(ENABLE_PLATFORM_IAMF_AUDIO)

  CodecSet mp4_codecs(mp4_audio_codecs);
  mp4_codecs.insert(mp4_video_codecs.begin(), mp4_video_codecs.end());

  const CodecSet implicit_codec;
  AddContainerWithCodecs("audio/wav", wav_codecs);
  AddContainerWithCodecs("audio/x-wav", wav_codecs);
  AddContainerWithCodecs("audio/webm", webm_audio_codecs);
  DCHECK(!webm_video_codecs.empty());
  AddContainerWithCodecs("video/webm", webm_codecs);
  AddContainerWithCodecs("audio/ogg", ogg_audio_codecs);
  // video/ogg is only supported if an appropriate video codec is supported.
  // Note: This assumes such codecs cannot be later excluded.
  if (!ogg_video_codecs.empty())
    AddContainerWithCodecs("video/ogg", ogg_codecs);
  // TODO(ddorwin): Should the application type support Opus?
  AddContainerWithCodecs("application/ogg", ogg_codecs);
  AddContainerWithCodecs("audio/flac", implicit_codec);
  AddContainerWithCodecs("audio/mpeg", mp3_codecs);  // Allow "mp3".
  AddContainerWithCodecs("audio/mp3", implicit_codec);
  AddContainerWithCodecs("audio/x-mp3", implicit_codec);
  AddContainerWithCodecs("audio/mp4", mp4_audio_codecs);
  DCHECK(!mp4_video_codecs.empty());
  AddContainerWithCodecs("video/mp4", mp4_codecs);

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  AddContainerWithCodecs("audio/aac", implicit_codec);  // AAC / ADTS.
  // These strings are supported for backwards compatibility only and thus only
  // support the codecs needed for compatibility.
  AddContainerWithCodecs("audio/x-m4a", aac);
  AddContainerWithCodecs("video/x-m4v", avc_and_aac);

  CodecSet video_3gpp_codecs(aac);
  video_3gpp_codecs.emplace(H264);
  AddContainerWithCodecs("video/3gpp", video_3gpp_codecs);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_HLS_DEMUXER)
  bool can_play_hls = false;
#endif
#if BUILDFLAG(IS_ANDROID)
  can_play_hls |= base::FeatureList::IsEnabled(kHlsPlayer);
#endif
#if BUILDFLAG(ENABLE_HLS_DEMUXER)
  can_play_hls |= base::FeatureList::IsEnabled(kBuiltInHlsPlayer);
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_HLS_DEMUXER)
  if (can_play_hls) {
    // HTTP Live Streaming (HLS).
    CodecSet hls_codecs{H264,
                        // TODO(ddorwin): Is any MP3 codec string variant
                        // included in real queries?
                        MP3,
                        // Android HLS only supports MPEG4_AAC (missing demuxer
                        // support for MPEG2_AAC)
                        MPEG4_AAC};
    AddContainerWithCodecs("application/x-mpegurl", hls_codecs);
    AddContainerWithCodecs("application/vnd.apple.mpegurl", hls_codecs);
    AddContainerWithCodecs("audio/mpegurl", hls_codecs);
    // Not documented by Apple, but unfortunately used extensively by Apple and
    // others for both audio-only and audio+video playlists. See
    // https://crbug.com/675552 for details and examples.
    AddContainerWithCodecs("audio/x-mpegurl", hls_codecs);
  }
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_HLS_DEMUXER)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
}

void MimeUtil::AddContainerWithCodecs(std::string mime_type, CodecSet codecs) {
  media_format_map_.insert_or_assign(std::move(mime_type), std::move(codecs));
}

bool MimeUtil::IsSupportedMediaMimeType(std::string_view mime_type) const {
  return media_format_map_.contains(base::ToLowerASCII(mime_type));
}

void MimeUtil::SplitCodecs(std::string_view codecs,
                           std::vector<std::string>* codecs_out) const {
  *codecs_out =
      base::SplitString(base::TrimString(codecs, "\"", base::TRIM_ALL), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  // Convert empty or all-whitespace input to 0 results.
  if (codecs_out->size() == 1 && (*codecs_out)[0].empty())
    codecs_out->clear();
}

void MimeUtil::StripCodecs(std::vector<std::string>* codecs) const {
  // Strip everything past the first '.'
  for (auto& codec : *codecs) {
    size_t found = codec.find_first_of('.');
    if (found != std::string::npos)
      codec.resize(found);
  }
}

std::optional<VideoType> MimeUtil::ParseVideoCodecString(
    std::string_view mime_type,
    std::string_view codec_id,
    bool allow_ambiguous_matches) const {
  // Internal parsing API expects a vector of codecs.
  std::vector<ParsedCodecResult> parsed_results;
  std::vector<std::string> codec_strings;
  if (!codec_id.empty())
    codec_strings.emplace_back(codec_id);

  if (!ParseCodecStrings(base::ToLowerASCII(mime_type), codec_strings,
                         &parsed_results)) {
    DVLOG(3) << __func__ << " Failed to parse mime/codec pair: "
             << (mime_type.empty() ? "<empty mime>" : mime_type) << "; "
             << codec_id;
    return std::nullopt;
  }

  CHECK_EQ(1U, parsed_results.size());

  if (!parsed_results[0].video) {
    DVLOG(3) << __func__ << " Codec string " << codec_id
             << " is not a VIDEO codec.";
    return std::nullopt;
  }

  if (!allow_ambiguous_matches && parsed_results[0].is_ambiguous) {
    DVLOG(3) << __func__ << " Refusing to return ambiguous codec string match.";
    return std::nullopt;
  }

  return parsed_results[0].video;
}

bool MimeUtil::ParseAudioCodecString(std::string_view mime_type,
                                     std::string_view codec_id,
                                     bool* out_is_ambiguous,
                                     AudioCodec* out_codec) const {
  DCHECK(out_is_ambiguous);
  DCHECK(out_codec);

  // Internal parsing API expects a vector of codecs.
  std::vector<ParsedCodecResult> parsed_results;
  std::vector<std::string> codec_strings;
  if (!codec_id.empty())
    codec_strings.emplace_back(codec_id);

  if (!ParseCodecStrings(base::ToLowerASCII(mime_type), codec_strings,
                         &parsed_results)) {
    DVLOG(3) << __func__ << " Failed to parse mime/codec pair:"
             << (mime_type.empty() ? "<empty mime>" : mime_type) << "; "
             << codec_id;
    return false;
  }

  CHECK_EQ(1U, parsed_results.size());
  *out_is_ambiguous = parsed_results[0].is_ambiguous;
  *out_codec = MimeUtilToAudioCodec(parsed_results[0].codec);

  if (*out_codec == AudioCodec::kUnknown) {
    DVLOG(3) << __func__ << " Codec string " << codec_id
             << " is not an AUDIO codec.";
    return false;
  }

  return true;
}

SupportsType MimeUtil::IsSupportedMediaFormat(
    std::string_view mime_type,
    const std::vector<std::string>& codecs,
    bool is_encrypted) const {
  const std::string mime_type_lower_case = base::ToLowerASCII(mime_type);
  std::vector<ParsedCodecResult> parsed_results;
  if (!ParseCodecStrings(mime_type_lower_case, codecs, &parsed_results)) {
    DVLOG(3) << __func__ << " Media format unsupported; codec parsing failed "
             << mime_type << " " << base::JoinString(codecs, ",");
    return SupportsType::kNotSupported;
  }

  if (parsed_results.empty()) {
    NOTREACHED_IN_MIGRATION()
        << __func__ << " Successful parsing should output results.";
    return SupportsType::kNotSupported;
  }

  // We get here if the mime type expects to get a codecs parameter
  // but none was provided and no default codec was implied. In this case
  // the best we can do is say "maybe" because we don't have enough
  // information.
  if (codecs.empty() && parsed_results.size() == 1 &&
      parsed_results[0].codec == INVALID_CODEC) {
    DCHECK(parsed_results[0].is_ambiguous);
    return SupportsType::kMaybeSupported;
  }

  return AreSupportedCodecs(parsed_results, mime_type_lower_case, is_encrypted);
}

// static
bool MimeUtil::IsCodecSupportedOnAndroid(Codec codec,
                                         std::string_view mime_type_lower_case,
                                         bool is_encrypted,
                                         VideoCodecProfile video_profile,
                                         const PlatformInfo& platform_info) {
  DVLOG(3) << __func__;
  DCHECK_NE(mime_type_lower_case, "");

  // NOTE: We do not account for Media Source Extensions (MSE) within these
  // checks since it has its own isTypeSupported() which will handle platform
  // specific codec rejections.  See http://crbug.com/587303.

  switch (codec) {
    // ----------------------------------------------------------------------
    // The following codecs are never supported.
    // ----------------------------------------------------------------------
    case INVALID_CODEC:
    case THEORA:
      return false;

    // ----------------------------------------------------------------------
    // The remaining codecs may be supported depending on platform abilities.
    // ----------------------------------------------------------------------
    case AV1:
      return BUILDFLAG(ENABLE_AV1_DECODER);

    case MPEG2_AAC:
      // MPEG2_AAC cannot be used in HLS (mpegurl suffix), but this is enforced
      // in the parsing step by excluding MPEG2_AAC from the list of
      // valid codecs to be used with HLS mime types.
      DCHECK(!base::EndsWith(mime_type_lower_case, "mpegurl",
                             base::CompareCase::SENSITIVE));
      [[fallthrough]];
    case PCM:
    case MP3:
    case MPEG4_AAC:
    case FLAC:
    case VORBIS:
      // These codecs are always supported; via a platform decoder (when used
      // with MSE/EME) or with a software decoder (the unified pipeline).
      return true;

    case MPEG4_XHE_AAC:
      return true;

    case MPEG_H_AUDIO:
      return false;

    case OPUS:
      // If clear, the unified pipeline can always decode Opus in software.
      if (!is_encrypted)
        return true;

      // Otherwise, platform support is required.
      if (!platform_info.has_platform_opus_decoder) {
        DVLOG(3) << "Platform does not support opus";
        return false;
      }

      return true;

    case H264:
      return true;

    case HEVC:
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
      return platform_info.has_platform_hevc_decoder;
#else
      return false;
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)

    case VP8:
      // If clear, the unified pipeline can always decode VP8 in software.
      return is_encrypted ? platform_info.has_platform_vp8_decoder : true;

    case VP9: {
      if (base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kReportVp9AsAnUnsupportedMimeType)) {
        return false;
      }

      // If clear, the unified pipeline can always decode VP9.0,1 in software.
      // If we don't know the profile, then support is ambiguous, but default to
      // true for historical reasons.
      if (!is_encrypted && (video_profile == VP9PROFILE_PROFILE0 ||
                            video_profile == VP9PROFILE_PROFILE1 ||
                            video_profile == VIDEO_CODEC_PROFILE_UNKNOWN)) {
        return true;
      }

      if (!platform_info.has_platform_vp9_decoder)
        return false;

      return true;
    }

    case DOLBY_VISION:
#if BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
      return true;
#else
      return false;
#endif

    case AC3:
    case EAC3:
#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
      return true;
#else
      return false;
#endif

    case DTS:
    case DTSXP2:
    case DTSE:
#if BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
      return true;
#else
      return false;
#endif

    case AC4:
    case IAMF:
      return false;
  }

  return false;
}

bool MimeUtil::ParseCodecStrings(
    std::string_view mime_type_lower_case,
    const std::vector<std::string>& codecs,
    std::vector<ParsedCodecResult>* out_results) const {
  DCHECK(out_results);

  // Nothing to parse.
  if (mime_type_lower_case.empty() && codecs.empty())
    return false;

  // When mime type is provided, it may imply a codec or only be valid with
  // certain codecs.
  const CodecSet* valid_codecs_for_mime;
  if (!mime_type_lower_case.empty()) {
    // Reject unrecognized mime types.
    auto it_media_format_map = media_format_map_.find(mime_type_lower_case);
    if (it_media_format_map == media_format_map_.end()) {
      DVLOG(3) << __func__
               << " Unrecognized mime type: " << mime_type_lower_case;
      return false;
    }

    valid_codecs_for_mime = &it_media_format_map->second;
    if (valid_codecs_for_mime->empty()) {
      // We get here if the mimetype does not expect a codecs parameter.
      if (!codecs.empty()) {
        DVLOG(3) << __func__
                 << " Codecs unexpected for mime type:" << mime_type_lower_case;
        return false;
      }

      // Determine implied codec for mime type.
      ParsedCodecResult implied_result = MakeDefaultParsedCodecResult();
      if (!GetDefaultCodec(mime_type_lower_case, &implied_result.codec)) {
        NOTREACHED_IN_MIGRATION()
            << " Mime types must offer a default codec if no explicit "
               "codecs are expected";
        return false;
      }
      out_results->push_back(implied_result);
      return true;
    }

    if (codecs.empty()) {
      // We get here if the mimetype expects to get a codecs parameter,
      // but didn't get one. If |mime_type_lower_case| does not have a default
      // codec, the string is considered ambiguous.
      ParsedCodecResult implied_result = MakeDefaultParsedCodecResult();
      implied_result.is_ambiguous =
          !GetDefaultCodec(mime_type_lower_case, &implied_result.codec);
      out_results->push_back(implied_result);
      return true;
    }
  }

  // All empty cases handled above.
  DCHECK(!codecs.empty());

  for (std::string codec_string : codecs) {
    ParsedCodecResult result;

    if (mime_type_lower_case == "video/mp2t")
      codec_string = TranslateLegacyAvc1CodecIds(codec_string);

    if (!ParseCodecHelper(mime_type_lower_case, codec_string, &result)) {
      DVLOG(3) << __func__ << " Failed to parse mime/codec pair: "
               << (mime_type_lower_case.empty() ? "<empty mime>"
                                                : mime_type_lower_case)
               << "; " << codec_string;
      return false;
    }
    DCHECK_NE(INVALID_CODEC, result.codec);

    // If mime type given, fail if mime + codec is not a valid combination.
    if (!mime_type_lower_case.empty() &&
        !valid_codecs_for_mime->contains(result.codec)) {
      DVLOG(3) << __func__
               << " Incompatible mime/codec pair: " << mime_type_lower_case
               << "; " << codec_string;
      return false;
    }

    out_results->push_back(result);
  }

  return true;
}

bool MimeUtil::ParseCodecHelper(std::string_view mime_type_lower_case,
                                std::string_view codec_id,
                                ParsedCodecResult* out_result) const {
  DCHECK_EQ(base::ToLowerASCII(mime_type_lower_case), mime_type_lower_case);
  DCHECK(out_result);

  *out_result = MakeDefaultParsedCodecResult();

  // We choose 709 as default color space elsewhere, so defaulting to 709
  // here as well. See here for context: https://crrev.com/1221903003/
  const auto kDefaultColorSpace = VideoColorSpace::REC709();

  // Simple codecs can be found in the codec map.
  auto itr = GetStringToCodecMap().find(codec_id);
  if (itr != GetStringToCodecMap().end()) {
    out_result->codec = itr->second;

    // Even "simple" video codecs should have an associated profile.
    switch (out_result->codec) {
      case Codec::VP8:
        out_result->video = {.codec = VideoCodec::kVP8,
                             .profile = VP8PROFILE_ANY,
                             .level = kNoVideoCodecLevel,
                             .color_space = kDefaultColorSpace};
        break;
      case Codec::THEORA:
        out_result->video = {.codec = VideoCodec::kTheora,
                             .profile = THEORAPROFILE_ANY,
                             .level = kNoVideoCodecLevel,
                             .color_space = kDefaultColorSpace};
        break;
      default:
        break;
    }

    return true;
  }

  // Check codec string against short list of allowed ambiguous codecs.
  // Hard-coded to discourage expansion. DO NOT ADD TO THIS LIST. DO NOT
  // INCREASE PLACES WHERE |ambiguous_codec_string| = true.
  // NOTE: avc1/avc3.XXXXXX may be ambiguous handled after ParseAVCCodecId().
  if (codec_id == "avc1" || codec_id == "avc3") {
    out_result->codec = MimeUtil::H264;
    out_result->is_ambiguous = true;
    out_result->video = {.codec = VideoCodec::kH264,
                         .profile = VIDEO_CODEC_PROFILE_UNKNOWN,
                         .level = kNoVideoCodecLevel,
                         .color_space = kDefaultColorSpace};
    return true;
  } else if (codec_id == "mp4a.40") {
    out_result->codec = MimeUtil::MPEG4_AAC;
    out_result->is_ambiguous = true;
    return true;
  }

  // If |codec_id| is not in |kStringToCodecMap|, then we assume that it is
  // either VP9, H.264 or HEVC/H.265 codec ID because currently those are the
  // only ones that are not added to the |kStringToCodecMap| and require
  // parsing.

  if (auto result = ParseVp9CodecID(mime_type_lower_case, codec_id)) {
    out_result->codec = MimeUtil::VP9;
    out_result->video = result;
    // Original VP9 codec string did not describe the profile.
    if (out_result->video->profile == VIDEO_CODEC_PROFILE_UNKNOWN) {
      // New VP9 string should never be ambiguous.
      DCHECK(!base::StartsWith(codec_id, "vp09", base::CompareCase::SENSITIVE));
      out_result->is_ambiguous = true;
      if (!out_result->video->color_space.IsSpecified()) {
        out_result->video->color_space = kDefaultColorSpace;
      }
    }
    return true;
  }

  if (auto result = ParseAv1CodecId(codec_id)) {
    out_result->codec = MimeUtil::AV1;
    out_result->video = result;
    DCHECK(out_result->video->color_space.IsSpecified());
    return true;
  }

  if (auto result = ParseAVCCodecId(codec_id)) {
    out_result->codec = MimeUtil::H264;
    out_result->video = result;
    // Allowed string ambiguity since 2014. DO NOT ADD NEW CASES FOR AMBIGUITY.
    out_result->is_ambiguous = !IsValidH264Level(out_result->video->level);

    DCHECK(!out_result->video->color_space.IsSpecified());
    out_result->video->color_space = kDefaultColorSpace;
    return true;
  }

  if (auto result = ParseHEVCCodecId(codec_id)) {
    out_result->codec = MimeUtil::HEVC;
    out_result->video = result;

    // HEVC has color space information, but we don't parse it.
    DCHECK(!out_result->video->color_space.IsSpecified());
    out_result->video->color_space = kDefaultColorSpace;
    return true;
  }

  if (auto result = ParseDolbyVisionCodecId(codec_id)) {
    out_result->codec = MimeUtil::DOLBY_VISION;
    out_result->video = result;

    // DV has color space information, but we don't parse it.
    DCHECK(!out_result->video->color_space.IsSpecified());
    out_result->video->color_space = kDefaultColorSpace;
    return true;
  }

// TODO(crbug.com/40145071): Remove buildflags for parsing functions; we
// shouldn't be combining codec support and parsing support.
#if BUILDFLAG(ENABLE_PLATFORM_MPEG_H_AUDIO)
  if (base::StartsWith(codec_id, "mhm1.", base::CompareCase::SENSITIVE) ||
      base::StartsWith(codec_id, "mha1.", base::CompareCase::SENSITIVE)) {
    out_result->codec = MimeUtil::MPEG_H_AUDIO;
    return true;
  }
#endif

#if BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
  if (ParseDolbyAc4CodecId(codec_id.data(), nullptr, nullptr, nullptr)) {
    out_result->codec = MimeUtil::AC4;
    return true;
  }
#endif

#if BUILDFLAG(ENABLE_PLATFORM_IAMF_AUDIO)
  if (ParseIamfCodecId(codec_id.data(), nullptr, nullptr)) {
    out_result->codec = MimeUtil::IAMF;
    return true;
  }
#endif

  DVLOG(2) << __func__ << ": Unrecognized codec id \"" << codec_id << "\"";
  return false;
}

SupportsType MimeUtil::IsCodecSupported(std::string_view mime_type_lower_case,
                                        Codec codec,
                                        VideoCodecProfile video_profile,
                                        VideoCodecLevel video_level,
                                        const VideoColorSpace& color_space,
                                        bool is_encrypted) const {
  DVLOG(3) << __func__;

  DCHECK_EQ(base::ToLowerASCII(mime_type_lower_case), mime_type_lower_case);
  DCHECK_NE(codec, INVALID_CODEC);

  VideoCodec video_codec = MimeUtilToVideoCodec(codec);
  if (video_codec != VideoCodec::kUnknown &&
      // Theora and VP8 do not have profiles/levels.
      video_codec != VideoCodec::kTheora && video_codec != VideoCodec::kVP8 &&
      // TODO(dalecurtis): AV1 has levels, but they aren't supported yet;
      // http://crbug.com/784993
      video_codec != VideoCodec::kAV1) {
    DCHECK_NE(video_profile, VIDEO_CODEC_PROFILE_UNKNOWN);
    DCHECK_GT(video_level, 0u);
  }

  // Check for cases of ambiguous platform support.
  // TODO(chcunningham): DELETE THIS. Platform should know its capabilities.
  // Answer should come from MediaClient.
  bool ambiguous_platform_support = false;
  if (codec == MimeUtil::H264) {
    switch (video_profile) {
      // Always supported
      case H264PROFILE_BASELINE:
      case H264PROFILE_MAIN:
      case H264PROFILE_HIGH:
        break;
      // Only supported on some hardware and via ffmpeg.
      case H264PROFILE_HIGH10PROFILE:
        if (IsBuiltInVideoCodec(VideoCodec::kH264)) {
          // FFmpeg is not generally used for encrypted videos, so we do not
          // know whether 10-bit is supported.
          ambiguous_platform_support = is_encrypted;
          break;
        }
        [[fallthrough]];
      default:
        ambiguous_platform_support = true;
    }
  }

  AudioCodec audio_codec = MimeUtilToAudioCodec(codec);
  if (audio_codec != AudioCodec::kUnknown) {
    AudioCodecProfile audio_profile = AudioCodecProfile::kUnknown;
    if (codec == MPEG4_XHE_AAC)
      audio_profile = AudioCodecProfile::kXHE_AAC;

    if (!IsSupportedAudioType({audio_codec, audio_profile, false}))
      return SupportsType::kNotSupported;
  }

  if (video_codec != VideoCodec::kUnknown) {
    if (!IsSupportedVideoType(
            {video_codec, video_profile, video_level, color_space})) {
      return SupportsType::kNotSupported;
    }
  }

#if BUILDFLAG(IS_ANDROID)
  // TODO(chcunningham): Delete this. Android platform support should be
  // handled by (android specific) media::IsSupportedVideoType() above.
  if (!IsCodecSupportedOnAndroid(codec, mime_type_lower_case, is_encrypted,
                                 video_profile, platform_info_)) {
    return SupportsType::kNotSupported;
  }
#endif

  return ambiguous_platform_support ? SupportsType::kMaybeSupported
                                    : SupportsType::kSupported;
}

bool MimeUtil::GetDefaultCodec(std::string_view mime_type,
                               Codec* default_codec) const {
  // Codecs below are unambiguously implied by the mime type string. DO NOT add
  // default codecs for ambiguous mime types.

  if (mime_type == "audio/mpeg" || mime_type == "audio/mp3" ||
      mime_type == "audio/x-mp3") {
    *default_codec = MimeUtil::MP3;
    return true;
  }

  if (mime_type == "audio/aac") {
    *default_codec = MimeUtil::MPEG4_AAC;
    return true;
  }

  if (mime_type == "audio/flac") {
    *default_codec = MimeUtil::FLAC;
    return true;
  }

  return false;
}

}  // namespace media::internal
