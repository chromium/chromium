// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_DRM_KEY_TYPE_H_
#define MEDIA_BASE_MEDIA_DRM_KEY_TYPE_H_

#include <stdint.h>

namespace media {

// These must be in sync with Android MediaDrm KEY_TYPE_XXX constants, except
// UNKNOWN and MAX:
// https://developer.android.com/reference/android/media/MediaDrm.html#KEY_TYPE_OFFLINE
enum class MediaDrmKeyType : uint32_t {
  UNKNOWN = 0,
  MIN = UNKNOWN,
  STREAMING = 1,
  OFFLINE = 2,
  RELEASE = 3,
  MAX = RELEASE,
};

}  // namespace media
#endif  // MEDIA_BASE_MEDIA_DRM_KEY_TYPE_H_
