// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_RESOLUTION_MONITOR_H_
#define MEDIA_FILTERS_RESOLUTION_MONITOR_H_

#include <memory>

#include "base/sequence_checker.h"
#include "media/base/media_export.h"
#include "media/base/video_codecs.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class DecoderBuffer;

// Resolution monitor acquires a resolution of DecoderBuffer by parsing it. We
// can know the stream resolution before the first keyframe is decoded. This
// avoids requesting a sender to produce a keyframe again when a software
// decoder fallback due to a stream resolution happens.
class MEDIA_EXPORT ResolutionMonitor {
 public:
  virtual ~ResolutionMonitor();

  static std::unique_ptr<ResolutionMonitor> Create(VideoCodec codec);

  virtual absl::optional<gfx::Size> GetResolution(
      const DecoderBuffer& buffer) = 0;
  virtual VideoCodec codec() const = 0;
};
}  // namespace media
#endif  // MEDIA_FILTERS_RESOLUTION_MONITOR_H_
