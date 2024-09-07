// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/tooltip_view.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

CGFloat kTooltipTailHeight = 8;
CGFloat kTooltipTailWidth = 16;
CGFloat kTooltipHorizontalPadding = 16.0f;
CGFloat kTooltipVerticalPadding = 10.0f;
CGFloat kTooltipCornerRadius = 8.0f;
CGFloat kTooltipFadeInTime = 0.2f;

}  // namespace

@implementation TooltipView {
  __weak UIView* _keyWindow;
  __weak NSObject* _target;
  SEL _action;
  UITapGestureRecognizer* _tapBehindGesture;
}

static __weak TooltipView* _active;

- (instancetype)initWithKeyWindow:(UIView*)keyWindow
                           target:(NSObject*)target
                           action:(SEL)action {
  self = [super init];
  if (self) {
    _keyWindow = keyWindow;
    _target = target;
    _action = action;

    _tapBehindGesture =
        [[UITapGestureRecognizer alloc] initWithTarget:self
                                                action:@selector(checkTap:)];
    [_tapBehindGesture setNumberOfTapsRequired:1];
    [_tapBehindGesture setCancelsTouchesInView:NO];

    if (@available(iOS 17, *)) {
      NSArray<UITrait>* traits = @[
        UITraitUserInterfaceIdiom.self, UITraitUserInterfaceStyle.self,
        UITraitDisplayGamut.self, UITraitAccessibilityContrast.self,
        UITraitUserInterfaceLevel.self
      ];
      __weak TooltipView* weakSelf = self;
      UITraitChangeHandler handler = ^(id<UITraitEnvironment> traitEnvironment,
                                       UITraitCollection* previousCollection) {
        weakSelf.backgroundLayer.fillColor =
            [UIColor colorNamed:kTextPrimaryColor].CGColor;
      };

      [self registerForTraitChanges:traits withHandler:handler];
    }
  }
  return self;
}

- (void)showMessage:(NSString*)message atBottomOf:(UIView*)view {
  if (_active) {
    [_active hide];
  }
  _active = self;

  [_keyWindow addGestureRecognizer:_tapBehindGesture];

  CGSize labelSize = [message sizeWithAttributes:@{
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline]
  }];

  UILabel* label = [[UILabel alloc]
      initWithFrame:CGRectMake(kTooltipHorizontalPadding,
                               kTooltipTailHeight + kTooltipVerticalPadding,
                               labelSize.width, labelSize.height)];
  label.textAlignment = NSTextAlignmentLeft;
  label.text = message;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  label.textColor = [UIColor colorNamed:kBackgroundColor];

  CGFloat width = labelSize.width + 2 * kTooltipHorizontalPadding;
  CGFloat height = labelSize.height + 2 * kTooltipVerticalPadding;

  CGPoint anchor =
      CGPointMake(view.frame.size.width / 2, view.frame.size.height);
  anchor = [_keyWindow convertPoint:anchor fromView:view];

  self.frame = CGRectMake(0.0, 0.0, width, kTooltipTailHeight + height);
  self.center = CGPointMake(
      MIN(_keyWindow.frame.size.width - width / 2 - kTooltipHorizontalPadding,
          MAX(kTooltipHorizontalPadding + width / 2, anchor.x)),
      anchor.y + kTooltipTailHeight + height / 2.0);
  self.translatesAutoresizingMaskIntoConstraints = NO;

  CGRect tooltipRect = CGRectMake(0.0, kTooltipTailHeight, width, height);
  CGFloat centerX = tooltipRect.size.width / 2.0;
  CGFloat halfTailWidth = kTooltipTailWidth / 2.0;

  UIBezierPath* path =
      [UIBezierPath bezierPathWithRoundedRect:tooltipRect
                                 cornerRadius:kTooltipCornerRadius];
  [path moveToPoint:CGPointMake(centerX, 0)];
  [path addLineToPoint:CGPointMake(centerX + halfTailWidth,
                                   kTooltipTailHeight + 1)];
  [path addLineToPoint:CGPointMake(centerX - halfTailWidth,
                                   kTooltipTailHeight + 1)];
  [path closePath];

  self.backgroundLayer.path = path.CGPath;
  self.backgroundLayer.fillColor =
      [UIColor colorNamed:kTextPrimaryColor].CGColor;

  [self addSubview:label];
  [_keyWindow addSubview:self];

  self.alpha = 0.0;
  [UIView animateWithDuration:kTooltipFadeInTime
                        delay:0.0
                      options:UIViewAnimationOptionCurveEaseOut
                   animations:^{
                     self.alpha = 1.0;
                   }
                   completion:nil];
}

- (void)hide {
  [_keyWindow removeGestureRecognizer:_tapBehindGesture];
  if (self == _active) {
    _active = nil;
  }
  [self.delegate tooltipViewWillDismiss:self];
  [UIView animateWithDuration:kTooltipFadeInTime
      delay:0.0
      options:UIViewAnimationOptionCurveEaseOut
      animations:^{
        self.alpha = 0.0;
      }
      completion:^(BOOL finished) {
        [self removeFromSuperview];
      }];
}

#pragma mark - Private

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }
  if ([self.traitCollection
          hasDifferentColorAppearanceComparedToTraitCollection:
              previousTraitCollection]) {
    self.backgroundLayer.fillColor =
        [UIColor colorNamed:kTextPrimaryColor].CGColor;
  }
}
#endif

- (void)checkTap:(UITapGestureRecognizer*)sender {
  if (sender.state == UIGestureRecognizerStateEnded) {
    CGPoint location = [sender locationInView:self];
    if ([self pointInside:location withEvent:nil]) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
      [_target performSelector:_action];
#pragma clang diagnostic pop
      sender.state = UIGestureRecognizerStateCancelled;
    }
    [self hide];
  }
}

#pragma mark - UIView overrides

+ (Class)layerClass {
  return [CAShapeLayer class];
}

- (CAShapeLayer*)backgroundLayer {
  return base::apple::ObjCCastStrict<CAShapeLayer>(self.layer);
}

@end
