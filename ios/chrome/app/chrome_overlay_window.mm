// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/chrome_overlay_window.h"

#import "base/check.h"
#import "ios/chrome/browser/crash_report/model/crash_keys_helper.h"
#import "ios/chrome/browser/metrics/model/user_interface_style_recorder.h"
#import "ui/base/device_form_factor.h"

@interface ChromeOverlayWindow ()
@property(nonatomic, strong)
    UserInterfaceStyleRecorder* userInterfaceStyleRecorder API_AVAILABLE(
        ios(13.0));

// Updates the crash keys with the current size class.
- (void)updateCrashKeys;

@end

@implementation ChromeOverlayWindow

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    // When not created via a nib, create the recorders immediately.
    [self updateCrashKeys];
    _userInterfaceStyleRecorder = [[UserInterfaceStyleRecorder alloc]
        initWithUserInterfaceStyle:self.traitCollection.userInterfaceStyle];
    if (@available(iOS 17, *)) {
      NSArray<UITrait>* traits = @[
        UITraitHorizontalSizeClass.class, UITraitUserInterfaceStyle.class
      ];
      [self registerForTraitChanges:traits
                         withAction:@selector(updateCrashKeys)];
    }
  }
  return self;
}

- (void)awakeFromNib {
  [super awakeFromNib];
  [self updateCrashKeys];
}

- (void)updateCrashKeys {
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

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }
  [self updateCrashKeys];
}
#endif

@end
