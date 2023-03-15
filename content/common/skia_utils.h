// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SKIA_UTILS_H_
#define CONTENT_COMMON_SKIA_UTILS_H_

namespace content {

// Common utility code for skia initialization done in the renderer process, and
// also in the GPU process for viz/oop-r which runs skia in the GPU process.
void InitializeSkia();

// Temporary utility to migrate tests to Skia's analytic antialiasing algorithm.
// The GPU and renderer processes call InitializeSkia, which does this
// internally. Browser tests that run in the browser process might not, so this
// is called during browser set up.
// https://crbug.com/1421297
void InitializeSkiaAnalyticAntialiasing();

}  // namespace content

#endif  // CONTENT_COMMON_SKIA_UTILS_H_
