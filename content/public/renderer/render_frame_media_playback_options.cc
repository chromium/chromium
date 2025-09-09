// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/render_frame_media_playback_options.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "content/public/common/content_features.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#endif

namespace content {
bool IsBackgroundMediaSuspendEnabled() {
#if BUILDFLAG(IS_ANDROID)
  // For Android devices, do not suspend background media for devices with large
  // displays
  return !base::android::device_info::was_launched_on_large_display();
#else
  // For non-Android devices, always allow background media to play
  return false;
#endif
}
}  // namespace content
