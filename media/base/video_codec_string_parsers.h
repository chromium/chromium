// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_CODEC_STRING_PARSERS_H_
#define MEDIA_BASE_VIDEO_CODEC_STRING_PARSERS_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <string_view>

#include "media/base/media_export.h"
#include "media/base/media_types.h"

namespace media {

// ParseNewStyleVp9CodecID handles parsing of new style vp9 codec IDs per
// proposed VP Codec ISO Media File Format Binding specification:
// https://storage.googleapis.com/downloads.webmproject.org/docs/vp9/vp-codec-iso-media-file-format-binding-20160516-draft.pdf
// ParseLegacyVp9CodecID handles parsing of legacy VP9 codec strings defined
// for WebM.
MEDIA_EXPORT std::optional<VideoType> ParseNewStyleVp9CodecID(
    std::string_view codec_id);

MEDIA_EXPORT std::optional<VideoType> ParseLegacyVp9CodecID(
    std::string_view codec_id);

MEDIA_EXPORT std::optional<VideoType> ParseAv1CodecId(
    std::string_view codec_id);

// Handle parsing AVC/H.264 codec ids as outlined in RFC 6381 and ISO-14496-10.
MEDIA_EXPORT std::optional<VideoType> ParseAVCCodecId(
    std::string_view codec_id);

MEDIA_EXPORT std::optional<VideoType> ParseHEVCCodecId(
    std::string_view codec_id);

MEDIA_EXPORT std::optional<VideoType> ParseVVCCodecId(
    std::string_view codec_id);

MEDIA_EXPORT std::optional<VideoType> ParseDolbyVisionCodecId(
    std::string_view codec_id);

MEDIA_EXPORT std::optional<VideoType> ParseCodec(std::string_view codec_id);

MEDIA_EXPORT VideoCodec StringToVideoCodec(std::string_view codec_id);

// Translate legacy avc1 codec ids (like avc1.66.30 or avc1.77.31) into a new
// style standard avc1 codec ids like avc1.4D002F. If the input codec is not
// recognized as a legacy codec id, then returns the input string unchanged.
std::string TranslateLegacyAvc1CodecIds(std::string_view codec_id);

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_CODEC_STRING_PARSERS_H_
