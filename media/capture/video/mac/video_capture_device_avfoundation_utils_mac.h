// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_UTILS_MAC_H_
#define MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_UTILS_MAC_H_

#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>

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

// Retrieve the capture supported formats for a given device |descriptor|.
// |implementation| is a class implementing FourCCToChromiumPixelFormat, which
// our VideoCaptureDeviceAVFoundationProtocol implementations do.
media::VideoCaptureFormats GetDeviceSupportedFormats(
    Class implementation,
    const media::VideoCaptureDeviceDescriptor& descriptor);

// Extracts |base_address| and |length| out of a SampleBuffer.
void ExtractBaseAddressAndLength(char** base_address,
                                 size_t* length,
                                 CMSampleBufferRef sample_buffer);

// Returns implementation class for VideoCaptureDeviceAVFoundation depending
// on if |kMacNextGenerationCapturer| is enabled or disabled.
Class GetVideoCaptureDeviceAVFoundationImplementationClass();

gfx::Size CAPTURE_EXPORT GetPixelBufferSize(CVPixelBufferRef pixel_buffer);
gfx::Size CAPTURE_EXPORT GetSampleBufferSize(CMSampleBufferRef sample_buffer);

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_UTILS_MAC_H_
