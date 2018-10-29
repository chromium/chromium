// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MIME_UTIL_H_
#define MEDIA_BASE_MIME_UTIL_H_

#include <string>
#include <vector>

#include "media/base/audio_codecs.h"
#include "media/base/media_export.h"
#include "media/base/video_codecs.h"

namespace media {

// Check to see if a particular MIME type is in the list of
// supported/recognized MIME types.
MEDIA_EXPORT bool IsSupportedMediaMimeType(const std::string& mime_type);

// Splits |codecs| separated by comma into |codecs_out|. Codecs in |codecs| may
// or may not be quoted. For example, "\"aaa.b.c,dd.eee\"" and "aaa.b.c,dd.eee"
// will both be split into {"aaa.b.c", "dd.eee"}.
// See http://www.ietf.org/rfc/rfc4281.txt.
MEDIA_EXPORT void SplitCodecs(const std::string& codecs,
                              std::vector<std::string>* codecs_out);

// Strips the profile and level info from |codecs| in place.  For example,
// {"aaa.b.c", "dd.eee"} will be strip into {"aaa", "dd"}.
// See http://www.ietf.org/rfc/rfc4281.txt.
MEDIA_EXPORT void StripCodecs(std::vector<std::string>* codecs);

// Returns true if successfully parsed the given |mime_type| and |codec_id|,
// setting |out_*| arguments to the parsed video codec, profile, and level.
// |out_is_ambiguous| will be true when the codec string is incomplete such that
// some guessing was required to decide the codec, profile, or level.
// Returns false if parsing fails (invalid string, or unrecognized video codec),
// in which case values for |out_*| arguments are undefined.
MEDIA_EXPORT bool ParseVideoCodecString(const std::string& mime_type,
                                        const std::string& codec_id,
                                        bool* out_is_ambiguous,
                                        VideoCodec* out_codec,
                                        VideoCodecProfile* out_profile,
                                        uint8_t* out_level,
                                        VideoColorSpace* out_colorspace);

// Returns true if successfully parsed the given |mime_type| and |codec_id|,
// setting |out_audio_codec| to found codec. |out_is_ambiguous| will be true
// when the codec string is incomplete such that some guessing was required to
// decide the codec.
// Returns false if parsing fails (invalid string, or unrecognized audio codec),
// in which case values for |out_*| arguments are undefined.
MEDIA_EXPORT bool ParseAudioCodecString(const std::string& mime_type,
                                        const std::string& codec_id,
                                        bool* out_is_ambiguous,
                                        AudioCodec* out_codec);

// Indicates that the MIME type and (possible codec string) are supported.
enum SupportsType {
  // The given MIME type and codec combination is not supported.
  IsNotSupported,

  // The given MIME type and codec combination is supported.
  IsSupported,

  // There's not enough information to determine if the given MIME type and
  // codec combination can be rendered or not before actually trying to play it.
  MayBeSupported
};

// Checks the |mime_type| and |codecs| against the MIME types known to support
// only a particular subset of codecs.
// * Returns IsSupported if the |mime_type| is supported and all the codecs
//   within the |codecs| are supported for the |mime_type|.
// * Returns MayBeSupported if the |mime_type| is supported and is known to
//   support only a subset of codecs, but |codecs| was empty. Also returned if
//   all the codecs in |codecs| are supported, but additional codec parameters
//   were supplied (such as profile) for which the support cannot be decided.
// * Returns IsNotSupported if either the |mime_type| is not supported or the
//   |mime_type| is supported but at least one of the codecs within |codecs| is
//   not supported for the |mime_type|.
MEDIA_EXPORT SupportsType
IsSupportedMediaFormat(const std::string& mime_type,
                       const std::vector<std::string>& codecs);

// Similar to the above, but for encrypted formats.
MEDIA_EXPORT SupportsType
IsSupportedEncryptedMediaFormat(const std::string& mime_type,
                                const std::vector<std::string>& codecs);

}  // namespace media

#endif  // MEDIA_BASE_MIME_UTIL_H_
