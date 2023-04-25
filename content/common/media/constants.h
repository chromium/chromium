// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEDIA_CONSTANTS_H_
#define CONTENT_COMMON_MEDIA_CONSTANTS_H_

#include "build/build_config.h"

namespace content {

// Default value for
// RenderFrameMediaPlaybackOptions::is_background_suspend_enabled is determined
// statically in Chromium, but some content embedders (e.g. Cast) may need to
// change it at runtime.
#if BUILDFLAG(IS_ANDROID)
const bool kIsBackgroundMediaSuspendEnabled = true;
#else
const bool kIsBackgroundMediaSuspendEnabled = false;
#endif

}  // namespace content

#endif  // CONTENT_COMMON_MEDIA_CONSTANTS_H_
