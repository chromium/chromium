// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MAC_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_HELPERS_H_
#define MEDIA_BASE_MAC_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_HELPERS_H_

#import <AVFoundation/AVFoundation.h>

#include "media/base/media_export.h"

namespace media {

// Use an AVCaptureDeviceDiscoverySession for enumerating cameras, instead of
// the deprecated [AVDeviceCapture devices].
MEDIA_EXPORT NSArray<AVCaptureDevice*>* GetVideoCaptureDevices(
    bool use_discovery_session);

}  // namespace media

#endif  // MEDIA_BASE_MAC_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_HELPERS_H_
