// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/scanner/video_preview_view.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/util/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface VideoPreviewView () {
  // The current viewport size.
  CGSize _viewportSize;
}
@end

@implementation VideoPreviewView

- (instancetype)initWithFrame:(CGRect)frame
                 viewportSize:(CGSize)previewViewportSize {
  self = [super initWithFrame:frame];

  if (self) {
    _viewportSize = previewViewportSize;
  }
  return self;
}

+ (Class)layerClass {
  return [AVCaptureVideoPreviewLayer class];
}

- (AVCaptureVideoPreviewLayer*)previewLayer {
  return base::mac::ObjCCastStrict<AVCaptureVideoPreviewLayer>([self layer]);
}

- (CGRect)viewportRegionOfInterest {
  CGRect rect = CGRectMakeCenteredRectInFrame(self.frame.size, _viewportSize);
  CGFloat x = rect.origin.x / self.frame.size.width;
  CGFloat y = rect.origin.y / self.frame.size.height;

  CGFloat width = rect.size.width / self.frame.size.width;
  CGFloat height = rect.size.height / self.frame.size.height;

  // Vision region of interest measures the frame size from the lower left
  // corner so reverse x & y, reverse width & height.
  return CGRectMake(y, x, height, width);
}

- (CGRect)viewportRectOfInterest {
  DCHECK(CGPointEqualToPoint(self.frame.origin, CGPointZero));
  CGRect viewportRect =
      CGRectMakeCenteredRectInFrame(self.frame.size, _viewportSize);
  AVCaptureVideoPreviewLayer* layer = [self previewLayer];
  // If the layer does not have a connection,
  // |metadataOutputRectOfInterestForRect:| does not return the right value.
  DCHECK(layer.connection);
  return [layer metadataOutputRectOfInterestForRect:viewportRect];
}

@end
