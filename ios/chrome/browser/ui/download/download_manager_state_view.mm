// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/download_manager_state_view.h"

#import <QuartzCore/QuartzCore.h>

#include "base/mac/foundation_util.h"
#include "ios/chrome/browser/ui/download/download_manager_animation_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Using fixed size allows to achieve pixel perfect image.
const CGFloat kViewSize = 28;
// Stroke line width.
const CGFloat kLineWidth = 1;
// The scale of "in progress" icon.
const CGFloat kInProgressScale = 0.65f;
}  // namespace

@interface DownloadManagerStateView ()
// CALayer that backs this view up. Responsible for drawing the icon.
@property(nonatomic, readonly) CAShapeLayer* iconLayer;

// CALayer for error or done badge.
@property(nonatomic, readonly) CALayer* badgeLayer;

@end

@implementation DownloadManagerStateView
@synthesize state = _state;
@synthesize downloadColor = _downloadColor;
@synthesize documentColor = _documentColor;
@synthesize badgeLayer = _badgeLayer;

#pragma mark - UIView overrides

+ (Class)layerClass {
  return [CAShapeLayer class];
}

- (void)setBounds:(CGRect)bounds {
  [super setBounds:bounds];

  CGImageRef badgeImage = [self completionBadgeImage];
  CGFloat scale = UIScreen.mainScreen.scale;
  self.badgeLayer.frame = CGRectMake(CGRectGetMidX(self.iconLayer.bounds),
                                     CGRectGetMidY(self.iconLayer.bounds),
                                     CGImageGetWidth(badgeImage) / scale,
                                     CGImageGetHeight(badgeImage) / scale);

  [self updateUIAnimated:NO];
}

- (CGSize)intrinsicContentSize {
  return CGSizeMake(kViewSize, kViewSize);
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self updateUIAnimated:NO];
}

#pragma mark - Public

- (void)setState:(DownloadManagerState)state {
  [self setState:state animated:NO];
}

- (void)setState:(DownloadManagerState)state animated:(BOOL)animated {
  if (_state != state) {
    _state = state;
    [self updateUIAnimated:animated];
  }
}

#pragma mark - Private

// Returns completion badge image approprivate for the state. The badge is
// visible only in Succeeded and Failed states. Badge presentation is animated
// by changing the badge scale.
- (CGImageRef)completionBadgeImage {
  NSString* const imageName =
      _state == kDownloadManagerStateSucceeded ? @"done_badge" : @"error_badge";
  return [UIImage imageNamed:imageName].CGImage;
}

// Updates CoreAnimation layers (icon and badge).
- (void)updateUIAnimated:(BOOL)animated {
  if (@available(iOS 13, *)) {
    [self.traitCollection performAsCurrentTraitCollection:^{
      [self updateUIWithCurrentTraitCollectionAnimated:animated];
    }];
    return;
  }
  [self updateUIWithCurrentTraitCollectionAnimated:animated];
}

// Updates the CoreAnimation layers assuming currentTraitCollection is set
// correctly, so any .CGColor property returns the correct color for the current
// mode (light vs. dark).
- (void)updateUIWithCurrentTraitCollectionAnimated:(BOOL)animated {
  NSTimeInterval animationDuration =
      animated ? kDownloadManagerAnimationDuration : 0.0;

  switch (_state) {
    case kDownloadManagerStateNotStarted:
      self.iconLayer.path = self.downloadPath.CGPath;
      self.iconLayer.fillColor = self.downloadColor.CGColor;
      self.iconLayer.strokeColor = self.downloadColor.CGColor;
      self.iconLayer.transform = CATransform3DIdentity;
      self.badgeLayer.transform =
          CATransform3DScale(CATransform3DIdentity, 0, 0, 1);
      break;
    case kDownloadManagerStateSucceeded:
    case kDownloadManagerStateFailed: {
      self.badgeLayer.contents = (__bridge id)[self completionBadgeImage];
      self.iconLayer.path = self.documentPath.CGPath;
      self.iconLayer.fillColor = self.documentColor.CGColor;
      self.iconLayer.strokeColor = self.documentColor.CGColor;
      if (!CATransform3DIsIdentity(self.iconLayer.transform)) {
        [UIView animateWithDuration:animationDuration
            animations:^{  // Resize the icon.
              self.iconLayer.transform = CATransform3DIdentity;
            }
            completion:^(BOOL finished) {  // Resize the badge.
              [CATransaction begin];
              [CATransaction setAnimationDuration:animationDuration];
              self.badgeLayer.transform = CATransform3DIdentity;
              [CATransaction commit];
            }];
      }
      break;
    }
    case kDownloadManagerStateInProgress:
      if (CGPathEqualToPath(self.iconLayer.path, self.downloadPath.CGPath)) {
        // There should be no animation when changing from downloadPath to
        // documentPath.
        animationDuration = 0;
      }

      self.iconLayer.path = self.documentPath.CGPath;
      self.iconLayer.fillColor = self.documentColor.CGColor;
      self.iconLayer.strokeColor = self.documentColor.CGColor;
      if (CATransform3DIsIdentity(self.iconLayer.transform)) {
        [CATransaction begin];  // Resize the badge.
        [CATransaction setAnimationDuration:animationDuration];
        self.badgeLayer.transform =
            CATransform3DScale(CATransform3DIdentity, 0, 0, 1);
        [CATransaction setCompletionBlock:^{
          [UIView animateWithDuration:animationDuration
                           animations:^{  // Resize the icon.
                             self.iconLayer.transform = CATransform3DScale(
                                 CATransform3DIdentity, kInProgressScale,
                                 kInProgressScale, 1);
                           }];
        }];
        [CATransaction commit];
      }

      break;
  }
  self.iconLayer.lineWidth = kLineWidth;
}

