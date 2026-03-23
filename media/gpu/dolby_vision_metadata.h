// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_DOLBY_VISION_METADATA_H_
#define MEDIA_GPU_DOLBY_VISION_METADATA_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/containers/span.h"
#include "base/time/time.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

struct MEDIA_GPU_EXPORT DolbyVisionMetadata {
  DolbyVisionMetadata();
  DolbyVisionMetadata(const DolbyVisionMetadata&);
  DolbyVisionMetadata(DolbyVisionMetadata&&);
  DolbyVisionMetadata& operator=(const DolbyVisionMetadata&);
  DolbyVisionMetadata& operator=(DolbyVisionMetadata&&);
  ~DolbyVisionMetadata();

  static DolbyVisionMetadata FromRaw(base::span<const uint8_t> data,
                                     base::TimeDelta timestamp);
  static DolbyVisionMetadata FromH265(base::span<const uint8_t> data,
                                      base::TimeDelta timestamp);

  std::vector<uint8_t> data;
  base::TimeDelta timestamp;
};

}  // namespace media

#endif  // MEDIA_GPU_DOLBY_VISION_METADATA_H_
