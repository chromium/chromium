// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/mac/test/fake_av_capture_device_format.h"

#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"

@implementation FakeAVFrameRateRange
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-designated-initializers"
- (instancetype)initWithMinFrameRate:(Float64)minFrameRate
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

@implementation FakeAVCaptureDeviceFormat
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-designated-initializers"
- (instancetype)initWithWidth:(int)width
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
