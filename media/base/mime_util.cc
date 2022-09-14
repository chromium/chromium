// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mime_util.h"

#include "base/no_destructor.h"
#include "base/strings/string_piece.h"
#include "media/base/mime_util_internal.h"

namespace media {

// This variable is Leaky because it is accessed from WorkerPool threads.
static const internal::MimeUtil* GetMimeUtil() {
  static const base::NoDestructor<internal::MimeUtil> mime_util;
  return &(*mime_util);
}

bool IsSupportedMediaMimeType(base::StringPiece mime_type) {
  return GetMimeUtil()->IsSupportedMediaMimeType(mime_type);
}

SupportsType IsSupportedMediaFormat(base::StringPiece mime_type,
                                    const std::vector<std::string>& codecs) {
  return GetMimeUtil()->IsSupportedMediaFormat(mime_type, codecs, false);
}

SupportsType IsSupportedEncryptedMediaFormat(
    base::StringPiece mime_type,
    const std::vector<std::string>& codecs) {
  return GetMimeUtil()->IsSupportedMediaFormat(mime_type, codecs, true);
}

void SplitCodecs(base::StringPiece codecs,
                 std::vector<std::string>* codecs_out) {
  GetMimeUtil()->SplitCodecs(codecs, codecs_out);
}

void StripCodecs(std::vector<std::string>* codecs) {
  GetMimeUtil()->StripCodecs(codecs);
}

bool ParseVideoCodecString(base::StringPiece mime_type,
                           base::StringPiece codec_id,
                           bool* ambiguous_codec_string,
                           VideoCodec* out_codec,
                           VideoCodecProfile* out_profile,
                           uint8_t* out_level,
                           VideoColorSpace* out_colorspace) {
  return GetMimeUtil()->ParseVideoCodecString(
      mime_type, codec_id, ambiguous_codec_string, out_codec, out_profile,
      out_level, out_colorspace);
}

bool ParseAudioCodecString(base::StringPiece mime_type,
                           base::StringPiece codec_id,
                           bool* ambiguous_codec_string,
                           AudioCodec* out_codec) {
  return GetMimeUtil()->ParseAudioCodecString(
      mime_type, codec_id, ambiguous_codec_string, out_codec);
}

}  // namespace media
