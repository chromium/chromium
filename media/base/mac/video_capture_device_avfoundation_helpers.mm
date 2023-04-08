// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mac/video_capture_device_avfoundation_helpers.h"

#include "base/notreached.h"
#include "build/build_config.h"

namespace media {

// Use an AVCaptureDeviceDiscoverySession for enumerating cameras, instead of
// the deprecated [AVDeviceCapture devices].
NSArray<AVCaptureDevice*>* GetVideoCaptureDevices() {
  if (@available(macOS 10.15, iOS 10.0, *)) {
    // Query for all camera device types available on apple platform. The others
    // in the enum are only supported on iOS/iPadOS.
    NSArray* captureDeviceType = @[
      AVCaptureDeviceTypeBuiltInWideAngleCamera,
#if BUILDFLAG(IS_MAC)
      AVCaptureDeviceTypeExternalUnknown
#endif
    ];

    AVCaptureDeviceDiscoverySession* deviceDescoverySession =
        [AVCaptureDeviceDiscoverySession
            discoverySessionWithDeviceTypes:captureDeviceType
                                  mediaType:AVMediaTypeVideo
                                   position:AVCaptureDevicePositionUnspecified];
    return deviceDescoverySession.devices;
  }

#if BUILDFLAG(IS_MAC) || (!defined(__IPHONE_10_0) || \
                          __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_10_0)
  return [AVCaptureDevice devices];
#else
  NOTREACHED();
  return nil;
#endif
}

}  // namespace media
