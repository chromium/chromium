// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reminder_notifications/ui/reminder_notifications_view_controller.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Point size of the bell badge symbol.
constexpr CGFloat kBellBadgeSymbolPointSize = 60;

// Size of the frame holding the symbol.
constexpr CGFloat kCustomFaviconSideLength = 42;

// Spacing before the image when there's no navigation bar.
constexpr CGFloat kSpacingBeforeImage = 16;

// Spacing after the image before the title.
constexpr CGFloat kSpacingAfterImage = 16;

// Accessibility identifier for the bell icon.
NSString* const kBellIconAccessibilityLabel =
    @"ReminderNotificationsBellIconAccessibilityLabel";

}  // namespace

@implementation ReminderNotificationsViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  // Configure strings and layout before calling super to ensure proper setup.
  self.titleString =
      l10n_util::GetNSString(IDS_IOS_REMINDER_NOTIFICATIONS_SHEET_TITLE);
  self.subtitleString =
      l10n_util::GetNSString(IDS_IOS_REMINDER_NOTIFICATIONS_DESCRIPTION);

  // Set up action buttons.
  self.primaryActionString = l10n_util::GetNSString(
      IDS_IOS_REMINDER_NOTIFICATIONS_SET_REMINDER_BUTTON);
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_REMINDER_NOTIFICATIONS_CANCEL_BUTTON);

  // Configure title spacing.
  self.titleTextStyle = UIFontTextStyleTitle2;

  // Configure image.
  self.image = SymbolWithPalette(
      DefaultSymbolWithPointSize(kBellBadgeSymbol, kBellBadgeSymbolPointSize),
      @[ [UIColor whiteColor] ]);
  self.imageBackgroundColor = [UIColor colorNamed:kBlue500Color];
  self.imageViewAccessibilityLabel = kBellIconAccessibilityLabel;
  self.customSpacingBeforeImageIfNoNavigationBar = kSpacingBeforeImage;
  self.customSpacingAfterImage = kSpacingAfterImage;
  self.imageHasFixedSize = YES;
  self.imageEnclosedWithShadowWithoutBadge = YES;
  self.customFaviconSideLength = kCustomFaviconSideLength;

  // Configure layout preferences.
  self.showDismissBarButton = NO;
  self.topAlignedLayout = YES;

  // Since this is a half-sheet with minimal content, disable scrolling.
  self.scrollEnabled = NO;
  self.showsVerticalScrollIndicator = NO;

  [super viewDidLoad];
}

@end
