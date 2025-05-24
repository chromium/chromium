// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mac/video_capture_device_avfoundation_helpers.h"

#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "build/build_config.h"

namespace media {

#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kUseAVCaptureDeviceTypeContinuity,
             "UseAVCaptureDeviceTypeContinuity",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_MAC)

NSArray<AVCaptureDevice*>* GetVideoCaptureDevices() {
  // Camera device types available on all apple platforms.
  NSArray* captureDeviceTypes = @[ AVCaptureDeviceTypeBuiltInWideAngleCamera ];

#if BUILDFLAG(IS_MAC)
  // MacOS has an additional 'external' device type we want to include.
  // AVCaptureDeviceTypeExternal since 14.0, AVCaptureDeviceTypeExternalUnknown
  // before. See crbug.com/1484830.
  if (@available(macOS 14.0, *)) {
    captureDeviceTypes =
        [captureDeviceTypes arrayByAddingObject:AVCaptureDeviceTypeExternal];
    // Continuity cameras are available from MacOS 14.0 and also have to
    // be queried.
    if (base::FeatureList::IsEnabled(kUseAVCaptureDeviceTypeContinuity)) {
      captureDeviceTypes = [captureDeviceTypes
          arrayByAddingObject:AVCaptureDeviceTypeContinuityCamera];
    }
  } else {
    captureDeviceTypes = [captureDeviceTypes
        arrayByAddingObject:AVCaptureDeviceTypeExternalUnknown];
  }
#endif  // BUILDFLAG(IS_MAC)

  @try {
    AVCaptureDeviceDiscoverySession* deviceDiscoverySession =
        [AVCaptureDeviceDiscoverySession
            discoverySessionWithDeviceTypes:captureDeviceTypes
                                  mediaType:AVMediaTypeVideo
                                   position:AVCaptureDevicePositionUnspecified];
    return deviceDiscoverySession.devices;
  } @catch (NSException* exception) {
    SCOPED_CRASH_KEY_STRING1024("AVCaptureDeviceCrash", "Exception_name",
                                exception.name.UTF8String);
    SCOPED_CRASH_KEY_STRING1024("AVCaptureDeviceCrash", "Exception_reason",
                                exception.reason.UTF8String);

    base::debug::DumpWithoutCrashing();

    // Return empty array when catching exception.
    return @[];
  }
}

}  // namespace media
