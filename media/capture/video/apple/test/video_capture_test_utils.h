// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_APPLE_TEST_VIDEO_CAPTURE_TEST_UTILS_H_
#define MEDIA_CAPTURE_VIDEO_APPLE_TEST_VIDEO_CAPTURE_TEST_UTILS_H_

#import <Foundation/Foundation.h>

#include <memory>

#include "base/functional/callback_forward.h"
#include "media/capture/video/apple/video_capture_device_factory_apple.h"

namespace media {

// Video capture code on MacOSX must run on a CFRunLoop enabled thread
// for interaction with AVFoundation.
// In order to make the test case run on the actual message loop that has
// been created for this thread, we need to run it inside a RunLoop. This is
// required, because on MacOS the capture code must run on a CFRunLoop
// enabled message loop.
void RunTestCase(base::OnceClosure test_case);

std::vector<VideoCaptureDeviceInfo> GetDevicesInfo(
    VideoCaptureDeviceFactoryApple* video_capture_device_factory);
// If there are no devices, nil is returned.
NSString* GetFirstDeviceId();

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_APPLE_TEST_VIDEO_CAPTURE_TEST_UTILS_H_
