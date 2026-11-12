// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_SYMPHONIA_DEMUXER_UTIL_H_
#define MEDIA_FILTERS_SYMPHONIA_DEMUXER_UTIL_H_

#include <string_view>

#include "media/base/audio_codecs.h"
#include "media/base/container_names.h"
#include "media/base/media_export.h"
#include "media/base/sample_format.h"
#include "media/filters/symphonia_demuxer_bridge.rs.h"

// Maps values reported by the `symphonia_demuxer_bridge` CXX bridge to their
// Chromium media equivalents.
//
// TODO(crbug.com/550619039): Have the demuxer and decoder bridges share one
// codec enum, so that these mappings and `SymphoniaAudioDecoder`'s reverse
// mapping can live together in one place.

namespace media {

// Returns the `AudioCodec` for `codec`, or `AudioCodec::kUnknown` for video
// codecs and codecs that Chromium does not support.
MEDIA_EXPORT AudioCodec SymphoniaCodecToAudioCodec(SymphoniaCodec codec);

// Returns the sample format that `codec` decodes to, or `kUnknownSampleFormat`
// for video codecs and unknown codecs.
MEDIA_EXPORT SampleFormat SymphoniaCodecToSampleFormat(SymphoniaCodec codec);

// Maps a Symphonia `FormatInfo::short_name` to a `MediaContainerName` for
// metrics. The names must match the format readers exactly; for example, the
// Matroska reader reports "matroska" for both MKV and WebM.
MEDIA_EXPORT container_names::MediaContainerName
SymphoniaFormatNameToContainerName(std::string_view name);

}  // namespace media

#endif  // MEDIA_FILTERS_SYMPHONIA_DEMUXER_UTIL_H_
