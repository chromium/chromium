// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/apple/video_capture_device_avfoundation_utils.h"

#include "base/mac/mac_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "media/base/mac/video_capture_device_avfoundation_helpers.h"
#include "media/base/media_switches.h"
#include "media/capture/video/apple/video_capture_device_apple.h"
#include "media/capture/video/apple/video_capture_device_avfoundation.h"
#include "media/capture/video/apple/video_capture_device_factory_apple.h"
#include "media/capture/video_capture_types.h"

#if BUILDFLAG(IS_MAC)
#import <IOKit/audio/IOAudioTypes.h>
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

#if BUILDFLAG(IS_IOS)
std::optional<int> MaybeGetVideoRotation(
    UIDeviceOrientation orientation,
    AVCaptureDevicePosition camera_position) {
  bool is_front_camera = NO;
  if (camera_position == AVCaptureDevicePositionFront) {
    is_front_camera = YES;
  } else if (camera_position == AVCaptureDevicePositionBack) {
    is_front_camera = NO;
  }

  std::optional<int> rotation;
  switch (orientation) {
    case UIDeviceOrientationPortrait:
      rotation = 90;
      break;
    case UIDeviceOrientationPortraitUpsideDown:
      rotation = 270;
      break;
    case UIDeviceOrientationLandscapeLeft:
      rotation = is_front_camera ? 180 : 0;
      break;
    case UIDeviceOrientationLandscapeRight:
      rotation = is_front_camera ? 0 : 180;
      break;
    // Don't change video orientation for FaceUp or FaceDown.
    case UIDeviceOrientationFaceUp:
    case UIDeviceOrientationFaceDown:
    case UIDeviceOrientationUnknown:
      break;
  }
  return rotation;
}
#endif

}  // namespace media
