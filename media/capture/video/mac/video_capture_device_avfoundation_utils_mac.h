// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_UTILS_MAC_H_
#define MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_UTILS_MAC_H_

#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>

#include "base/feature_list.h"
#include "base/mac/scoped_nsobject.h"
#include "media/capture/video/video_capture_device_descriptor.h"
#include "media/capture/video_capture_types.h"

namespace media {

std::string CAPTURE_EXPORT MacFourCCToString(OSType fourcc);

// Returns a dictionary of capture devices with friendly name and unique id.
// VideoCaptureDeviceMac should call this function to fetch the list of devices
// available in the system; this method returns the list of device names that
// have to be used with -[VideoCaptureDeviceAVFoundation setCaptureDevice:].
base::scoped_nsobject<NSDictionary> GetVideoCaptureDeviceNames();

// Extracts |base_address| and |length| out of a SampleBuffer. Returns true on
// success and false if we failed to retrieve the information due to OS call
// error return, or unexpected output parameters.
[[nodiscard]] bool ExtractBaseAddressAndLength(char** base_address,
                                               size_t* length,
                                               CMSampleBufferRef sample_buffer);

gfx::Size CAPTURE_EXPORT GetPixelBufferSize(CVPixelBufferRef pixel_buffer);
gfx::Size CAPTURE_EXPORT GetSampleBufferSize(CMSampleBufferRef sample_buffer);

// When enabled, we use an AVCaptureDeviceDiscoverySession for enumerating
// cameras, instead of the deprecated [AVDeviceCapture devices].
CAPTURE_EXPORT BASE_DECLARE_FEATURE(kUseAVCaptureDeviceDiscoverySession);

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_UTILS_MAC_H_
