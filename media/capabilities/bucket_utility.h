// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPABILITIES_BUCKET_UTILITY_H_
#define MEDIA_CAPABILITIES_BUCKET_UTILITY_H_

#include "media/base/media_export.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// Find the nearest "bucket" with dimensions >= |raw_size|. While smaller
// buckets may more closely describe |raw_size|, the next largest bucket is
// chosen to surface cutoff resolutions in HW-accelerated decoders. Exceeding
// the HW cutoff will invoke the software fallback, giving potentially very
// different decode performance at larger resolutions. Will return an empty
// size if |raw_size| is too small to be bucketed.
MEDIA_EXPORT gfx::Size GetSizeBucket(const gfx::Size& raw_size);

// Round |raw_fps| to the nearest (smaller or larger) "bucket". FrameRates in
// the same bucket should have nearly identical decode performance
// characteristics. Bucketing helps avoid fragmentation of recorded stats.
MEDIA_EXPORT int GetFpsBucket(double raw_fps);

}  // namespace media

#endif  // MEDIA_CAPABILITIES_BUCKET_UTILITY_H_
