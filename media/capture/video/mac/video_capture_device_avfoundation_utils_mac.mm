// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/mac/video_capture_device_avfoundation_utils_mac.h"

#import <IOKit/audio/IOAudioTypes.h>

#include "base/mac/mac_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "media/base/mac/video_capture_device_avfoundation_helpers.h"
#include "media/base/media_switches.h"
#include "media/capture/video/mac/video_capture_device_avfoundation_mac.h"
#include "media/capture/video/mac/video_capture_device_factory_mac.h"
#include "media/capture/video/mac/video_capture_device_mac.h"
#include "media/capture/video_capture_types.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace media {

std::string MacFourCCToString(OSType fourcc) {
  char arr[] = {static_cast<char>(fourcc >> 24),
                static_cast<char>(fourcc >> 16), static_cast<char>(fourcc >> 8),
                static_cast<char>(fourcc), 0};
  return arr;
}

bool ExtractBaseAddressAndLength(char** base_address,
                                 size_t* length,
                                 CMSampleBufferRef sample_buffer) {
  CMBlockBufferRef block_buffer = CMSampleBufferGetDataBuffer(sample_buffer);
  DCHECK(block_buffer);

  size_t length_at_offset;
  const OSStatus status = CMBlockBufferGetDataPointer(
      block_buffer, 0, &length_at_offset, length, base_address);
  DCHECK_EQ(noErr, status);
  // Expect the (M)JPEG data to be available as a contiguous reference, i.e.
  // not covered by multiple memory blocks.
  DCHECK_EQ(length_at_offset, *length);
  return status == noErr && length_at_offset == *length;
}

NSDictionary<NSString*, DeviceNameAndTransportType*>*
GetVideoCaptureDeviceNames() {
  // At this stage we already know that AVFoundation is supported and the whole
  // library is loaded and initialised, by the device monitoring.
  NSMutableDictionary<NSString*, DeviceNameAndTransportType*>* device_names =
      [[NSMutableDictionary alloc] init];

  NSArray<AVCaptureDevice*>* devices = GetVideoCaptureDevices(
      base::FeatureList::IsEnabled(media::kUseAVCaptureDeviceDiscoverySession));

  for (AVCaptureDevice* device in devices) {
    if ([device hasMediaType:AVMediaTypeVideo] ||
        [device hasMediaType:AVMediaTypeMuxed]) {
      if (device.suspended) {
        continue;
      }

      // Transport types are defined for Audio devices and reused for video.
      int transport_type = device.transportType;
      VideoCaptureTransportType device_transport_type =
          (transport_type == kIOAudioDeviceTransportTypeBuiltIn ||
           transport_type == kIOAudioDeviceTransportTypeUSB)
              ? VideoCaptureTransportType::MACOSX_USB_OR_BUILT_IN
              : VideoCaptureTransportType::OTHER_TRANSPORT;
      DeviceNameAndTransportType* name_and_transport_type =
          [[DeviceNameAndTransportType alloc]
               initWithName:device.localizedName
              transportType:device_transport_type];
      device_names[device.uniqueID] = name_and_transport_type;
    }
  }
  return device_names;
}

gfx::Size GetPixelBufferSize(CVPixelBufferRef pixel_buffer) {
  return gfx::Size(CVPixelBufferGetWidth(pixel_buffer),
                   CVPixelBufferGetHeight(pixel_buffer));
}

gfx::Size GetSampleBufferSize(CMSampleBufferRef sample_buffer) {
  if (CVPixelBufferRef pixel_buffer =
          CMSampleBufferGetImageBuffer(sample_buffer)) {
    return GetPixelBufferSize(pixel_buffer);
  }
  CMFormatDescriptionRef format_description =
      CMSampleBufferGetFormatDescription(sample_buffer);
  CMVideoDimensions dimensions =
      CMVideoFormatDescriptionGetDimensions(format_description);
  return gfx::Size(dimensions.width, dimensions.height);
}

BASE_FEATURE(kUseAVCaptureDeviceDiscoverySession,
             "UseAVCaptureDeviceDiscoverySession",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace media
