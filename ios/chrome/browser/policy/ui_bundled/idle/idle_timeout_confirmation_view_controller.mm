// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/ui_bundled/idle/idle_timeout_confirmation_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/policy/ui_bundled/idle/constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
constexpr CGFloat kCustomSpacingBeforeImageIfNoNavigationBar = 24.0;
constexpr CGFloat kCustomSpacingAfterImage = 1.0;
}  // namespace

@implementation IdleTimeoutConfirmationViewController {
  // Text view showing countdown until the dialog is dismissed.
  UITextView* _timeRemainingTextView;
}

- (instancetype)initWithIdleTimeoutTitleId:(int)titleId
                     idleTimeoutSubtitleId:(int)subtitleId
                      idleTimeoutThreshold:(int)threshold {
  if ((self = [super init])) {
    self.titleString = l10n_util::GetNSString(titleId);
    self.subtitleString = base::SysUTF16ToNSString(
        l10n_util::GetPluralStringFUTF16(subtitleId, threshold));
    _timeRemainingTextView = [self createUnderTitleViewTextView];
    self.underTitleView = _timeRemainingTextView;
    self.primaryActionString =
        l10n_util::GetNSString(IDS_IOS_IDLE_TIMEOUT_CONTINUE_USING_CHROME);
  }
  return self;
}

// Creates a UITextView matching the subtitle defaults.
- (UITextView*)createUnderTitleViewTextView {
  UITextView* view = [[UITextView alloc] init];
  view.textAlignment = NSTextAlignmentCenter;
  view.translatesAutoresizingMaskIntoConstraints = NO;
  view.adjustsFontForContentSizeCategory = YES;
  view.editable = NO;
  view.selectable = NO;
  view.scrollEnabled = NO;
  view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  view.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  view.textColor = [UIColor colorNamed:kTextSecondaryColor];
  view.accessibilityIdentifier =
      kConfirmationAlertUnderTitleViewAccessibilityIdentifier;
  return view;
}

#pragma mark - IdleTimeoutConfirmationConsumer

- (void)setCountdown:(base::TimeDelta)countdown {
  _timeRemainingTextView.text =
      base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
          IDS_IOS_IDLE_TIMEOUT_CONFIRMATION_DIALOG_REMAINING_TIME_LABEL,
          countdown.InSeconds()));
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.view.accessibilityIdentifier = kIdleTimeoutDialogAccessibilityIdentifier;
  self.image = [UIImage imageNamed:@"enterprise_grey_icon_large"];
  self.imageHasFixedSize = YES;
  self.imageViewAccessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_IDLE_TIMEOUT_DIALOG_ACCESSIBILITY_LABEL);

  self.showDismissBarButton = NO;
  self.dismissBarButtonSystemItem = UIBarButtonSystemItemDone;

  self.titleTextStyle = UIFontTextStyleTitle2;
  // Icon already contains some spacing for the shadow.
  self.customSpacingBeforeImageIfNoNavigationBar =
      kCustomSpacingBeforeImageIfNoNavigationBar;
  self.customSpacingAfterImage = kCustomSpacingAfterImage;
  self.topAlignedLayout = YES;
  [super viewDidLoad];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification,
                                  self.imageViewAccessibilityLabel);
}

@end
