// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/chrome_overlay_window.h"

#import "base/check.h"
#import "ios/chrome/browser/crash_report/crash_keys_helper.h"
#import "ios/chrome/browser/metrics/user_interface_style_recorder.h"
#import "ui/base/device_form_factor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ChromeOverlayWindow ()
@property(nonatomic, strong)
    UserInterfaceStyleRecorder* userInterfaceStyleRecorder API_AVAILABLE(
        ios(13.0));

// Updates the Breakpad report with the current size class.
- (void)updateBreakpad;

@end

@implementation ChromeOverlayWindow

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    // When not created via a nib, create the recorders immediately.
    [self updateBreakpad];
    _userInterfaceStyleRecorder = [[UserInterfaceStyleRecorder alloc]
        initWithUserInterfaceStyle:self.traitCollection.userInterfaceStyle];
  }
  return self;
}

- (void)awakeFromNib {
  [super awakeFromNib];
  [self updateBreakpad];
}

- (void)updateBreakpad {
  crash_keys::SetCurrentHorizontalSizeClass(
      self.traitCollection.horizontalSizeClass);
  crash_keys::SetCurrentUserInterfaceStyle(
      self.traitCollection.userInterfaceStyle);
}

- (void)setFrame:(CGRect)rect {
  if ((ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) &&
      (rect.origin.x != 0 || rect.origin.y != 0)) {
    // skip, this rect is wrong and probably in portrait while
    // display is in landscape or vice-versa.
  } else {
    [super setFrame:rect];
  }
}

#pragma mark - UITraitEnvironment

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (previousTraitCollection.horizontalSizeClass !=
      self.traitCollection.horizontalSizeClass) {
    [self updateBreakpad];
  }
  if ([self.traitCollection
          hasDifferentColorAppearanceComparedToTraitCollection:
              previousTraitCollection]) {
    [self.userInterfaceStyleRecorder
        userInterfaceStyleDidChange:self.traitCollection.userInterfaceStyle];
  }
  [self updateBreakpad];
}

@end