// Used for all states except "not started" for icon layer.
- (UIBezierPath*)documentPath {
  const CGFloat kVerticalMargin = 4;  // top and bottom margins
  const CGFloat kAspectRatio = 0.82;  // height is bigger than width

  // The constants below define the area where document icon is drawn.
  const CGFloat minY = CGRectGetMinY(self.bounds);
  const CGFloat maxY = CGRectGetMaxY(self.bounds);
  const CGFloat top = minY + kVerticalMargin + kLineWidth / 2;
  const CGFloat bottom = maxY - kVerticalMargin - kLineWidth / 2;
  const CGFloat horizontalMargin =
      round((bottom - top) * (1 - kAspectRatio) / 2) + kVerticalMargin;
  const CGFloat minX = CGRectGetMinX(self.bounds);
  const CGFloat maxX = CGRectGetMaxX(self.bounds);
  const CGFloat left = minX + horizontalMargin + kLineWidth / 2;
  const CGFloat right = maxX - horizontalMargin - kLineWidth / 2;

  // All corners except top-right are rounded.
  const CGFloat kRadius = 1;

  // Top-right corner is folded and not rounded.
  const CGFloat cornerSize = (right - left) * 0.45;

  UIBezierPath* path = [UIBezierPath bezierPath];
  [path moveToPoint:CGPointMake(right - cornerSize, top)];
  [path addLineToPoint:CGPointMake(left + kRadius, top)];
  [path addArcWithCenter:CGPointMake(left + kRadius, top + kRadius)
                  radius:kRadius
              startAngle:-M_PI_2
                endAngle:M_PI
               clockwise:NO];
  [path addLineToPoint:CGPointMake(left, bottom - kRadius)];
  [path addArcWithCenter:CGPointMake(left + kRadius, bottom - kRadius)
                  radius:kRadius
              startAngle:M_PI
                endAngle:M_PI_2
               clockwise:NO];
  [path addLineToPoint:CGPointMake(right - kRadius, bottom)];
  [path addArcWithCenter:CGPointMake(right - kRadius, bottom - kRadius)
                  radius:kRadius
              startAngle:M_PI_2
                endAngle:0
               clockwise:NO];
  [path addLineToPoint:CGPointMake(right, top + cornerSize)];
  [path addLineToPoint:CGPointMake(right - cornerSize, top + cornerSize)];
  [path closePath];
  [path addLineToPoint:CGPointMake(right - cornerSize + kLineWidth, top)];
  [path addLineToPoint:CGPointMake(right, top + cornerSize - kLineWidth)];
  [path addLineToPoint:CGPointMake(right, top + cornerSize)];
  return path;
}

// Used for "not started" state for icon layer.
- (UIBezierPath*)downloadPath {
  const CGFloat horizontalMargin = 6;  // left and right margins
  const CGFloat topMargin = 4;
  const CGFloat bottomMargin = 5;

  // The constants below define the area where arrow icon is drawn.
  const CGFloat minX = CGRectGetMinX(self.bounds);
  const CGFloat maxX = CGRectGetMaxX(self.bounds);
  const CGFloat midX = CGRectGetMidX(self.bounds);
  const CGFloat left = minX + horizontalMargin + kLineWidth / 2;
  const CGFloat right = maxX - horizontalMargin - kLineWidth / 2;
  const CGFloat minY = CGRectGetMinY(self.bounds);
  const CGFloat maxY = CGRectGetMaxY(self.bounds);
  const CGFloat top = minY + topMargin + kLineWidth / 2;
  const CGFloat bottom = maxY - bottomMargin - kLineWidth / 2;
  const CGFloat width = (right - left);
  const CGFloat height = (bottom - top);

  // Top part of download icon has a pointing down arrow.
  const CGFloat arrowWidth = round(width * 0.4);     // does not include the tip
  const CGFloat arrowHeight = round(height * 0.80);  // includes arrow tip
  const CGFloat arrowMid = top + arrowHeight / 2;

  // Bottom part of download icon has a rect, which symbolizes ground.
  const CGFloat groundHeight = 1;
  CGRect ground = CGRectMake(left, bottom - groundHeight, width, groundHeight);

  UIBezierPath* path = [UIBezierPath bezierPath];
  [path moveToPoint:CGPointMake(midX + arrowWidth / 2, top)];
  [path addLineToPoint:CGPointMake(midX - arrowWidth / 2, top)];
  [path addLineToPoint:CGPointMake(midX - arrowWidth / 2, arrowMid)];
  [path addLineToPoint:CGPointMake(left, arrowMid)];
  [path addLineToPoint:CGPointMake(midX, top + arrowHeight)];
  [path addLineToPoint:CGPointMake(right, arrowMid)];
  [path addLineToPoint:CGPointMake(midX + arrowWidth / 2, arrowMid)];
  [path closePath];
  [path appendPath:[UIBezierPath bezierPathWithRect:ground]];
  return path;
}

- (CAShapeLayer*)iconLayer {
  return base::mac::ObjCCastStrict<CAShapeLayer>(self.layer);
}

- (CALayer*)badgeLayer {
  if (!_badgeLayer) {
    _badgeLayer = [CALayer layer];
    [self.iconLayer addSublayer:_badgeLayer];
  }
  return _badgeLayer;
}

@end
