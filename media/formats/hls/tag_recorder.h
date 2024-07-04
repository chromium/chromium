// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_TAG_RECORDER_H_
#define MEDIA_FORMATS_HLS_TAG_RECORDER_H_

#include <stdint.h>

#include "media/base/media_export.h"

namespace media::hls {

// A TagRecorder is used to measure frequenceies of present tags and their
// corresponding attributes inside playlists.
class MEDIA_EXPORT TagRecorder {
 public:
  enum class Metric {
    // Segments from a media playlist.
    kSegmentTS,
    kSegmentMP4,
    kSegmentAAC,
    kSegmentOther,

    // Presence of specific tags:
    kContentSteering,
    kDiscontinuity,
    kDiscontinuitySequence,
    kGap,
    kPart,
    kSkip,
    kUnknownTag,
    kKey,
    kSessionKey,

    // Cryptography methods
    kSegmentAES,
    kSample,
    kNoCrypto,
    kAESCTR,
    kAESCENC,
    kISO230017
  };

  virtual ~TagRecorder() = 0;
  virtual void SetMetric(Metric metric) = 0;
  virtual void RecordError(uint32_t err_code) = 0;
};

}  // namespace media::hls

#endif  // MEDIA_FORMATS_HLS_TAG_RECORDER_H_
