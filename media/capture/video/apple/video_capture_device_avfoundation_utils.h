// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_APPLE_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_UTILS_H_
#define MEDIA_CAPTURE_VIDEO_APPLE_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_UTILS_H_

#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>

#include <optional>
#include <string>

#include "base/containers/span.h"
#include "media/capture/capture_export.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_IOS)
#import <AVFoundation/AVFoundation.h>
#import <UIKit/UIKit.h>
#endif

namespace media {

std::string CAPTURE_EXPORT MacFourCCToString(OSType fourcc);

// Extracts base address and length out of a SampleBuffer as a span. Returns
// std::nullopt on OS call failure or if the buffer is not contiguous.
[[nodiscard]] std::optional<base::span<const uint8_t>> ExtractDataSpan(
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
