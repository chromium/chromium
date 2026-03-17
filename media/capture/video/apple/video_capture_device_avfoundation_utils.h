// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_APPLE_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_UTILS_H_
#define MEDIA_CAPTURE_VIDEO_APPLE_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_UTILS_H_

#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>

#include <optional>
#include <string>

#include "media/capture/capture_export.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_IOS)
#import <AVFoundation/AVFoundation.h>
#import <UIKit/UIKit.h>
#endif

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
