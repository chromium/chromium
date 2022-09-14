// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_CAPTURE_SWITCHES_H_
#define MEDIA_CAPTURE_CAPTURE_SWITCHES_H_

#include "media/capture/capture_export.h"

namespace switches {

CAPTURE_EXPORT extern const char kVideoCaptureUseGpuMemoryBuffer[];
CAPTURE_EXPORT extern const char kDisableVideoCaptureUseGpuMemoryBuffer[];

CAPTURE_EXPORT bool IsVideoCaptureUseGpuMemoryBufferEnabled();

}  // namespace switches

#endif  // MEDIA_CAPTURE_CAPTURE_SWITCHES_H_
