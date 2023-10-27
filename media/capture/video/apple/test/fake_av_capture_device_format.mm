// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/apple/test/fake_av_capture_device_format.h"

#include "base/apple/scoped_cftyperef.h"

@implementation FakeAVFrameRateRange
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-designated-initializers"
- (instancetype)initWithMinFrameRate:(Float64)minFrameRate
                        maxFrameRate:(Float64)maxFrameRate {
  _minFrameRate = minFrameRate;
  _maxFrameRate = maxFrameRate;
  return self;
}
- (Float64)minFrameRate {
  return _minFrameRate;
}
- (Float64)maxFrameRate {
  return _maxFrameRate;
}
@end

@implementation FakeAVCaptureDeviceFormat {
  base::apple::ScopedCFTypeRef<CMVideoFormatDescriptionRef> _formatDescription;
  FakeAVFrameRateRange* __strong _frameRateRange1;
  FakeAVFrameRateRange* __strong _frameRateRange2;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-designated-initializers"
- (instancetype)initWithWidth:(int)width
                       height:(int)height
                       fourCC:(FourCharCode)fourCC
                    frameRate:(Float64)frameRate {
  CMVideoFormatDescriptionCreate(nullptr, fourCC, width, height, nullptr,
                                 _formatDescription.InitializeInto());
  _frameRateRange1 =
      [[FakeAVFrameRateRange alloc] initWithMinFrameRate:frameRate
                                            maxFrameRate:frameRate];
  return self;
}
#pragma clang diagnostic pop

- (void)setSecondFrameRate:(Float64)frameRate {
  _frameRateRange2 =
      [[FakeAVFrameRateRange alloc] initWithMinFrameRate:frameRate
                                            maxFrameRate:frameRate];
}

- (CMFormatDescriptionRef)formatDescription {
  return _formatDescription.get();
}

- (NSArray<AVFrameRateRange*>*)videoSupportedFrameRateRanges {
  return _frameRateRange2 ? @[ _frameRateRange1, _frameRateRange2 ]
                          : @[ _frameRateRange1 ];
}
@end
