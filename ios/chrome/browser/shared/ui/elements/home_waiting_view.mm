// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/home_waiting_view.h"

#import <MaterialComponents/MaterialActivityIndicator.h>

#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@interface HomeWaitingView () <MDCActivityIndicatorDelegate>
@property(nonatomic, retain) MDCActivityIndicator* activityIndicator;
@property(nonatomic, copy) ProceduralBlock animateOutCompletionBlock;
@end

@implementation HomeWaitingView

@synthesize activityIndicator = _activityIndicator;
@synthesize animateOutCompletionBlock = _animateOutCompletionBlock;

- (instancetype)initWithFrame:(CGRect)frame backgroundColor:(UIColor*)color {
  self = [super initWithFrame:frame];
  if (self) {
    self.backgroundColor = color;
    self.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  }
  return self;
}

- (void)startWaiting {
  dispatch_time_t delayForIndicatorAppearance =
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC));
  dispatch_after(delayForIndicatorAppearance, dispatch_get_main_queue(), ^{
    MDCActivityIndicator* activityIndicator =
        [[MDCActivityIndicator alloc] initWithFrame:CGRectMake(0, 0, 24, 24)];
    self.activityIndicator = activityIndicator;
    self.activityIndicator.delegate = self;
    self.activityIndicator.autoresizingMask =
        UIViewAutoresizingFlexibleLeadingMargin() |
        UIViewAutoresizingFlexibleTopMargin |
        UIViewAutoresizingFlexibleTrailingMargin() |
        UIViewAutoresizingFlexibleBottomMargin;
    self.activityIndicator.center = CGPointMake(
        CGRectGetWidth(self.bounds) / 2, CGRectGetHeight(self.bounds) / 2);
    self.activityIndicator.cycleColors = @[ [UIColor colorNamed:kBlueColor] ];
    [self addSubview:self.activityIndicator];
    [self.activityIndicator startAnimating];
  });
}

- (void)stopWaitingWithCompletion:(ProceduralBlock)completion {
  if (self.activityIndicator) {
    self.animateOutCompletionBlock = completion;
    [self.activityIndicator stopAnimating];
  } else if (completion) {
    completion();
  }
}

#pragma mark - MDCActivityIndicatorDelegate

- (void)activityIndicatorAnimationDidFinish:
    (MDCActivityIndicator*)activityIndicator {
  [self.activityIndicator removeFromSuperview];
  self.activityIndicator = nil;
  if (self.animateOutCompletionBlock) {
    self.animateOutCompletionBlock();
  }
  self.animateOutCompletionBlock = nil;
}

@end
