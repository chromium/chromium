// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/app/remoting_theme.h"

#import <MaterialComponents/MDCAlertColorThemer.h>
#import <MaterialComponents/MDCColorScheme.h>

#include "remoting/base/string_resources.h"
#include "ui/base/l10n/l10n_util.h"

@implementation RemotingTheme

#pragma mark - Colors

+ (UIColor*)dialogBackgroundColor {
  return UIColor.whiteColor;
}

+ (UIColor*)dialogTextColor {
  return UIColor.blackColor;
}

+ (UIColor*)dialogPrimaryButtonTextColor {
  static UIColor* color;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    color = [UIColor colorWithRed:0.29 green:0.58 blue:0.96 alpha:1.0];
  });
  return color;
}

+ (UIColor*)dialogSecondaryButtonTextColor {
  static UIColor* color;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    color = [UIColor colorWithWhite:0.25f alpha:1.f];
  });
  return color;
}

+ (UIColor*)firstLaunchViewBackgroundColor {
  return UIColor.whiteColor;
}

+ (UIColor*)connectionViewBackgroundColor {
  static UIColor* color;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    color = [UIColor colorWithRed:0.06f green:0.12f blue:0.33f alpha:1.f];
  });
  return color;
}

+ (UIColor*)connectionViewForegroundColor {
  return UIColor.whiteColor;
}

+ (UIColor*)hostListBackgroundColor {
  static UIColor* color;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    color = [UIColor colorWithRed:0.11f green:0.23f blue:0.66f alpha:1.f];
  });
  return color;
}

+ (UIColor*)hostListForegroundColor {
  return UIColor.whiteColor;
}

+ (UIColor*)hostListHeaderTitleColor {
  return UIColor.whiteColor;
}

+ (UIColor*)menuBlueColor {
  static UIColor* color;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    color = [UIColor colorWithRed:0.29 green:0.58 blue:0.96 alpha:1.0];
  });
  return color;
}

+ (UIColor*)menuTextColor {
  return UIColor.whiteColor;
}

+ (UIColor*)menuSeparatorColor {
  static UIColor* color;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    color = [UIColor colorWithWhite:1.f alpha:0.4f];
  });
  return color;
}

+ (UIColor*)pinEntryPairingColor {
  return UIColor.whiteColor;
}

+ (UIColor*)pinEntryPlaceholderColor {
  static UIColor* color;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    color = [UIColor colorWithWhite:1.f alpha:0.5f];
  });
  return color;
}

+ (UIColor*)pinEntryTextColor {
  return UIColor.whiteColor;
}

+ (UIColor*)hostOfflineColor {
  static UIColor* color;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    color = [UIColor colorWithWhite:0.87f alpha:1.f];
  });
  return color;
}

+ (UIColor*)hostOnlineColor {
  static UIColor* color;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    color = [UIColor colorWithRed:0.40f green:0.75f blue:0.40f alpha:1.f];
  });
  return color;
}

+ (UIColor*)hostWarningColor {
  static UIColor* color;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    color = [UIColor colorWithRed:1.f green:0.60f blue:0.f alpha:1.f];
  });
  return color;
}

+ (UIColor*)hostErrorColor {
  static UIColor* color;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    color = [UIColor colorWithRed:249.f / 255.f
                            green:146.f / 255.f
                             blue:34.f / 255.f
                            alpha:1.f];
  });
  return color;
}

+ (UIColor*)buttonBackgroundColor {
  static UIColor* color;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    color = [UIColor colorWithRed:0.11f green:0.55f blue:0.95f alpha:1.f];
  });
  return color;
}

+ (UIColor*)buttonTextColor {
  return UIColor.whiteColor;
}

+ (UIColor*)flatButtonTextColor {
  static UIColor* color;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    color = [UIColor colorWithRed:0.11f green:0.55f blue:0.95f alpha:1.f];
  });
  return color;
}

+ (UIColor*)refreshIndicatorColor {
  static UIColor* color;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    color = UIColor.whiteColor;
  });
  return color;
}

+ (UIColor*)hostCellTitleColor {
  static UIColor* color;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    color = [UIColor colorWithWhite:0 alpha:0.87f];
  });
  return color;
}

+ (UIColor*)hostCellStatusTextColor {
  return UIColor.blackColor;
}

+ (UIColor*)setupListBackgroundColor {
  static UIColor* color;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    color = [UIColor colorWithWhite:1.f alpha:0.9f];
  });
  return color;
}

+ (UIColor*)setupListTextColor {
  static UIColor* color;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    color = [UIColor colorWithWhite:0.38f alpha:1.f];
  });
  return color;
}

+ (UIColor*)setupListNumberColor {
  return UIColor.whiteColor;
}

+ (UIColor*)sideMenuIconColor {
  static UIColor* color;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    color = [UIColor colorWithWhite:0.f alpha:0.54f];
  });
  return color;
}

#pragma mark - Icons

+ (UIImage*)arrowIcon {
  static UIImage* icon;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    icon = [UIImage imageNamed:@"ic_arrow_forward_white"];
  });
  return icon;
}

+ (UIImage*)backIcon {
  static UIImage* icon;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    icon = [UIImage imageNamed:@"ic_chevron_left_white_36pt"];
  });
  return icon;
}

+ (UIImage*)checkboxCheckedIcon {
  static UIImage* icon;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    icon = [UIImage imageNamed:@"ic_check_box_white"];
  });
  return icon;
}

+ (UIImage*)checkboxOutlineIcon {
  static UIImage* icon;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    icon = [UIImage imageNamed:@"ic_check_box_outline_blank_white"];
  });
  return icon;
}

+ (UIImage*)closeIcon {
  static UIImage* icon;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    icon = [UIImage imageNamed:@"ic_close_white"];
    icon.accessibilityLabel = l10n_util::GetNSString(IDS_CLOSE);
  });
  return icon;
}

+ (UIImage*)desktopIcon {
  static UIImage* icon;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    icon = [UIImage imageNamed:@"ic_desktop_windows_white"];
  });
  return icon;
}

+ (UIImage*)menuIcon {
  static UIImage* icon;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    icon = [UIImage imageNamed:@"ic_menu_white"];
    icon.accessibilityLabel = l10n_util::GetNSString(IDS_ACTIONBAR_MENU);
  });
  return icon;
}

+ (UIImage*)radioCheckedIcon {
  static UIImage* icon;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    icon = [UIImage imageNamed:@"ic_radio_button_checked_white"];
  });
  return icon;
}

+ (UIImage*)radioOutlineIcon {
  static UIImage* icon;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    icon = [UIImage imageNamed:@"ic_radio_button_unchecked_white"];
  });
  return icon;
}

+ (UIImage*)refreshIcon {
  static UIImage* icon;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    icon = [UIImage imageNamed:@"ic_refresh_white"];
  });
  return icon;
}

+ (UIImage*)settingsIcon {
  static UIImage* icon;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    icon = [UIImage imageNamed:@"ic_settings_white"];
  });
  return icon;
}

+ (UIImage*)helpIcon {
  static UIImage* icon;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    icon = [[UIImage imageNamed:@"ic_help"]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  });
  return icon;
}

+ (UIImage*)feedbackIcon {
  static UIImage* icon;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    icon = [[UIImage imageNamed:@"ic_feedback"]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  });
  return icon;
}

@end
