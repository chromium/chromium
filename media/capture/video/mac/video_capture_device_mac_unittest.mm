// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/mac/video_capture_device_avfoundation_mac.h"
#include "media/capture/video/mac/video_capture_device_avfoundation_utils_mac.h"

#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "testing/gtest/include/gtest/gtest.h"

// Create a subclass of AVFrameRateRange because there is no API to initialize
// a custom AVFrameRateRange.
@interface FakeAVFrameRateRange : AVFrameRateRange {
  Float64 _minFrameRate;
  Float64 _maxFrameRate;
}
- (id)initWithMinFrameRate:(Float64)minFrameRate
              maxFrameRate:(Float64)maxFrameRate;
@end

@implementation FakeAVFrameRateRange
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-designated-initializers"
- (id)initWithMinFrameRate:(Float64)minFrameRate
              maxFrameRate:(Float64)maxFrameRate {
  _minFrameRate = minFrameRate;
  _maxFrameRate = maxFrameRate;
  return self;
}
- (void)dealloc {
  [super dealloc];
}
- (Float64)minFrameRate {
  return _minFrameRate;
}
- (Float64)maxFrameRate {
  return _maxFrameRate;
}
@end

// Create a subclass of AVCaptureDeviceFormat because there is no API to
// initialize a custom AVCaptureDeviceFormat.
@interface FakeAVCaptureDeviceFormat : AVCaptureDeviceFormat {
  base::ScopedCFTypeRef<CMVideoFormatDescriptionRef> _formatDescription;
  base::scoped_nsobject<FakeAVFrameRateRange> _frameRateRange1;
  base::scoped_nsobject<FakeAVFrameRateRange> _frameRateRange2;
};
- (id)initWithWidth:(int)width
             height:(int)height
             fourCC:(FourCharCode)fourCC
          frameRate:(Float64)frameRate;
- (void)setSecondFrameRate:(Float64)frameRate;
@end

@implementation FakeAVCaptureDeviceFormat
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-designated-initializers"
- (id)initWithWidth:(int)width
             height:(int)height
             fourCC:(FourCharCode)fourCC
          frameRate:(Float64)frameRate {
  CMVideoFormatDescriptionCreate(nullptr, fourCC, width, height, nullptr,
                                 _formatDescription.InitializeInto());
  _frameRateRange1.reset([[FakeAVFrameRateRange alloc]
      initWithMinFrameRate:frameRate
              maxFrameRate:frameRate]);
  return self;
}
#pragma clang diagnostic pop

- (void)setSecondFrameRate:(Float64)frameRate {
  _frameRateRange2.reset([[FakeAVFrameRateRange alloc]
      initWithMinFrameRate:frameRate
              maxFrameRate:frameRate]);
}

- (CMFormatDescriptionRef)formatDescription {
  return _formatDescription;
}

- (NSArray<AVFrameRateRange*>*)videoSupportedFrameRateRanges {
  return _frameRateRange2 ? @[ _frameRateRange1, _frameRateRange2 ]
                          : @[ _frameRateRange1 ];
}
@end

