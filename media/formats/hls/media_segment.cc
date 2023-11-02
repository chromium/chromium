// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/media_segment.h"

#include "base/time/time.h"
#include "media/formats/hls/types.h"
#include "url/gurl.h"

namespace media::hls {

MediaSegment::InitializationSegment::InitializationSegment(
    GURL uri,
    absl::optional<types::ByteRange> byte_range)
    : uri_(std::move(uri)), byte_range_(byte_range) {}

MediaSegment::InitializationSegment::~InitializationSegment() = default;

MediaSegment::MediaSegment(
    base::TimeDelta duration,
    types::DecimalInteger media_sequence_number,
    types::DecimalInteger discontinuity_sequence_number,
    GURL uri,
    scoped_refptr<InitializationSegment> initialization_segment,
    absl::optional<types::ByteRange> byte_range,
    absl::optional<types::DecimalInteger> bitrate,
    bool has_discontinuity,
    bool is_gap)
    : duration_(duration),
      media_sequence_number_(media_sequence_number),
      discontinuity_sequence_number_(discontinuity_sequence_number),
      uri_(std::move(uri)),
      initialization_segment_(std::move(initialization_segment)),
      byte_range_(byte_range),
      bitrate_(bitrate),
      has_discontinuity_(has_discontinuity),
      is_gap_(is_gap) {}

MediaSegment::~MediaSegment() = default;

}  // namespace media::hls
