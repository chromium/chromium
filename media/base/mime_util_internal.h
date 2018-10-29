// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MIME_UTIL_INTERNAL_H_
#define MEDIA_BASE_MIME_UTIL_INTERNAL_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "media/base/media_export.h"
#include "media/base/mime_util.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"

namespace media {

namespace internal {

// Internal utility class for handling mime types.  Should only be invoked by
// tests and the functions within mime_util.cc -- NOT for direct use by others.
class MEDIA_EXPORT MimeUtil {
 public:
  MimeUtil();
  ~MimeUtil();

  enum Codec {
    INVALID_CODEC,
    PCM,
    MP3,
    AC3,
    EAC3,
    MPEG2_AAC,
    MPEG4_AAC,
    VORBIS,
    OPUS,
    FLAC,
    H264,
    HEVC,
    VP8,
    VP9,
    THEORA,
    DOLBY_VISION,
    AV1,
    MPEG_H_AUDIO,
    LAST_CODEC = MPEG_H_AUDIO
  };

  // Platform configuration structure.  Controls which codecs are supported at
  // runtime.  Also used by tests to simulate platform differences.
  struct PlatformInfo {
    bool has_platform_decoders = false;

    bool has_platform_vp8_decoder = false;
    bool has_platform_vp9_decoder = false;
    bool supports_opus = false;
  };

  struct ParsedCodecResult {
    Codec codec;
    bool is_ambiguous;
    VideoCodecProfile video_profile;
    uint8_t video_level;
    VideoColorSpace video_color_space;
  };

  // See mime_util.h for more information on these methods.
  bool IsSupportedMediaMimeType(const std::string& mime_type) const;
  void SplitCodecs(const std::string& codecs,
                   std::vector<std::string>* codecs_out);
  void StripCodecs(std::vector<std::string>* codecs);
  bool ParseVideoCodecString(const std::string& mime_type,
                             const std::string& codec_id,
                             bool* out_is_ambiguous,
                             VideoCodec* out_codec,
                             VideoCodecProfile* out_profile,
                             uint8_t* out_level,
                             VideoColorSpace* out_color_space);
  bool ParseAudioCodecString(const std::string& mime_type,
                             const std::string& codec_id,
                             bool* out_is_ambiguous,
                             AudioCodec* out_codec);
  SupportsType IsSupportedMediaFormat(const std::string& mime_type,
                                      const std::vector<std::string>& codecs,
                                      bool is_encrypted) const;

  // Checks android platform specific codec restrictions. Returns true if
  // |codec| is supported when contained in |mime_type_lower_case|.
  // |is_encrypted| means the codec will be used with encrypted blocks.
  // |platform_info| describes the availability of various platform features;
  // see PlatformInfo for more details.
  static bool IsCodecSupportedOnAndroid(Codec codec,
                                        const std::string& mime_type_lower_case,
                                        bool is_encrypted,
                                        const PlatformInfo& platform_info);

 private:
  typedef base::flat_set<int> CodecSet;
  typedef base::flat_map<std::string, CodecSet> MediaFormatMappings;

  // Initializes the supported media types into hash sets for faster lookup.
  void InitializeMimeTypeMaps();

  // Initializes the supported media formats (|media_format_map_|).
  void AddSupportedMediaFormats();

  // Adds |mime_type| with the specified codecs to |media_format_map_|.
  void AddContainerWithCodecs(const std::string& mime_type,
                              const CodecSet& codecs_list,
                              bool is_proprietary_mime_type);

  // Returns IsSupported if all codec IDs in |codecs| are unambiguous and are
  // supported in |mime_type_lower_case|. MayBeSupported is returned if at least
  // one codec ID in |codecs| is ambiguous but all the codecs are supported.
  // IsNotSupported is returned if |mime_type_lower_case| is not supported or at
  // least one is not supported in |mime_type_lower_case|. |is_encrypted| means
  // the codec will be used with encrypted blocks.
  SupportsType AreSupportedCodecs(
      const std::vector<ParsedCodecResult>& parsed_codecs,
      const std::string& mime_type_lower_case,
      bool is_encrypted) const;

  // Parse the combination of |mime_type_lower_case| and |codecs|. Returns true
  // when parsing succeeds and output is written to |out_results|. Returns false
  // when parsing fails. Failure may be caused by
  //  - invalid/unrecognized codec strings and mime_types
  //  - invalid combinations of codec strings and mime_types (e.g. H264 in WebM)
  // See comment for ParseCodecHelper().
  bool ParseCodecStrings(const std::string& mime_type_lower_case,
                         const std::vector<std::string>& codecs,
                         std::vector<ParsedCodecResult>* out_results) const;

  // Helper to ParseCodecStrings(). Parses a single |codec_id| with
  // |mime_type_lower_case| to populate the fields of |out_result|. This helper
  // method does not validate the combination of |mime_type_lower_case| and
  // |codec_id|, nor does it handle empty/unprovided codecs; See caller
  // ParseCodecStrings().
  //
  // |out_result| is only valid when this method returns true (parsing success).
  // |out_result->is_ambiguous| will be set to true when the codec string
  // matches one of a fixed number of *non-RFC compliant* strings (e.g. "avc").
  // Ambiguous video codec strings may fail to provide video profile and/or
  // level info. In these cases, we use the following values to indicate
  // "unspecified":
  //  - out_result->video_profile = VIDEO_CODEC_PROFILE_UNKNOWN
  //  - out_result->video_level = 0
  //
  // For unambiguous video codecs, |video_profile| and |video_level| will be
  // set in |out_result|.
  //
  // |out_result|'s |video_color_space| will report the codec strings color
  // space when provided. Most codec strings do not yet describe color, so this
  // will often be set to the default of REC709.
  bool ParseCodecHelper(const std::string& mime_type_lower_case,
                        const std::string& codec_id,
                        ParsedCodecResult* out_result) const;

  // Returns IsSupported if |codec| when platform supports codec contained in
  // |mime_type_lower_case|. Returns MayBeSupported when platform support is
  // unclear. Otherwise returns NotSupported. Note: This method will always
  // return NotSupported for proprietary codecs if |allow_proprietary_codecs_|
  // is set to false. |is_encrypted| means the codec will be used with encrypted
  // blocks.
  // TODO(chcunningham): Make this method return a bool. Platform support should
  // always be knowable for a fully specified codec.
  SupportsType IsCodecSupported(const std::string& mime_type_lower_case,
                                Codec codec,
                                VideoCodecProfile video_profile,
                                uint8_t video_level,
                                const VideoColorSpace& eotf,
                                bool is_encrypted) const;

  // Returns true if |codec| refers to a proprietary codec.
  bool IsCodecProprietary(Codec codec) const;

  // Returns true and sets |*default_codec| if |mime_type_lower_case| has a
  // default codec associated with it. Returns false otherwise and the value of
  // |*default_codec| is undefined.
  bool GetDefaultCodec(const std::string& mime_type_lower_case,
                       Codec* default_codec) const;

#if defined(OS_ANDROID)
  // Indicates the support of various codecs within the platform.
  PlatformInfo platform_info_;
#endif

  // A map of mime_types and hash map of the supported codecs for the mime_type.
  MediaFormatMappings media_format_map_;

  // List of proprietary containers in |media_format_map_|.
  std::vector<std::string> proprietary_media_containers_;
  // Whether proprietary codec support should be advertised to callers.
  bool allow_proprietary_codecs_;

  DISALLOW_COPY_AND_ASSIGN(MimeUtil);
};

}  // namespace internal
}  // namespace media

#endif  // MEDIA_BASE_MIME_UTIL_INTERNAL_H_
