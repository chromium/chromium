// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mac/video_capture_device_avfoundation_helpers.h"

#include "build/build_config.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace media {

NSArray<AVCaptureDevice*>* GetVideoCaptureDevices() {
  // Query for all camera device types available on apple platform. The
  // others in the enum are only supported on iOS/iPadOS.
  NSArray* captureDeviceType = @[
    AVCaptureDeviceTypeBuiltInWideAngleCamera,
#if BUILDFLAG(IS_MAC)
    AVCaptureDeviceTypeExternalUnknown
#endif
  ];

  AVCaptureDeviceDiscoverySession* deviceDiscoverySession =
      [AVCaptureDeviceDiscoverySession
          discoverySessionWithDeviceTypes:captureDeviceType
                                mediaType:AVMediaTypeVideo
                                 position:AVCaptureDevicePositionUnspecified];
  return deviceDiscoverySession.devices;
}

}  // namespace media
