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
    std::optional<types::ByteRange> byte_range)
    : uri_(std::move(uri)), byte_range_(byte_range) {}

MediaSegment::InitializationSegment::~InitializationSegment() = default;

MediaSegment::EncryptionData::EncryptionData(
    GURL uri,
    MediaSegment::EncryptionData::Mode mode,
    MediaSegment::EncryptionData::IVContainer iv,
    bool identity)
    : uri_(std::move(uri)),
      mode_(mode),
      iv_(std::move(iv)),
      identity_(identity) {}

MediaSegment::EncryptionData::~EncryptionData() = default;

MediaSegment::EncryptionData::IVContainer MediaSegment::EncryptionData::GetIV(
    types::DecimalInteger media_sequence_number) const {
  if (identity_ && !iv_.has_value()) {
    return std::make_tuple(0, media_sequence_number);
  }
  return iv_;
}

MediaSegment::MediaSegment(
    base::TimeDelta duration,
    types::DecimalInteger media_sequence_number,
    types::DecimalInteger discontinuity_sequence_number,
    GURL uri,
    scoped_refptr<InitializationSegment> initialization_segment,
    scoped_refptr<EncryptionData> encryption_data,
    std::optional<types::ByteRange> byte_range,
    std::optional<types::DecimalInteger> bitrate,
    bool has_discontinuity,
    bool is_gap,
    bool has_new_init_segment,
    bool has_new_encryption_data)
    : duration_(duration),
      media_sequence_number_(media_sequence_number),
      discontinuity_sequence_number_(discontinuity_sequence_number),
      uri_(std::move(uri)),
      initialization_segment_(std::move(initialization_segment)),
      encryption_data_(std::move(encryption_data)),
      byte_range_(byte_range),
      bitrate_(bitrate),
      has_discontinuity_(has_discontinuity),
      is_gap_(is_gap),
      has_new_init_segment_(has_new_init_segment),
      has_new_encryption_data_(has_new_encryption_data) {}

MediaSegment::~MediaSegment() = default;

}  // namespace media::hls
