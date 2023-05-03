// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP4_WRITABLE_BOX_DEFINITIONS_H_
#define MEDIA_FORMATS_MP4_WRITABLE_BOX_DEFINITIONS_H_

#include <vector>

#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/formats/mp4/fourccs.h"

namespace media::mp4::writable_boxes {

// Box header without version.
struct Box {
  uint32_t total_size;
  FourCC fourcc;
};

// Box header with version and flags.
struct FullBox : Box {
  // version 1 is 64 bits where applicable, 0 is 32 bits.
  uint8_t version;
  uint32_t flags : 24;
};

// Track Extends (`trex`) box.
struct TrackExtends : FullBox {
  uint32_t track_id;
  uint32_t default_sample_description_index;
  base::TimeDelta default_sample_duration;
  uint32_t default_sample_size;

  // The sample flags field in sample fragments is coded as a 32-bit value.
  // bit(4) reserved=0;
  // unsigned int(2) is_leading;
  // unsigned int(2) sample_depends_on;
  // unsigned int(2) sample_is_depended_on;
  // unsigned int(2) sample_has_redundancy;
  // bit(3) sample_padding_value;
  // bit(1) sample_is_non_sync_sample;
  // unsigned int(16) sample_degradation_priority;
  uint32_t default_sample_flags;
};

// Movie Extends (`mvex`) box.
struct MEDIA_EXPORT MovieExtends : Box {
  MovieExtends();
  ~MovieExtends();
  std::vector<TrackExtends> track_extends;
};

// Movie Header (`mvhd`) box.
struct MEDIA_EXPORT MovieHeader : FullBox {
  MovieHeader();
  ~MovieHeader();

  // It is Windows epoch time so it should be converted to Jan. 1, 1904 UTC
  // before writing. Dates before Jan 1, 1904 UTC will fail / are unsupported.
  base::Time creation_time;
  base::Time modification_time;

  // This is the number of time units that pass in one second.
  uint32_t timescale;
  base::TimeDelta duration;
  uint32_t next_track_id;
};

// Movie (`moov`) box.
struct Movie : Box {
  MovieHeader header;
  MovieExtends extends;
};

}  // namespace media::mp4::writable_boxes

#endif  // MEDIA_FORMATS_MP4_WRITABLE_BOX_DEFINITIONS_H_
