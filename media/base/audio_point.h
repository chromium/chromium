// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_POINT_H_
#define MEDIA_BASE_AUDIO_POINT_H_

#include <string>
#include <vector>

#include "media/base/media_shmem_export.h"
#include "ui/gfx/geometry/point3_f.h"

namespace media {

using Point = gfx::Point3F;

// Returns |points| as a human-readable string. (Not necessarily in the format
// required by ParsePointsFromString).
MEDIA_SHMEM_EXPORT std::string PointsToString(const std::vector<Point>& points);

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_POINT_H_
