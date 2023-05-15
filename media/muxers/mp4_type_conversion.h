// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MUXERS_MP4_TYPE_CONVERSION_H_
#define MEDIA_MUXERS_MP4_TYPE_CONVERSION_H_

#include "base/strings/string_piece.h"
#include "media/base/media_export.h"

namespace media {

// Convert to ISO time, seconds since midnight, Jan. 1, 1904, in UTC time.
// base::Time time1904;
// base::Time::FromUTCString("1904-01-01 00:00:00 UTC", &time1904);
// 9561628800 = time1904.ToDeltaSinceWindowsEpoch().InSeconds();
static constexpr int64_t k1601To1904DeltaInSeconds = INT64_C(9561628800);

static constexpr uint16_t kUndefinedLanguageCode =
    0x55C4;  // "und" on ISO 639-2/T.

uint16_t MEDIA_EXPORT
ConvertIso639LanguageCodeToU16(const base::StringPiece input_language);

}  // namespace media

#endif  // MEDIA_MUXERS_MP4_TYPE_CONVERSION_H_
