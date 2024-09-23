// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/filters/stream_parser_factory.h"

#include <stddef.h>

#include <algorithm>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/media.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder_config.h"
#include "media/formats/mp4/mp4_stream_parser.h"
#include "media/formats/mpeg/adts_stream_parser.h"
#include "media/formats/mpeg/mpeg1_audio_stream_parser.h"
#include "media/formats/webcodecs/webcodecs_encoded_chunk_stream_parser.h"
#include "media/formats/webm/webm_stream_parser.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "media/base/android/media_codec_util.h"
#endif

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "media/formats/mp4/es_descriptor.h"
#if BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)
#include "media/formats/mp2t/mp2t_stream_parser.h"
#endif
#endif

namespace media {

typedef bool (*CodecIDValidatorFunction)(std::string_view codecs_id,
                                         MediaLog* media_log);

struct CodecInfo {
  enum Type { UNKNOWN, AUDIO, VIDEO };

  // Update tools/metrics/histograms/enums.xml if new values are added.
  enum HistogramTag {
    HISTOGRAM_UNKNOWN,
    HISTOGRAM_VP8,
    HISTOGRAM_VP9,
    HISTOGRAM_VORBIS,
    HISTOGRAM_H264,
    HISTOGRAM_MPEG2AAC,
    HISTOGRAM_MPEG4AAC,
    HISTOGRAM_EAC3,
    HISTOGRAM_MP3,
    HISTOGRAM_OPUS,
    HISTOGRAM_HEVC,
    HISTOGRAM_AC3,
    HISTOGRAM_DOLBYVISION,
    HISTOGRAM_FLAC,
    HISTOGRAM_AV1,
    HISTOGRAM_MPEG_H_AUDIO,
    HISTOGRAM_DTS,
    HISTOGRAM_DTSXP2,
    HISTOGRAM_DTSE,
    HISTOGRAM_AC4,
    HISTOGRAM_IAMF,
    HISTOGRAM_MAX = HISTOGRAM_IAMF  // Must be equal to largest logged entry.
  };

