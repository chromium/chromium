// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/scanner/video_preview_view.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "ios/chrome/common/ui/util/ui_util.h"

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
  return base::apple::ObjCCastStrict<AVCaptureVideoPreviewLayer>([self layer]);
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
  CGRect viewportRect =
      CGRectMakeCenteredRectInFrame(self.frame.size, _viewportSize);
  AVCaptureVideoPreviewLayer* layer = [self previewLayer];
  // If the layer does not have a connection,
  // `metadataOutputRectOfInterestForRect:` does not return the right value.
  DCHECK(layer.connection);
  return [layer metadataOutputRectOfInterestForRect:viewportRect];
}

@end
