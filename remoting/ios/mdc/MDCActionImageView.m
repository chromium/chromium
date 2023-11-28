// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/mdc/MDCActionImageView.h"

#import <Foundation/Foundation.h>

static const CGFloat kIconRotationRadians = 0.375f * 2 * M_PI;
static const CGFloat kIconShrinkScale = 0.4f;

@implementation MDCActionImageView {
  UIImageView* _primaryIcon;
  UIImageView* _secondaryIcon;
}

@synthesize active = _active;

- (id)initWithFrame:(CGRect)frame
       primaryImage:(UIImage*)primary
        activeImage:(UIImage*)active {
  if ((self = [super initWithFrame:frame])) {
    self.userInteractionEnabled = NO;

    UIViewAutoresizing autoresizingMask =
        UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleWidth;

    _primaryIcon = [[UIImageView alloc] initWithFrame:frame];
    _primaryIcon.image = primary;
    _primaryIcon.contentMode = UIViewContentModeCenter;
    _primaryIcon.autoresizingMask = autoresizingMask;
    [self addSubview:_primaryIcon];

    _secondaryIcon = [[UIImageView alloc] initWithFrame:frame];
    _secondaryIcon.image = active;
    _secondaryIcon.contentMode = UIViewContentModeCenter;
    _secondaryIcon.autoresizingMask = autoresizingMask;
    [self addSubview:_secondaryIcon];

    self.active = NO;
  }
  return self;
}

- (void)setActive:(BOOL)active {
  _active = active;
  if (_active) {
    // Show the secondary icon and reset it to normal transform. Hide the
    // primary icon while rotating it CCW.
    _primaryIcon.alpha = 0.f;
    _primaryIcon.transform = CGAffineTransformScale(
        CGAffineTransformMakeRotation(kIconRotationRadians), kIconShrinkScale,
        kIconShrinkScale);

    _secondaryIcon.alpha = 1.f;
    _secondaryIcon.transform = CGAffineTransformIdentity;
  } else {
    // Show the primary icon and reset it to normal transform. Hide the
    // secondary icon while rotating it CCW.
    _primaryIcon.alpha = 1.f;
    _primaryIcon.transform = CGAffineTransformIdentity;

    _secondaryIcon.alpha = 0.f;
    _secondaryIcon.transform = CGAffineTransformScale(
        CGAffineTransformMakeRotation(-kIconRotationRadians), kIconShrinkScale,
        kIconShrinkScale);
  }
}

- (void)setActive:(BOOL)active animated:(BOOL)animated {
  NSAssert(NO, @"Unimplemented");
}

@end
