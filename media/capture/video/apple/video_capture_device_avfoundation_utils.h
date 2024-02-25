// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_APPLE_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_UTILS_H_
#define MEDIA_CAPTURE_VIDEO_APPLE_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_UTILS_H_

#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>

#include "base/feature_list.h"
#include "media/capture/video/apple/video_capture_device_apple.h"
#include "media/capture/video/video_capture_device_descriptor.h"
#include "media/capture/video_capture_types.h"

#if BUILDFLAG(IS_IOS)
#import <UIKit/UIKit.h>
#endif

@class DeviceNameAndTransportType;

namespace media {

std::string CAPTURE_EXPORT MacFourCCToString(OSType fourcc);

// Extracts |base_address| and |length| out of a SampleBuffer. Returns true on
// success and false if we failed to retrieve the information due to OS call
// error return, or unexpected output parameters.
[[nodiscard]] bool ExtractBaseAddressAndLength(char** base_address,
                                               size_t* length,
                                               CMSampleBufferRef sample_buffer);

gfx::Size CAPTURE_EXPORT GetPixelBufferSize(CVPixelBufferRef pixel_buffer);
gfx::Size CAPTURE_EXPORT GetSampleBufferSize(CMSampleBufferRef sample_buffer);

#if BUILDFLAG(IS_IOS)
std::optional<int> MaybeGetVideoRotation(
    UIDeviceOrientation orientation,
    AVCaptureDevicePosition camera_position);
#endif
}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_APPLE_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_UTILS_H_