  const char* pattern;
  Type type;
  CodecIDValidatorFunction validator;
  HistogramTag tag;
};

typedef StreamParser* (*ParserFactoryFunction)(
    base::span<const std::string> codecs,
    MediaLog* media_log);

struct SupportedTypeInfo {
  const char* type;
  const ParserFactoryFunction factory_function;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #global-scope
  RAW_PTR_EXCLUSION const CodecInfo* const* codecs;
};

static const CodecInfo kVP8CodecInfo = {"vp8", CodecInfo::VIDEO, nullptr,
                                        CodecInfo::HISTOGRAM_VP8};
static const CodecInfo kLegacyVP9CodecInfo = {"vp9", CodecInfo::VIDEO, nullptr,
                                              CodecInfo::HISTOGRAM_VP9};
static const CodecInfo kVP9CodecInfo = {"vp09.*", CodecInfo::VIDEO, nullptr,
                                        CodecInfo::HISTOGRAM_VP9};
static const CodecInfo kVorbisCodecInfo = {"vorbis", CodecInfo::AUDIO, nullptr,
                                           CodecInfo::HISTOGRAM_VORBIS};
static const CodecInfo kOpusCodecInfo = {"opus", CodecInfo::AUDIO, nullptr,
                                         CodecInfo::HISTOGRAM_OPUS};
static const CodecInfo kOpusCodecInfo2 = {"Opus", CodecInfo::AUDIO, nullptr,
                                          CodecInfo::HISTOGRAM_OPUS};

#if BUILDFLAG(ENABLE_AV1_DECODER)
// Note: Validation of the codec string is handled by the caller.
static const CodecInfo kAV1CodecInfo = {"av01.*", CodecInfo::VIDEO, nullptr,
                                        CodecInfo::HISTOGRAM_AV1};
#endif

static const CodecInfo* const kVideoWebMCodecs[] = {
    &kVP8CodecInfo,  &kLegacyVP9CodecInfo, &kVP9CodecInfo, &kVorbisCodecInfo,
    &kOpusCodecInfo, &kOpusCodecInfo2,
#if BUILDFLAG(ENABLE_AV1_DECODER)
    &kAV1CodecInfo,
#endif
    nullptr};

static const CodecInfo* const kAudioWebMCodecs[] = {
    &kVorbisCodecInfo, &kOpusCodecInfo, &kOpusCodecInfo2, nullptr};

static StreamParser* BuildWebMParser(base::span<const std::string> codecs,
                                     MediaLog* media_log) {
  return new WebMStreamParser();
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
static int GetMP4AudioObjectType(std::string_view codec_id,
                                 MediaLog* media_log) {
  // From RFC 6381 section 3.3 (ISO Base Media File Format Name Space):
  // When the first element of a ['codecs' parameter value] is 'mp4a' ...,
  // the second element is a hexadecimal representation of the MP4 Registration
  // Authority ObjectTypeIndication (OTI). Note that MP4RA uses a leading "0x"
  // with these values, which is omitted here and hence implied.
  std::vector<std::string_view> tokens = base::SplitStringPiece(
      codec_id, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (tokens.size() == 3 && tokens[0] == "mp4a" && tokens[1] == "40") {
    // From RFC 6381 section 3.3:
    // One of the OTI values for 'mp4a' is 40 (identifying MPEG-4 audio). For
    // this value, the third element identifies the audio ObjectTypeIndication
    // (OTI) ... expressed as a decimal number.
    int audio_object_type;
    if (base::StringToInt(tokens[2], &audio_object_type))
      return audio_object_type;
  }

  MEDIA_LOG(DEBUG, media_log)
      << "Malformed mimetype codec '" << codec_id << "'";
  return -1;
}

// AAC Object Type IDs that Chrome supports.
constexpr int kAACLCObjectType = 2;
constexpr int kAACSBRObjectType = 5;
constexpr int kAACPSObjectType = 29;
constexpr int kAACXHEObjectType = 42;

bool ValidateMP4ACodecID(std::string_view codec_id, MediaLog* media_log) {
  int audio_object_type = GetMP4AudioObjectType(codec_id, media_log);
  if (audio_object_type == kAACLCObjectType ||
      audio_object_type == kAACSBRObjectType ||
      audio_object_type == kAACPSObjectType ||
      audio_object_type == kAACXHEObjectType) {
    return true;
  }

  MEDIA_LOG(DEBUG, media_log)
      << "Unsupported audio object type " << audio_object_type << " in codec '"
      << codec_id << "'";
  return false;
}

static const CodecInfo kH264AVC1CodecInfo = {
    "avc1.*", CodecInfo::VIDEO, nullptr, CodecInfo::HISTOGRAM_H264};
static const CodecInfo kH264AVC3CodecInfo = {
    "avc3.*", CodecInfo::VIDEO, nullptr, CodecInfo::HISTOGRAM_H264};

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
static const CodecInfo kHEVCHEV1CodecInfo = {
    "hev1.*", CodecInfo::VIDEO, nullptr, CodecInfo::HISTOGRAM_HEVC};
static const CodecInfo kHEVCHVC1CodecInfo = {
    "hvc1.*", CodecInfo::VIDEO, nullptr, CodecInfo::HISTOGRAM_HEVC};
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
#if BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
static const CodecInfo kDolbyVisionAVCCodecInfo1 = {
    "dva1.*", CodecInfo::VIDEO, nullptr, CodecInfo::HISTOGRAM_DOLBYVISION};
static const CodecInfo kDolbyVisionAVCCodecInfo2 = {
    "dvav.*", CodecInfo::VIDEO, nullptr, CodecInfo::HISTOGRAM_DOLBYVISION};
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
static const CodecInfo kDolbyVisionHEVCCodecInfo1 = {
    "dvh1.*", CodecInfo::VIDEO, nullptr, CodecInfo::HISTOGRAM_DOLBYVISION};
static const CodecInfo kDolbyVisionHEVCCodecInfo2 = {
    "dvhe.*", CodecInfo::VIDEO, nullptr, CodecInfo::HISTOGRAM_DOLBYVISION};
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
#endif  // BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
static const CodecInfo kMPEG4AACCodecInfo = {"mp4a.40.*", CodecInfo::AUDIO,
                                             &ValidateMP4ACodecID,
                                             CodecInfo::HISTOGRAM_MPEG4AAC};
static const CodecInfo kMPEG2AACLCCodecInfo = {
    "mp4a.67", CodecInfo::AUDIO, nullptr, CodecInfo::HISTOGRAM_MPEG2AAC};

#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
// The 'ac-3' and 'ec-3' are mime codec ids for AC3 and EAC3 according to
// http://www.mp4ra.org/codecs.html
// The object types for AC3 and EAC3 in MP4 container are 0xa5 and 0xa6, so
// according to RFC 6381 this corresponds to codec ids 'mp4a.A5' and 'mp4a.A6'.
// Codec ids with lower case oti (mp4a.a5 and mp4a.a6) are supported for
// backward compatibility.
static const CodecInfo kAC3CodecInfo1 = {"ac-3", CodecInfo::AUDIO, nullptr,
                                         CodecInfo::HISTOGRAM_AC3};
static const CodecInfo kAC3CodecInfo2 = {"mp4a.a5", CodecInfo::AUDIO, nullptr,
                                         CodecInfo::HISTOGRAM_AC3};
static const CodecInfo kAC3CodecInfo3 = {"mp4a.A5", CodecInfo::AUDIO, nullptr,
                                         CodecInfo::HISTOGRAM_AC3};
static const CodecInfo kEAC3CodecInfo1 = {"ec-3", CodecInfo::AUDIO, nullptr,
                                          CodecInfo::HISTOGRAM_EAC3};
static const CodecInfo kEAC3CodecInfo2 = {"mp4a.a6", CodecInfo::AUDIO, nullptr,
                                          CodecInfo::HISTOGRAM_EAC3};
static const CodecInfo kEAC3CodecInfo3 = {"mp4a.A6", CodecInfo::AUDIO, nullptr,
                                          CodecInfo::HISTOGRAM_EAC3};
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)

#if BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
// The 'ac-4' is mime codec id for AC4 according to
// http://www.mp4ra.org/codecs.html
// The object types for AC4 in MP4 container is 0xae, so according to RFC 6381
// this corresponds to codec ids 'mp4a.AE'.
// Codec id with lower case oti (mp4a.ae) is supported for backward
// compatibility.
static const CodecInfo kAC4CodecInfo1 = {"ac-4.*", CodecInfo::AUDIO, nullptr,
                                         CodecInfo::HISTOGRAM_AC4};
static const CodecInfo kAC4CodecInfo2 = {"mp4a.ae.*", CodecInfo::AUDIO, nullptr,
                                         CodecInfo::HISTOGRAM_AC4};
static const CodecInfo kAC4CodecInfo3 = {"mp4a.AE.*", CodecInfo::AUDIO, nullptr,
                                         CodecInfo::HISTOGRAM_AC4};
static const CodecInfo kAC4CodecInfo4 = {"ac-4", CodecInfo::AUDIO, nullptr,
                                         CodecInfo::HISTOGRAM_AC4};
static const CodecInfo kAC4CodecInfo5 = {"mp4a.ae", CodecInfo::AUDIO, nullptr,
                                         CodecInfo::HISTOGRAM_AC4};
static const CodecInfo kAC4CodecInfo6 = {"mp4a.AE", CodecInfo::AUDIO, nullptr,
                                         CodecInfo::HISTOGRAM_AC4};
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)

#if BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
// The 'dtsc' and 'dtsx' are mime codec ids for DTS and DTSX according to
// http://mp4ra.org/#/codecs
// The object types for DTS and DTSX in MP4 container are 0xa9 and 0xb2, so
// according to RFC 6381 this corresponds to codec ids 'mp4a.A9' and 'mp4a.B2'.
// Codec ids with lower case oti (mp4a.a9 and mp4a.b2) are supported for
// backward compatibility.
static const CodecInfo kDTSCodecInfo1 = {"dtsc", CodecInfo::AUDIO, nullptr,
                                         CodecInfo::HISTOGRAM_DTS};
static const CodecInfo kDTSCodecInfo2 = {"mp4a.a9", CodecInfo::AUDIO, nullptr,
                                         CodecInfo::HISTOGRAM_DTS};
static const CodecInfo kDTSCodecInfo3 = {"mp4a.A9", CodecInfo::AUDIO, nullptr,
                                         CodecInfo::HISTOGRAM_DTS};
static const CodecInfo kDTSXCodecInfo1 = {"dtsx", CodecInfo::AUDIO, nullptr,
                                          CodecInfo::HISTOGRAM_DTSXP2};
static const CodecInfo kDTSXCodecInfo2 = {"mp4a.b2", CodecInfo::AUDIO, nullptr,
                                          CodecInfo::HISTOGRAM_DTSXP2};
static const CodecInfo kDTSXCodecInfo3 = {"mp4a.B2", CodecInfo::AUDIO, nullptr,
                                          CodecInfo::HISTOGRAM_DTSXP2};
static const CodecInfo kDTSECodecInfo1 = {"dtse", CodecInfo::AUDIO, nullptr,
                                          CodecInfo::HISTOGRAM_DTSE};
static const CodecInfo kDTSECodecInfo2 = {"mp4a.ac", CodecInfo::AUDIO, nullptr,
                                          CodecInfo::HISTOGRAM_DTSE};
static const CodecInfo kDTSECodecInfo3 = {"mp4a.AC", CodecInfo::AUDIO, nullptr,
                                          CodecInfo::HISTOGRAM_DTSE};
#endif  // BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)

#if BUILDFLAG(ENABLE_PLATFORM_MPEG_H_AUDIO)
static const CodecInfo kMpegHAudioCodecInfo1 = {
    "mhm1.*", CodecInfo::AUDIO, nullptr, CodecInfo::HISTOGRAM_MPEG_H_AUDIO};
static const CodecInfo kMpegHAudioCodecInfo2 = {
    "mha1.*", CodecInfo::AUDIO, nullptr, CodecInfo::HISTOGRAM_MPEG_H_AUDIO};
#endif

#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

static const CodecInfo kMP3CodecInfo = {nullptr, CodecInfo::AUDIO, nullptr,
                                        CodecInfo::HISTOGRAM_MP3};
static const CodecInfo* const kAudioMP3Codecs[] = {&kMP3CodecInfo, nullptr};

static StreamParser* BuildMP3Parser(base::span<const std::string> codecs,
                                    MediaLog* media_log) {
  return new MPEG1AudioStreamParser();
}

static const CodecInfo kMPEG4VP09CodecInfo = {
    "vp09.*", CodecInfo::VIDEO, nullptr, CodecInfo::HISTOGRAM_VP9};
static const CodecInfo kMPEG4FLACCodecInfo = {"flac", CodecInfo::AUDIO, nullptr,
                                              CodecInfo::HISTOGRAM_FLAC};
static const CodecInfo kMPEG4FLACCodecInfo2 = {
    "fLaC", CodecInfo::AUDIO, nullptr, CodecInfo::HISTOGRAM_FLAC};
#if BUILDFLAG(ENABLE_PLATFORM_IAMF_AUDIO)
static const CodecInfo kIAMFCodecInfo1 = {"iamf", CodecInfo::AUDIO, nullptr,
                                          CodecInfo::HISTOGRAM_IAMF};
static const CodecInfo kIAMFCodecInfo2 = {"iamf.*", CodecInfo::AUDIO, nullptr,
                                          CodecInfo::HISTOGRAM_IAMF};
#endif  // BUILDFLAG(ENABLE_PLATFORM_IAMF_AUDIO)

static const CodecInfo* const kVideoMP4Codecs[] = {&kMPEG4FLACCodecInfo,
                                                   &kMPEG4FLACCodecInfo2,
                                                   &kOpusCodecInfo,
                                                   &kOpusCodecInfo2,
                                                   &kMPEG4VP09CodecInfo,
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
                                                   &kH264AVC1CodecInfo,
                                                   &kH264AVC3CodecInfo,
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
                                                   &kHEVCHEV1CodecInfo,
                                                   &kHEVCHVC1CodecInfo,
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
#if BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
                                                   &kDolbyVisionAVCCodecInfo1,
                                                   &kDolbyVisionAVCCodecInfo2,
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
                                                   &kDolbyVisionHEVCCodecInfo1,
                                                   &kDolbyVisionHEVCCodecInfo2,
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
#endif  // BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
                                                   &kMPEG4AACCodecInfo,
                                                   &kMPEG2AACLCCodecInfo,
#if BUILDFLAG(ENABLE_PLATFORM_MPEG_H_AUDIO)
                                                   &kMpegHAudioCodecInfo1,
                                                   &kMpegHAudioCodecInfo2,
#endif  // BUILDFLAG(ENABLE_PLATFORM_MPEG_H_AUDIO)
#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
                                                   &kAC3CodecInfo1,
                                                   &kAC3CodecInfo2,
                                                   &kAC3CodecInfo3,
                                                   &kEAC3CodecInfo1,
                                                   &kEAC3CodecInfo2,
                                                   &kEAC3CodecInfo3,
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
#if BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
                                                   &kAC4CodecInfo1,
                                                   &kAC4CodecInfo2,
                                                   &kAC4CodecInfo3,
                                                   &kAC4CodecInfo4,
                                                   &kAC4CodecInfo5,
                                                   &kAC4CodecInfo6,
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
#if BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
                                                   &kDTSCodecInfo1,
                                                   &kDTSCodecInfo2,
                                                   &kDTSCodecInfo3,
                                                   &kDTSXCodecInfo1,
                                                   &kDTSXCodecInfo2,
                                                   &kDTSXCodecInfo3,
                                                   &kDTSECodecInfo1,
                                                   &kDTSECodecInfo2,
                                                   &kDTSECodecInfo3,
#endif  // BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
#if BUILDFLAG(ENABLE_AV1_DECODER)
                                                   &kAV1CodecInfo,
#endif  // BUILDFLAG(ENABLE_AV1_DECODER)
#if BUILDFLAG(ENABLE_PLATFORM_IAMF_AUDIO)
                                                   &kIAMFCodecInfo1,
                                                   &kIAMFCodecInfo2,
#endif  // BUILDFLAG(ENABLE_PLATFORM_IAMF_AUDIO)
                                                   nullptr};

static const CodecInfo* const kAudioMP4Codecs[] = {&kMPEG4FLACCodecInfo,
                                                   &kMPEG4FLACCodecInfo2,
                                                   &kOpusCodecInfo,
                                                   &kOpusCodecInfo2,
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
                                                   &kMPEG4AACCodecInfo,
                                                   &kMPEG2AACLCCodecInfo,
#if BUILDFLAG(ENABLE_PLATFORM_MPEG_H_AUDIO)
                                                   &kMpegHAudioCodecInfo1,
                                                   &kMpegHAudioCodecInfo2,
#endif  // BUILDFLAG(ENABLE_PLATFORM_MPEG_H_AUDIO)
#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
                                                   &kAC3CodecInfo1,
                                                   &kAC3CodecInfo2,
                                                   &kAC3CodecInfo3,
                                                   &kEAC3CodecInfo1,
                                                   &kEAC3CodecInfo2,
                                                   &kEAC3CodecInfo3,
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
#if BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
                                                   &kAC4CodecInfo1,
                                                   &kAC4CodecInfo2,
                                                   &kAC4CodecInfo3,
                                                   &kAC4CodecInfo4,
                                                   &kAC4CodecInfo5,
                                                   &kAC4CodecInfo6,
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
#if BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
                                                   &kDTSCodecInfo1,
                                                   &kDTSCodecInfo2,
                                                   &kDTSCodecInfo3,
                                                   &kDTSXCodecInfo1,
                                                   &kDTSXCodecInfo2,
                                                   &kDTSXCodecInfo3,
                                                   &kDTSECodecInfo1,
                                                   &kDTSECodecInfo2,
                                                   &kDTSECodecInfo3,
#endif  // BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
#if BUILDFLAG(ENABLE_PLATFORM_IAMF_AUDIO)
                                                   &kIAMFCodecInfo1,
                                                   &kIAMFCodecInfo2,
#endif  // BUILDFLAG(ENABLE_PLATFORM_IAMF_AUDIO)
                                                   nullptr};

static StreamParser* BuildMP4Parser(base::span<const std::string> codecs,
                                    MediaLog* media_log) {
  base::flat_set<int> audio_object_types;
  bool has_sbr = false;
  bool has_dv = false;

  // The draft version 0.0.4 FLAC-in-ISO spec
  // (https://github.com/xiph/flac/blob/master/doc/isoflac.txt) does not define
  // any encapsulation using MP4AudioSampleEntry with objectTypeIndication.
  // Rather, it uses a FLAC-specific "fLaC" codingname in the sample entry along
  // with a "dfLa" FLACSpecificBox. We still need to tell our parser to
  // conditionally expect a FLAC stream, hence |has_flac|.
  bool has_flac = false;

  // Like FLAC, IAMF v1.0.0
  // (https://aomediacodec.github.io/iamf/v1.0.0.html#isobmff) does not define
  // an encapsulation using MP4AudioSampleEntry with objectTypeIndication.
  bool has_iamf = false;

  for (const auto& codec_id : codecs) {
    if (base::MatchPattern(codec_id, kMPEG4FLACCodecInfo.pattern) ||
        base::MatchPattern(codec_id, kMPEG4FLACCodecInfo2.pattern)) {
      has_flac = true;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    } else if (base::MatchPattern(codec_id, kMPEG2AACLCCodecInfo.pattern)) {
      audio_object_types.insert(mp4::kISO_13818_7_AAC_LC);
    } else if (base::MatchPattern(codec_id, kMPEG4AACCodecInfo.pattern)) {
      int audio_object_type = GetMP4AudioObjectType(codec_id, media_log);
      DCHECK_GT(audio_object_type, 0);

      audio_object_types.insert(mp4::kISO_14496_3);

      if (audio_object_type == kAACSBRObjectType ||
          audio_object_type == kAACPSObjectType) {
        has_sbr = true;
        break;
      }
#if BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
    } else if (base::MatchPattern(codec_id,
                                  kDolbyVisionAVCCodecInfo1.pattern) ||
               base::MatchPattern(codec_id, kDolbyVisionAVCCodecInfo2.pattern)
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
               || base::MatchPattern(codec_id,
                                     kDolbyVisionHEVCCodecInfo1.pattern) ||
               base::MatchPattern(codec_id, kDolbyVisionHEVCCodecInfo2.pattern)
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
    ) {
      has_dv = true;
#endif  // BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
    } else if (base::MatchPattern(codec_id, kAC3CodecInfo1.pattern) ||
               base::MatchPattern(codec_id, kAC3CodecInfo2.pattern) ||
               base::MatchPattern(codec_id, kAC3CodecInfo3.pattern)) {
      audio_object_types.insert(mp4::kAC3);
    } else if (base::MatchPattern(codec_id, kEAC3CodecInfo1.pattern) ||
               base::MatchPattern(codec_id, kEAC3CodecInfo2.pattern) ||
               base::MatchPattern(codec_id, kEAC3CodecInfo3.pattern)) {
      audio_object_types.insert(mp4::kEAC3);
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
#if BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
    } else if (base::MatchPattern(codec_id, kAC4CodecInfo1.pattern) ||
               base::MatchPattern(codec_id, kAC4CodecInfo2.pattern) ||
               base::MatchPattern(codec_id, kAC4CodecInfo3.pattern) ||
               base::MatchPattern(codec_id, kAC4CodecInfo4.pattern) ||
               base::MatchPattern(codec_id, kAC4CodecInfo5.pattern) ||
               base::MatchPattern(codec_id, kAC4CodecInfo6.pattern)) {
      audio_object_types.insert(mp4::kAC4);
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
#if BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
    } else if (base::MatchPattern(codec_id, kDTSCodecInfo1.pattern) ||
               base::MatchPattern(codec_id, kDTSCodecInfo2.pattern) ||
               base::MatchPattern(codec_id, kDTSCodecInfo3.pattern)) {
      audio_object_types.insert(mp4::kDTS);
    } else if (base::MatchPattern(codec_id, kDTSXCodecInfo1.pattern) ||
               base::MatchPattern(codec_id, kDTSXCodecInfo2.pattern) ||
               base::MatchPattern(codec_id, kDTSXCodecInfo3.pattern)) {
      audio_object_types.insert(mp4::kDTSX);
    } else if (base::MatchPattern(codec_id, kDTSECodecInfo1.pattern) ||
               base::MatchPattern(codec_id, kDTSECodecInfo2.pattern) ||
               base::MatchPattern(codec_id, kDTSECodecInfo3.pattern)) {
      audio_object_types.insert(mp4::kDTSE);
#endif  // BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
#if BUILDFLAG(ENABLE_PLATFORM_IAMF_AUDIO)
    } else if (base::MatchPattern(codec_id, kIAMFCodecInfo1.pattern) ||
               base::MatchPattern(codec_id, kIAMFCodecInfo2.pattern)) {
      has_iamf = true;
#endif  //  BUILDFLAG(ENABLE_PLATFORM_IAMF_AUDIO)
    }
  }

  return new mp4::MP4StreamParser(audio_object_types, has_sbr, has_flac,
                                  has_iamf, has_dv);
}
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
static const CodecInfo kADTSCodecInfo = {nullptr, CodecInfo::AUDIO, nullptr,
                                         CodecInfo::HISTOGRAM_MPEG4AAC};
static const CodecInfo* const kAudioADTSCodecs[] = {&kADTSCodecInfo, nullptr};

static StreamParser* BuildADTSParser(base::span<const std::string> codecs,
                                     MediaLog* media_log) {
  return new ADTSStreamParser();
}

#if BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)
// These codec ids correspond to object types registered with MP4RA and are the
// same as MP3 audio codec ids in media/base/mime_util_internal.cc.
// From http://www.mp4ra.org/object.html:
// 69   Audio ISO/IEC 13818-3
// 6B   Audio ISO/IEC 11172-3
static const CodecInfo kMPEG2TS_MP3CodecInfo1 = {
    "mp4a.69", CodecInfo::AUDIO, nullptr, CodecInfo::HISTOGRAM_MP3};
static const CodecInfo kMPEG2TS_MP3CodecInfo2 = {
    "mp4a.6B", CodecInfo::AUDIO, nullptr, CodecInfo::HISTOGRAM_MP3};

static const CodecInfo* const kVideoMP2TCodecs[] = {&kH264AVC1CodecInfo,
                                                    &kH264AVC3CodecInfo,
                                                    &kMPEG2TS_MP3CodecInfo1,
                                                    &kMPEG2TS_MP3CodecInfo2,
                                                    &kMPEG4AACCodecInfo,
                                                    &kMPEG2AACLCCodecInfo,
                                                    nullptr};

static StreamParser* BuildMP2TParser(base::span<const std::string> codecs,
                                     MediaLog* media_log) {
  bool has_sbr = false;
  for (const auto& codec_id : codecs) {
    if (base::MatchPattern(codec_id, kMPEG4AACCodecInfo.pattern)) {
      int audio_object_type = GetMP4AudioObjectType(codec_id, media_log);
      if (audio_object_type == kAACSBRObjectType ||
          audio_object_type == kAACPSObjectType) {
        has_sbr = true;
      }
    }
  }

  return new media::mp2t::Mp2tStreamParser(codecs, has_sbr);
}
#endif  // ENABLE_MSE_MPEG2TS_STREAM_PARSER
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

static const SupportedTypeInfo kSupportedTypeInfo[] = {
    {"video/webm", &BuildWebMParser, kVideoWebMCodecs},
    {"audio/webm", &BuildWebMParser, kAudioWebMCodecs},
    {"audio/mpeg", &BuildMP3Parser, kAudioMP3Codecs},
    // NOTE: Including proprietary MP4 codecs is gated by build flags above.
    {"video/mp4", &BuildMP4Parser, kVideoMP4Codecs},
    {"audio/mp4", &BuildMP4Parser, kAudioMP4Codecs},
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    {"audio/aac", &BuildADTSParser, kAudioADTSCodecs},
#if BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)
    {"video/mp2t", &BuildMP2TParser, kVideoMP2TCodecs},
#endif
#endif
};

#if BUILDFLAG(ENABLE_HLS_DEMUXER)
std::unique_ptr<StreamParser> StreamParserFactory::CreateRelaxedParser(
    RelaxedParserSupportedType mime) {
  const bool enable_mp4 = base::FeatureList::IsEnabled(kBuiltInHlsMP4);
  switch (mime) {
    case RelaxedParserSupportedType::kMP2T: {
      // TODO(issue/40253609): Figure out how to determine SBR presence.
      return std::make_unique<mp2t::Mp2tStreamParser>(std::nullopt, false);
    }
    case RelaxedParserSupportedType::kAAC: {
      return std::make_unique<ADTSStreamParser>();
    }
    case RelaxedParserSupportedType::kMP4: {
      // TODO(issue/40253609): Figure out how to determine presence of SBR,
      // FLAC, IAMF, DolbyVision.
      return enable_mp4 ? std::make_unique<mp4::MP4StreamParser>(
                              std::nullopt, false, true, false, false)
                        : nullptr;
    }
  }
}
#endif

// Verify that |codec_info| is supported on this platform.
//
// Returns true if |codec_info| is a valid audio/video codec and is allowed.
// |audio_codecs| has |codec_info|.tag added to its list if |codec_info| is an
// audio codec. |audio_codecs| may be nullptr, in which case it is not updated.
// |video_codecs| has |codec_info|.tag added to its list if |codec_info| is a
// video codec. |video_codecs| may be nullptr, in which case it is not updated.
//
// Returns false otherwise, and |audio_codecs| and |video_codecs| not touched.
static bool VerifyCodec(const CodecInfo* codec_info,
                        std::vector<CodecInfo::HistogramTag>* audio_codecs,
                        std::vector<CodecInfo::HistogramTag>* video_codecs) {
  switch (codec_info->type) {
    case CodecInfo::AUDIO:
      if (audio_codecs)
        audio_codecs->push_back(codec_info->tag);
      return true;
    case CodecInfo::VIDEO:
      if (video_codecs)
        video_codecs->push_back(codec_info->tag);
      return true;
    default:
      // Not audio or video, so skip it.
      DVLOG(1) << "CodecInfo type of " << codec_info->type
               << " should not be specified in a SupportedTypes list";
      return false;
  }
}

// Checks to see if the specified |type| and |codecs| list are supported.
// Returns kNotSupported if |type| and |codecs| are definitively not supported.
// The values of |factory_function|, |audio_codecs|, and |video_codecs| are
// undefined in this case.
// Returns kSupported if |type| and all codecs listed in |codecs| are
// supported, any non-empty codecs requirement is met for |type|, and all of
// |codecs| are supported for |type|.
// Returns kMaybeSupported if |type| is supported, but requires a codecs
// parameter that is missing.
// For both kSupported and kMaybeSupported results, |factory_function| is
// updated to be a function that can build a StreamParser for this type,
// |audio_codecs| is updated with the appropriate HistogramTags for matching
// audio codecs specified in |codecs|, and |video_codecs| is updated with the
// appropriate HistogramTags for matching video codecs specified in |codecs|.
// The value of each of |factory_function|, |audio_codecs| and |video_codecs| is
// not updated if it was nullptr initially.
static SupportsType CheckTypeAndCodecs(
    std::string_view type,
    base::span<const std::string> codecs,
    MediaLog* media_log,
    ParserFactoryFunction* factory_function,
    std::vector<CodecInfo::HistogramTag>* audio_codecs,
    std::vector<CodecInfo::HistogramTag>* video_codecs) {
  // Search for the SupportedTypeInfo for |type|.
  for (const auto& type_info : kSupportedTypeInfo) {
    if (type == type_info.type) {
      if (codecs.empty()) {
        const CodecInfo* codec_info = type_info.codecs[0];
        if (codec_info && !codec_info->pattern &&
            VerifyCodec(codec_info, audio_codecs, video_codecs)) {
          // If there was no specified codec parameter, and if the major/minor
          // type is supported, specific and requires no codec parameter (such
          // as audio/mpeg is specific to MP3), then populate the expected
          // specific codec value and factory function and return definitive
          // support.
          if (factory_function)
            *factory_function = type_info.factory_function;
          return SupportsType::kSupported;
        }

        MEDIA_LOG(DEBUG, media_log)
            << "A codecs parameter must be provided for '" << type
            << "' to determine definitive support proactively.";
        return SupportsType::kMaybeSupported;
      }

      // Make sure all the codecs specified in |codecs| are
      // in the supported type info.
      for (const auto& codec_id : codecs) {
        // Search the type info for a match.
        bool found_codec = false;
        for (int i = 0; type_info.codecs[i]; ++i) {
          // Only check a codec pattern if there is one to check. Some types,
          // like audio/mpeg and audio/aac require there be no codecs parameter,
          // and instead have implicit codec. If a codec is provided for such a
          // type then it is not supported by MSE. We don't check any other
          // potential matches because none should be configured.
          if (!type_info.codecs[i]->pattern) {
            DCHECK(i == 0 && !type_info.codecs[1])
                << "For a type with implicit codec, then only one codec must "
                   "be configured";
            break;
          }

          if (base::MatchPattern(codec_id, type_info.codecs[i]->pattern) &&
              (!type_info.codecs[i]->validator ||
               type_info.codecs[i]->validator(codec_id, media_log))) {
            found_codec =
                VerifyCodec(type_info.codecs[i], audio_codecs, video_codecs);
            break;  // Since only 1 pattern will match, no need to check others.
          }
        }

        if (!found_codec) {
          MEDIA_LOG(DEBUG, media_log)
              << "Codec '" << codec_id << "' is not supported for '" << type
              << "'";
          // Though the major/minor type is supported, a codecs parameter value
          // was found to not be supported.
          return SupportsType::kNotSupported;
        }
      }

      if (factory_function)
        *factory_function = type_info.factory_function;

      // There was a non-empty |codecs| for this supported |type|, and all of
      // |codecs| are supported for this |type|.
      return SupportsType::kSupported;
    }
  }

  // |type| didn't match any of the supported types.
  return SupportsType::kNotSupported;
}

// static
SupportsType StreamParserFactory::IsTypeSupported(
    std::string_view type,
    base::span<const std::string> codecs) {
  // TODO(wolenetz): Questionable MediaLog usage, http://crbug.com/712310
  NullMediaLog media_log;
  return CheckTypeAndCodecs(type, codecs, &media_log, nullptr, nullptr,
                            nullptr);
}

// static
std::unique_ptr<StreamParser> StreamParserFactory::Create(
    std::string_view type,
    base::span<const std::string> codecs,
    MediaLog* media_log) {
  std::unique_ptr<StreamParser> stream_parser;
  ParserFactoryFunction factory_function;
  std::vector<CodecInfo::HistogramTag> audio_codecs;
  std::vector<CodecInfo::HistogramTag> video_codecs;

  // TODO(crbug.com/41204005): Relax the requirement for specific codecs (allow
  // kMaybeSupported here), and relocate the logging to the parser configuration
  // callback. This creation method is called in AddId(), and also in
  // CanChangeType() and ChangeType(), so potentially overlogs codecs leading to
  // disproportion versus actually parsed codec configurations from
  // initialization segments. For this work and also recording when implicit
  // codec switching occurs (without explicit ChangeType), see
  // https://crbug.com/535738.
  SupportsType supportsType = CheckTypeAndCodecs(
      type, codecs, media_log, &factory_function, &audio_codecs, &video_codecs);

  if (SupportsType::kSupported == supportsType) {
    // Log the expected codecs.
    for (size_t i = 0; i < audio_codecs.size(); ++i) {
      UMA_HISTOGRAM_ENUMERATION("Media.MSE.AudioCodec", audio_codecs[i],
                                CodecInfo::HISTOGRAM_MAX + 1);
    }
    for (size_t i = 0; i < video_codecs.size(); ++i) {
      UMA_HISTOGRAM_ENUMERATION("Media.MSE.VideoCodec", video_codecs[i],
                                CodecInfo::HISTOGRAM_MAX + 1);
      if (type == "video/mp4") {
        UMA_HISTOGRAM_ENUMERATION("Media.MSE.VideoCodec.MP4", video_codecs[i],
                                  CodecInfo::HISTOGRAM_MAX + 1);
      } else if (type == "video/webm") {
        UMA_HISTOGRAM_ENUMERATION("Media.MSE.VideoCodec.WebM", video_codecs[i],
                                  CodecInfo::HISTOGRAM_MAX + 1);
      }
    }

    stream_parser.reset(factory_function(codecs, media_log));
  }

  return stream_parser;
}

// static
std::unique_ptr<StreamParser> StreamParserFactory::Create(
    std::unique_ptr<AudioDecoderConfig> audio_config) {
  DCHECK(audio_config);

  // TODO(crbug.com/40155657): Histogram-log the codec used for buffering
  // WebCodecs in MSE?

  return std::make_unique<media::WebCodecsEncodedChunkStreamParser>(
      std::move(audio_config));
}

// static
std::unique_ptr<StreamParser> StreamParserFactory::Create(
    std::unique_ptr<VideoDecoderConfig> video_config) {
  DCHECK(video_config);

  // TODO(crbug.com/40155657): Histogram-log the codec used for buffering
  // WebCodecs in MSE?

  return std::make_unique<media::WebCodecsEncodedChunkStreamParser>(
      std::move(video_config));
}

}  // namespace media
