// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/render_frame_media_playback_options.h"

#include "build/android_buildflags.h"
#include "build/build_config.h"

namespace content {

#if BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_DESKTOP_ANDROID)
const bool kIsBackgroundMediaSuspendEnabled = true;
#else
const bool kIsBackgroundMediaSuspendEnabled = false;
#endif

}  // namespace content
