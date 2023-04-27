// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SKIA_UTILS_H_
#define CONTENT_COMMON_SKIA_UTILS_H_

namespace content {

// Common utility code for skia initialization done in the renderer process, and
// also in the GPU process for viz/oop-r which runs skia in the GPU process.
void InitializeSkia();

}  // namespace content

#endif  // CONTENT_COMMON_SKIA_UTILS_H_
