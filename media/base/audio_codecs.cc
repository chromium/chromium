// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_codecs.h"

#include <ostream>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace media {

// These names come from src/third_party/ffmpeg/libavcodec/codec_desc.c
std::string GetCodecName(AudioCodec codec) {
  switch (codec) {
    case AudioCodec::kUnknown:
      return "unknown";
    case AudioCodec::kAAC:
      return "aac";
    case AudioCodec::kMP3:
      return "mp3";
    case AudioCodec::kPCM:
    case AudioCodec::kPCM_S16BE:
    case AudioCodec::kPCM_S24BE:
      return "pcm";
    case AudioCodec::kVorbis:
      return "vorbis";
    case AudioCodec::kFLAC:
      return "flac";
    case AudioCodec::kAMR_NB:
      return "amr_nb";
    case AudioCodec::kAMR_WB:
      return "amr_wb";
    case AudioCodec::kPCM_MULAW:
      return "pcm_mulaw";
    case AudioCodec::kGSM_MS:
      return "gsm_ms";
    case AudioCodec::kOpus:
      return "opus";
    case AudioCodec::kPCM_ALAW:
      return "pcm_alaw";
    case AudioCodec::kEAC3:
      return "eac3";
    case AudioCodec::kALAC:
      return "alac";
    case AudioCodec::kAC3:
      return "ac3";
    case AudioCodec::kMpegHAudio:
      return "mpeg-h-audio";
    case AudioCodec::kDTS:
      return "dts";
    case AudioCodec::kDTSXP2:
      return "dtsx-p2";
    case AudioCodec::kDTSE:
      return "dtse";
    case AudioCodec::kAC4:
      return "ac4";
  }
}

std::string GetProfileName(AudioCodecProfile profile) {
  switch (profile) {
    case AudioCodecProfile::kUnknown:
      return "unknown";
    case AudioCodecProfile::kXHE_AAC:
      return "xhe-aac";
  }
}

AudioCodec StringToAudioCodec(const std::string& codec_id) {
  if (codec_id == "aac")
    return AudioCodec::kAAC;
  if (codec_id == "ac-3" || codec_id == "mp4a.A5" || codec_id == "mp4a.a5")
    return AudioCodec::kAC3;
  if (codec_id == "ac-4" || codec_id == "mp4a.AE" || codec_id == "mp4a.ae" ||
      base::StartsWith(codec_id, "ac-4.", base::CompareCase::SENSITIVE)) {
    return AudioCodec::kAC4;
  }
  if (codec_id == "ec-3" || codec_id == "mp4a.A6" || codec_id == "mp4a.a6")
    return AudioCodec::kEAC3;
  if (codec_id == "dtsc" || codec_id == "mp4a.A9" || codec_id == "mp4a.a9") {
    return AudioCodec::kDTS;
  }
  if (codec_id == "dtse" || codec_id == "mp4a.AC" || codec_id == "mp4a.ac") {
    return AudioCodec::kDTSE;
  }
  if (codec_id == "dtsx" || codec_id == "mp4a.B2" || codec_id == "mp4a.b2") {
    return AudioCodec::kDTSXP2;
  }
  if (codec_id == "mp3" || codec_id == "mp4a.69" || codec_id == "mp4a.6B")
    return AudioCodec::kMP3;
  if (codec_id == "alac")
    return AudioCodec::kALAC;
  if (codec_id == "flac")
    return AudioCodec::kFLAC;
  if (base::StartsWith(codec_id, "mhm1.", base::CompareCase::SENSITIVE) ||
      base::StartsWith(codec_id, "mha1.", base::CompareCase::SENSITIVE)) {
    return AudioCodec::kMpegHAudio;
  }
  if (codec_id == "opus")
    return AudioCodec::kOpus;
  if (codec_id == "vorbis")
    return AudioCodec::kVorbis;
  if (base::StartsWith(codec_id, "mp4a.40.", base::CompareCase::SENSITIVE))
    return AudioCodec::kAAC;
  return AudioCodec::kUnknown;
}

std::ostream& operator<<(std::ostream& os, const AudioCodec& codec) {
  return os << GetCodecName(codec);
}

