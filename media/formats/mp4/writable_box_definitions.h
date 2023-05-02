// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP4_WRITABLE_BOX_DEFINITIONS_H_
#define MEDIA_FORMATS_MP4_WRITABLE_BOX_DEFINITIONS_H_

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

// `mvhd` box.
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

// `moov` box.
struct Movie : Box {
  MovieHeader header;
};

}  // namespace media::mp4::writable_boxes

#endif  // MEDIA_FORMATS_MP4_WRITABLE_BOX_DEFINITIONS_H_
