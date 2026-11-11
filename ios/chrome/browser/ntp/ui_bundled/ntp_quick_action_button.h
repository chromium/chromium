// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NTP_QUICK_ACTION_BUTTON_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NTP_QUICK_ACTION_BUTTON_H_

#import <UIKit/UIKit.h>

// Layout variants for `NTPQuickActionButton`.
enum class NTPQuickActionButtonLayout {
  // In compact layout, only the icon should be shown.
  kCompact = 0,
  // In regular layout, a quick action button will be shown with its title and
  // icon visible.
  kRegular = 1,
  kMaxValue = kRegular,
};

// Custom button for NTP Quick Actions.
@interface NTPQuickActionButton : UIButton

// The layout variant used by this button.
@property(nonatomic, assign) NTPQuickActionButtonLayout layoutType;

// The icon displayed in this button.
@property(nonatomic, strong) UIImage* icon;

// The text displayed by this button when in Regular layout mode.
@property(nonatomic, copy) NSString* titleText;

// Configured buttons are provided by convenience initializers.
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame
                primaryAction:(UIAction*)primaryAction NS_UNAVAILABLE;

// Returns an AI Mode Quick Action button.
+ (instancetype)aimButtonWithTitle:(NSString*)title
                              icon:(UIImage*)icon
                accessibilityLabel:(NSString*)accessibilityLabel
                        layoutType:(NTPQuickActionButtonLayout)layoutType;

// Returns a Quick Action button for opening AI Mode with the Image Generation
// chip active.
+ (instancetype)aimImageGenerationButton:(NTPQuickActionButtonLayout)layoutType;

// Returns a Quick Action button for attaching media when triggering AI Mode.
+ (instancetype)aimAttachImageButton:(NTPQuickActionButtonLayout)layoutType;

// Returns an Incognito search Quick Action button.
+ (instancetype)incognitoSearchButton:(NTPQuickActionButtonLayout)layoutType;

// Updates the title, icon, and accessibility label of the button. Primarily
// used to update the AI Mode button for third-party AI mode button providers.
- (void)updateTitle:(NSString*)title
                  icon:(UIImage*)icon
    accessibilityLabel:(NSString*)accessibilityLabel;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NTP_QUICK_ACTION_BUTTON_H_