#if BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
bool ParseDolbyAc4CodecId(const std::string& codec_id,
                          uint8_t* bitstream_versionc,
                          uint8_t* presentation_versionc,
                          uint8_t* presentation_levelc) {
  // Reference: ETSI Standard: Digital Audio Compression (AC-4) Standard;
  //            Part 2: Immersive and personalized audio
  //            ETSI TS 103 190-2 v1.2.1 (2018-02)
  //            E.13 The MIME codecs parameter
  // (https://www.etsi.org/deliver/etsi_ts/103100_103199/10319002/01.02.01_60/ts_10319002v010201p.pdf)

  if (!base::StartsWith(codec_id, "ac-4")) {
    return false;
  }

  const int kMaxAc4CodecIdLength =
      4     // FOURCC string "ac-4"
      + 1   // delimiting period
      + 2   // bitstream_version version as 2 digit string
      + 1   // delimiting period
      + 2   // presentation_version as 2 digit string.
      + 1   // delimiting period
      + 2;  // presentation_level as 2 digit string.

  if (codec_id.size() > kMaxAc4CodecIdLength) {
    DVLOG(4) << __func__ << ": Codec id is too long (" << codec_id << ")";
    return false;
  }

  std::vector<std::string> elem = base::SplitString(
      codec_id, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  DCHECK(elem[0] == "ac-4");

  if (elem.size() != 4) {
    DVLOG(4) << __func__ << ": invalid Dolby AC-4 codec id:" << codec_id;
    return false;
  }

  if (StringToAudioCodec(elem[0]) != AudioCodec::kAC4) {
    DVLOG(4) << __func__ << ": invalid Dolby AC-4 codec id:" << codec_id;
    return false;
  }

  // bitstream_version string should be two digits and equals "02". "00","01"
  // are valid according to spec, but they are not used in real case anymore.
  unsigned bitstream_version = 0;
  if (elem[1].size() != 2 ||
      !base::HexStringToUInt(elem[1], &bitstream_version) ||
      bitstream_version != 0x02) {
    DVLOG(4) << __func__
             << ": invalid Dolby AC-4 bitstream version: " << elem[1];
    return false;
  }

  // If bitstream_version is "02", then "01" and "02" are valid values for
  // presentation_version. Value "02" is special for AC-4 IMS.
  // Reference: ETSI Standard: Digital Audio Compression (AC-4) Standard;
  //            Part 2: Immersive and personalized audio
  //            ETSI TS 103 190-2 v1.2.1 (2018-02)
  //            4.6 Decoder compatibilities
  // (https://www.etsi.org/deliver/etsi_ts/103100_103199/10319002/01.02.01_60/ts_10319002v010201p.pdf)
  unsigned presentation_version = 0;
  if (elem[2].size() != 2 ||
      !base::HexStringToUInt(elem[2], &presentation_version) ||
      !(presentation_version == 0x01 || presentation_version == 0x02)) {
    DVLOG(4) << __func__
             << ": invalid Dolby AC-4 presentation_version: " << elem[1];
    return false;
  }

  // presentation_level(mdcompat) should be two digits and in range
  // [0,1,2,3,4,5,6,7].
  // Reference: ETSI Standard: Digital Audio Compression (AC-4) Standard;
  //            Part 2: Immersive and personalized audio
  //            ETSI TS 103 190-2 v1.2.1 (2018-02)
  //            6.3.2.2.3 mdcompat
  // (https://www.etsi.org/deliver/etsi_ts/103100_103199/10319002/01.02.01_60/ts_10319002v010201p.pdf)
  unsigned presentation_level = 0;
  if (elem[3].size() != 2 ||
      !base::HexStringToUInt(elem[3], &presentation_level) ||
      presentation_level > 0x07) {
    DVLOG(4) << __func__
             << ": invalid Dolby AC-4 presentation_level: " << elem[1];
    return false;
  }

  if (bitstream_versionc) {
    *bitstream_versionc = bitstream_version;
  }
  if (presentation_versionc) {
    *presentation_versionc = presentation_version;
  }
  if (presentation_levelc) {
    *presentation_levelc = presentation_level;
  }

  return true;
}
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
}  // namespace media
