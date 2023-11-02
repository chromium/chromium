// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_MAC_TEST_FAKE_AV_CAPTURE_DEVICE_FORMAT_H_
#define MEDIA_CAPTURE_VIDEO_MAC_TEST_FAKE_AV_CAPTURE_DEVICE_FORMAT_H_

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"

// Create a subclass of AVFrameRateRange because there is no API to initialize
// a custom AVFrameRateRange.
@interface FakeAVFrameRateRange : AVFrameRateRange {
  Float64 _minFrameRate;
  Float64 _maxFrameRate;
}
- (instancetype)initWithMinFrameRate:(Float64)minFrameRate
                        maxFrameRate:(Float64)maxFrameRate;
@end

// Create a subclass of AVCaptureDeviceFormat because there is no API to
// initialize a custom AVCaptureDeviceFormat.
@interface FakeAVCaptureDeviceFormat : AVCaptureDeviceFormat {
  base::ScopedCFTypeRef<CMVideoFormatDescriptionRef> _formatDescription;
  base::scoped_nsobject<FakeAVFrameRateRange> _frameRateRange1;
  base::scoped_nsobject<FakeAVFrameRateRange> _frameRateRange2;
};
- (instancetype)initWithWidth:(int)width
                       height:(int)height
                       fourCC:(FourCharCode)fourCC
                    frameRate:(Float64)frameRate;
- (void)setSecondFrameRate:(Float64)frameRate;
@end

#endif  // MEDIA_CAPTURE_VIDEO_MAC_TEST_FAKE_AV_CAPTURE_DEVICE_FORMAT_H_