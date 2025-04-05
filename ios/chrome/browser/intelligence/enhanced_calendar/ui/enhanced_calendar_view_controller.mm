// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/enhanced_calendar/ui/enhanced_calendar_view_controller.h"

#import "ios/chrome/browser/intelligence/enhanced_calendar/ui/enhanced_calendar_mutator.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The header image SF Symbol name.
constexpr NSString* kHeaderImageName = @"wand.and.sparkles";

// The header image size.
const CGFloat kHeaderImageSize = 80.0;

}  // namespace

@implementation EnhancedCalendarViewController

- (void)viewDidLoad {
  self.actionHandler = self;

  // Configuration.
  self.scrollEnabled = NO;
  self.showDismissBarButton = NO;

  // Titles.
  self.titleString =
      l10n_util::GetNSString(IDS_IOS_ENHANCED_CALENDAR_BOTTOM_SHEET_TITLE);
  self.subtitleString =
      l10n_util::GetNSString(IDS_IOS_ENHANCED_CALENDAR_BOTTOM_SHEET_SUBTITLE);
  self.primaryActionString = l10n_util::GetNSString(
      IDS_IOS_ENHANCED_CALENDAR_BOTTOM_SHEET_CANCEL_BUTTON);

  // Header image.
  self.imageHasFixedSize = YES;
  self.image = DefaultSymbolWithPointSize(kHeaderImageName, kHeaderImageSize);

  // Loading throbber.
  UIActivityIndicatorView* loadingThrobber = [[UIActivityIndicatorView alloc]
      initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleLarge];
  loadingThrobber.translatesAutoresizingMaskIntoConstraints = NO;
  self.underTitleView = loadingThrobber;
  [loadingThrobber startAnimating];

  [super viewDidLoad];
}

- (void)viewDidDisappear:(BOOL)animated {
  [self.mutator cancelEnhancedCalendarRequestAndDismissBottomSheet];
  [super viewDidDisappear:animated];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self.mutator cancelEnhancedCalendarRequestAndDismissBottomSheet];
}

@end
