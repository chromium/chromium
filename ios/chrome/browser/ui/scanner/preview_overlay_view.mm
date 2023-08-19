// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/scanner/preview_overlay_view.h"

#import <AVFoundation/AVFoundation.h>

#import "ios/chrome/common/ui/util/ui_util.h"
#import "ui/base/device_form_factor.h"

namespace {

// Length of the viewport borders, starting from the corner.
const CGFloat kViewportBorderLengthFromCornerIPhone = 25.0;
const CGFloat kViewportBorderLengthFromCornerIPad = 30.0;

// Opacity of the preview overlay.
const CGFloat kPreviewOverlayOpacity = 0.5;

// Corner radius of the border around the viewport.
const CGFloat kViewportBorderCornerRadius = 2.0;
// Line width of the viewport border.
const CGFloat kViewportBorderLineWidth = 4.0;
// Shadow opacity of the viewport border.
const CGFloat kViewportBorderShadowOpacity = 1.0;
// Shadow radius of the viewport border.
const CGFloat kViewportBorderShadowRadius = 10.0;

}  // namespace

@interface PreviewOverlayView () {
  // A transparent preview overlay. The overlay is a sublayer of the
  // PreviewOverlayView's view to keep the opacity of the view's layer 1.0,
  // otherwise the viewport border would inherit the opacity of the overlay.
  CALayer* _previewOverlay;
  // A container for the viewport border to draw a shadow under the border.
  // Sublayer of PreviewOverlayView's layer.
  CALayer* _viewportBorderContainer;
  // The preview viewport border. Sublayer of `_viewportBorderContainer`.
  CAShapeLayer* _viewportBorder;
  // The current viewport size.
  CGSize _viewportSize;
}
@end

@implementation PreviewOverlayView

- (instancetype)initWithFrame:(CGRect)frame
                 viewportSize:(CGSize)overlayViewportSize {
  self = [super initWithFrame:frame];
  if (!self) {
    return nil;
  }

  _viewportSize = overlayViewportSize;

  _previewOverlay = [[CALayer alloc] init];
  [_previewOverlay setBackgroundColor:[[UIColor blackColor] CGColor]];
  [_previewOverlay setOpacity:kPreviewOverlayOpacity];
  [[self layer] addSublayer:_previewOverlay];

  _viewportBorderContainer = [[CALayer alloc] init];
  [_viewportBorderContainer setShadowColor:[[UIColor blackColor] CGColor]];
  [_viewportBorderContainer setShadowOffset:CGSizeZero];
  [_viewportBorderContainer setShadowRadius:kViewportBorderShadowRadius];
  [_viewportBorderContainer setShadowOpacity:kViewportBorderShadowOpacity];
  [_viewportBorderContainer setShouldRasterize:YES];
  [_viewportBorderContainer
      setRasterizationScale:[[UIScreen mainScreen] scale]];

  _viewportBorder = [[CAShapeLayer alloc] init];
  [_viewportBorder setStrokeColor:[[UIColor whiteColor] CGColor]];
  [_viewportBorder setFillColor:nil];
  [_viewportBorder setOpacity:1.0];
  [_viewportBorder setLineWidth:kViewportBorderLineWidth];
  [_viewportBorderContainer addSublayer:_viewportBorder];

  [[self layer] addSublayer:_viewportBorderContainer];
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  CGSize frameSize = self.frame.size;

  CGFloat viewportWidth = _viewportSize.width;
  CGFloat viewportHeight = _viewportSize.height;

  [_previewOverlay
      setFrame:CGRectMake(0, 0, frameSize.width, frameSize.height)];
  [_previewOverlay setMask:[self viewportMaskWithFrameSize:frameSize]];

  CGRect borderRect = CGRectMakeCenteredRectInFrame(
      frameSize, CGSizeMake(viewportWidth + kViewportBorderLineWidth,
                            viewportHeight + kViewportBorderLineWidth));
  UIBezierPath* borderPath =
      [UIBezierPath bezierPathWithRoundedRect:borderRect
                                 cornerRadius:kViewportBorderCornerRadius];

  [_viewportBorder setPath:[borderPath CGPath]];
  [_viewportBorder setMask:[self viewportBorderMaskWithFrameSize:frameSize]];
}

#pragma mark - Private

// Creates a square mask for the overlay to keep the viewport transparent.
- (CAShapeLayer*)viewportMaskWithFrameSize:(CGSize)frameSize {
  CGRect frameRect = CGRectMake(0, 0, frameSize.width, frameSize.height);
  CGRect viewportRect = CGRectMakeCenteredRectInFrame(frameSize, _viewportSize);
  UIBezierPath* maskPath = [UIBezierPath bezierPathWithRect:frameRect];
  [maskPath appendPath:[UIBezierPath bezierPathWithRect:viewportRect]];

  CAShapeLayer* mask = [[CAShapeLayer alloc] init];
  [mask setFillColor:[[UIColor blackColor] CGColor]];
  [mask setFillRule:kCAFillRuleEvenOdd];
  [mask setFrame:frameRect];
  [mask setPath:maskPath.CGPath];
  return mask;
}

// Creates a mask to only draw the corners of the viewport border.
- (CAShapeLayer*)viewportBorderMaskWithFrameSize:(CGSize)frameSize {
  CGFloat viewportBorderCornerLength =
      (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET)
          ? kViewportBorderLengthFromCornerIPad
          : kViewportBorderLengthFromCornerIPhone;
  CGRect maskRect = CGRectMakeCenteredRectInFrame(
      frameSize,
      CGSizeMake(_viewportSize.width - 2 * viewportBorderCornerLength,
                 _viewportSize.height - 2 * viewportBorderCornerLength));
  CGFloat sizeX = maskRect.origin.x;
  CGFloat sizeY = maskRect.origin.y;
  CGFloat offsetX = sizeX + maskRect.size.width;
  CGFloat offsetY = sizeY + maskRect.size.height;

  UIBezierPath* path =
      [UIBezierPath bezierPathWithRect:CGRectMake(0, 0, sizeX, sizeY)];
  [path appendPath:[UIBezierPath bezierPathWithRect:CGRectMake(0, offsetY,
                                                               sizeX, sizeY)]];
  [path appendPath:[UIBezierPath bezierPathWithRect:CGRectMake(offsetX, 0,
                                                               sizeX, sizeY)]];
  [path appendPath:[UIBezierPath bezierPathWithRect:CGRectMake(offsetX, offsetY,
                                                               sizeX, sizeY)]];

  CAShapeLayer* mask = [[CAShapeLayer alloc] init];
  [mask setFillColor:[[UIColor blackColor] CGColor]];
  [mask setFrame:CGRectMake(0, 0, frameSize.width, frameSize.height)];
  [mask setPath:path.CGPath];
  return mask;
}

@end
