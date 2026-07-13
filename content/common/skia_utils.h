// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SKIA_UTILS_H_
#define CONTENT_COMMON_SKIA_UTILS_H_

#include "content/common/content_export.h"

namespace content {

// Full Skia initialization for processes that do heavy Skia work (renderer,
// GPU, in-process-GPU browser). Configures kill-switches, font caches, etc.
void InitializeSkia();

// Lightweight Skia initialization for processes that don't need full Skia setup
// but still need kill-switches and diagnostics (e.g. browser process with
// out-of-process GPU). Configures ICC/EXIF kill-switches, event tracing, and
// memory dump providers.
void InitializeSkiaLite();

// Returns whether one of the InitializeSkia* functions above has run in this
// process. Exposed for tests that need to verify per-process initialization.
CONTENT_EXPORT bool IsSkiaInitializedForTesting();

}  // namespace content

#endif  // CONTENT_COMMON_SKIA_UTILS_H_
