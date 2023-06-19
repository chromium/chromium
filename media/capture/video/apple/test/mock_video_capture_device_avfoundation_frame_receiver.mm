// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "media/capture/video/apple/test/mock_video_capture_device_avfoundation_frame_receiver.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace media {

MockVideoCaptureDeviceAVFoundationFrameReceiver::
    MockVideoCaptureDeviceAVFoundationFrameReceiver() = default;

MockVideoCaptureDeviceAVFoundationFrameReceiver::
    ~MockVideoCaptureDeviceAVFoundationFrameReceiver() = default;

}  // namespace media
