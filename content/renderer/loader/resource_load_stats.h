// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_LOADER_RESOURCE_LOAD_STATS_H_
#define CONTENT_RENDERER_LOADER_RESOURCE_LOAD_STATS_H_

#include "build/build_config.h"

namespace content {

#if defined(OS_ANDROID)
void NotifyUpdateUserGestureCarryoverInfo(int render_frame_id);
#endif

}  // namespace content

#endif  // CONTENT_RENDERER_LOADER_RESOURCE_LOAD_STATS_H_