namespace media {

// Test the behavior of the function FindBestCaptureFormat which is used to
// determine the capture format.
TEST(VideoCaptureDeviceMacTest, FindBestCaptureFormat) {
  base::scoped_nsobject<FakeAVCaptureDeviceFormat> fmt_320_240_xyzw_30(
      [[FakeAVCaptureDeviceFormat alloc] initWithWidth:320
                                                height:240
                                                fourCC:'xyzw'
                                             frameRate:30]);

  base::scoped_nsobject<FakeAVCaptureDeviceFormat> fmt_320_240_yuvs_30(
      [[FakeAVCaptureDeviceFormat alloc] initWithWidth:320
                                                height:240
                                                fourCC:'yuvs'
                                             frameRate:30]);
  base::scoped_nsobject<FakeAVCaptureDeviceFormat> fmt_640_480_yuvs_30(
      [[FakeAVCaptureDeviceFormat alloc] initWithWidth:640
                                                height:480
                                                fourCC:'yuvs'
                                             frameRate:30]);

  base::scoped_nsobject<FakeAVCaptureDeviceFormat> fmt_320_240_2vuy_30(
      [[FakeAVCaptureDeviceFormat alloc] initWithWidth:320
                                                height:240
                                                fourCC:'2vuy'
                                             frameRate:30]);
  base::scoped_nsobject<FakeAVCaptureDeviceFormat> fmt_640_480_2vuy_30(
      [[FakeAVCaptureDeviceFormat alloc] initWithWidth:640
                                                height:480
                                                fourCC:'2vuy'
                                             frameRate:30]);
  base::scoped_nsobject<FakeAVCaptureDeviceFormat> fmt_640_480_2vuy_60(
      [[FakeAVCaptureDeviceFormat alloc] initWithWidth:640
                                                height:480
                                                fourCC:'2vuy'
                                             frameRate:30]);
  base::scoped_nsobject<FakeAVCaptureDeviceFormat> fmt_640_480_2vuy_30_60(
      [[FakeAVCaptureDeviceFormat alloc] initWithWidth:640
                                                height:480
                                                fourCC:'2vuy'
                                             frameRate:30]);
  [fmt_640_480_2vuy_30_60 setSecondFrameRate:60];

  // We'll be using this for the result in all of the below tests. Note that
  // in all of the tests, the test is run with the candidate capture functions
  // in two orders (forward and reversed). This is to avoid having the traversal
  // order of FindBestCaptureFormat affect the result.
  AVCaptureDeviceFormat* result = nil;

  // If we can't find a valid format, we should return nil;
  result = FindBestCaptureFormat(@[ fmt_320_240_xyzw_30 ], 320, 240, 30);
  EXPECT_EQ(result, nil);

  // Can't find a matching resolution
  result = FindBestCaptureFormat(@[ fmt_320_240_yuvs_30, fmt_320_240_2vuy_30 ],
                                 640, 480, 30);
  EXPECT_EQ(result, nil);
  result = FindBestCaptureFormat(@[ fmt_320_240_2vuy_30, fmt_320_240_yuvs_30 ],
                                 640, 480, 30);
  EXPECT_EQ(result, nil);

  // Simple exact match.
  result = FindBestCaptureFormat(@[ fmt_640_480_yuvs_30, fmt_320_240_yuvs_30 ],
                                 320, 240, 30);
  EXPECT_EQ(result, fmt_320_240_yuvs_30.get());
  result = FindBestCaptureFormat(@[ fmt_320_240_yuvs_30, fmt_640_480_yuvs_30 ],
                                 320, 240, 30);
  EXPECT_EQ(result, fmt_320_240_yuvs_30.get());

  // Different frame rate.
  result = FindBestCaptureFormat(@[ fmt_640_480_2vuy_30 ], 640, 480, 60);
  EXPECT_EQ(result, fmt_640_480_2vuy_30.get());

  // Prefer the same frame rate.
  result = FindBestCaptureFormat(@[ fmt_640_480_yuvs_30, fmt_640_480_2vuy_60 ],
                                 640, 480, 60);
  EXPECT_EQ(result, fmt_640_480_2vuy_60.get());
  result = FindBestCaptureFormat(@[ fmt_640_480_2vuy_60, fmt_640_480_yuvs_30 ],
                                 640, 480, 60);
  EXPECT_EQ(result, fmt_640_480_2vuy_60.get());

  // Prefer version with matching frame rate.
  result = FindBestCaptureFormat(@[ fmt_640_480_yuvs_30, fmt_640_480_2vuy_60 ],
                                 640, 480, 60);
  EXPECT_EQ(result, fmt_640_480_2vuy_60.get());
  result = FindBestCaptureFormat(@[ fmt_640_480_2vuy_60, fmt_640_480_yuvs_30 ],
                                 640, 480, 60);
  EXPECT_EQ(result, fmt_640_480_2vuy_60.get());

  // Prefer version with matching frame rate when there are multiple framerates.
  result = FindBestCaptureFormat(
      @[ fmt_640_480_yuvs_30, fmt_640_480_2vuy_30_60 ], 640, 480, 60);
  EXPECT_EQ(result, fmt_640_480_2vuy_30_60.get());
  result = FindBestCaptureFormat(
      @[ fmt_640_480_2vuy_30_60, fmt_640_480_yuvs_30 ], 640, 480, 60);
  EXPECT_EQ(result, fmt_640_480_2vuy_30_60.get());

  // Prefer version with the lower maximum framerate when there are multiple
  // framerates.
  result = FindBestCaptureFormat(
      @[ fmt_640_480_2vuy_30, fmt_640_480_2vuy_30_60 ], 640, 480, 30);
  EXPECT_EQ(result, fmt_640_480_2vuy_30.get());
  result = FindBestCaptureFormat(
      @[ fmt_640_480_2vuy_30_60, fmt_640_480_2vuy_30 ], 640, 480, 30);
  EXPECT_EQ(result, fmt_640_480_2vuy_30.get());

  // Prefer the Chromium format order.
  result = FindBestCaptureFormat(@[ fmt_640_480_yuvs_30, fmt_640_480_2vuy_30 ],
                                 640, 480, 30);
  EXPECT_EQ(result, fmt_640_480_2vuy_30.get());
  result = FindBestCaptureFormat(@[ fmt_640_480_2vuy_30, fmt_640_480_yuvs_30 ],
                                 640, 480, 30);
  EXPECT_EQ(result, fmt_640_480_2vuy_30.get());
}

}  // namespace media
