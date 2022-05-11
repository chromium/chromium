// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/media_segment.h"

#include "media/formats/hls/types.h"
#include "url/gurl.h"

namespace media::hls {

MediaSegment::MediaSegment(types::DecimalFloatingPoint duration,
                           types::DecimalInteger media_sequence_number,
                           GURL uri,
                           bool has_discontinuity,
                           bool is_gap)
    : duration_(duration),
      media_sequence_number_(media_sequence_number),
      uri_(std::move(uri)),
      has_discontinuity_(has_discontinuity),
      is_gap_(is_gap) {}
MediaSegment::~MediaSegment() = default;
MediaSegment::MediaSegment(const MediaSegment&) = default;
MediaSegment::MediaSegment(MediaSegment&&) = default;
MediaSegment& MediaSegment::operator=(const MediaSegment&) = default;
MediaSegment& MediaSegment::operator=(MediaSegment&&) = default;

}  // namespace media::hls
