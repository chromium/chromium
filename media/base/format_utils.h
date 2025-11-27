// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_FORMAT_UTILS_H_
#define MEDIA_BASE_FORMAT_UTILS_H_

#include <optional>

#include "components/viz/common/resources/shared_image_format.h"
#include "media/base/media_export.h"
#include "media/base/video_types.h"

namespace media {

MEDIA_EXPORT std::optional<VideoPixelFormat>
SharedImageFormatToVideoPixelFormat(viz::SharedImageFormat format);

MEDIA_EXPORT std::optional<viz::SharedImageFormat>
VideoPixelFormatToSharedImageFormat(VideoPixelFormat pixel_format);

}  // namespace media

#endif  // MEDIA_BASE_FORMAT_UTILS_H_
