// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MAC_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_HELPERS_H_
#define MEDIA_BASE_MAC_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_HELPERS_H_

#import <AVFoundation/AVFoundation.h>

#include "media/base/media_export.h"

namespace media {

// Enumerate available cameras.
MEDIA_EXPORT NSArray<AVCaptureDevice*>* GetVideoCaptureDevices();

}  // namespace media

#endif  // MEDIA_BASE_MAC_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_HELPERS_H_
