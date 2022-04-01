// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/follow/first_follow_view_controller.h"

#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Accessibility identifier for the Go To Feed button.
NSString* const kFirstFollowGoToFeedButtonIdentifier =
    @"FirstFollowGoToFeedButtonIdentifier";

// Accessibility identifier for the Got It button.
NSString* const kFirstFollowGotItButtonIdentifier =
    @"FirstFollowGotItButtonIdentifier";

// Spacing within stackView.
constexpr CGFloat kStackViewSubViewSpacing = 13.0;

// Button corner radius.
constexpr CGFloat kButtonCornerRadius = 8;

}  // namespace

@implementation FirstFollowViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  // TODO(crbug.com/1312124): Polish this UI and add favicon.
  UILabel* titleLabel =
      [self labelWithText:l10n_util::GetNSStringF(
                              IDS_IOS_FIRST_FOLLOW_TITLE,
                              base::SysNSStringToUTF16(self.webChannelTitle))
                textStyle:UIFontTextStyleTitle1];
  UILabel* subTitleLabel =
      [self labelWithText:l10n_util::GetNSString(IDS_IOS_FIRST_FOLLOW_SUBTITLE)
                textStyle:UIFontTextStyleHeadline];
  UILabel* bodyLabel =
      [self labelWithText:l10n_util::GetNSString(IDS_IOS_FIRST_FOLLOW_BODY)
                textStyle:UIFontTextStyleBody];
  UIButton* goToFeedButton = [self filledGoToFeedButton];
  UIButton* gotItButton = [self plainGotItButton];

  UIStackView* verticalStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    titleLabel, subTitleLabel, bodyLabel, goToFeedButton, gotItButton
  ]];
  verticalStack.axis = UILayoutConstraintAxisVertical;
  verticalStack.distribution = UIStackViewDistributionFill;
  verticalStack.alignment = UIStackViewAlignmentCenter;
  verticalStack.spacing = kStackViewSubViewSpacing;
  verticalStack.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:verticalStack];
  self.view.backgroundColor = [UIColor whiteColor];

  [NSLayoutConstraint activateConstraints:@[
    [verticalStack.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [verticalStack.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [verticalStack.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [verticalStack.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.view.bottomAnchor],
  ]];
}

#pragma mark - Helper

// Go To Feed button tapped.
- (void)handleGoToFeedTapped {
  // TODO(crbug.com/1312124): Use a dispatcher to handle this action.
}

// Dismisses the sheet.
- (void)handleGotItTapped {
  [self.presentingViewController dismissViewControllerAnimated:YES
                                                    completion:nil];
}

// Returns a lable with |textStyle|.
- (UILabel*)labelWithText:(NSString*)text textStyle:(UIFontTextStyle)textStyle {
  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.font = [UIFont preferredFontForTextStyle:textStyle];
  label.adjustsFontForContentSizeCategory = YES;
  label.numberOfLines = 0;
  label.textAlignment = NSTextAlignmentCenter;
  label.text = text;
  return label;
}

// Returns a filled button.
- (UIButton*)filledGoToFeedButton {
  UIButton* button = [[UIButton alloc] init];
  button.backgroundColor = [UIColor colorNamed:kBlueColor];
  button.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  button.titleLabel.adjustsFontForContentSizeCategory = YES;
  button.titleLabel.adjustsFontSizeToFitWidth = YES;
  button.titleLabel.textAlignment = NSTextAlignmentCenter;
  button.layer.cornerRadius = kButtonCornerRadius;
  button.clipsToBounds = YES;
  [button setTranslatesAutoresizingMaskIntoConstraints:NO];

  [button setTitle:l10n_util::GetNSString(IDS_IOS_FIRST_FOLLOW_GO_TO_FEED)
          forState:UIControlStateNormal];
  [button setAccessibilityIdentifier:kFirstFollowGoToFeedButtonIdentifier];
  [button addTarget:self
                action:@selector(handleGoToFeedTapped)
      forControlEvents:UIControlEventTouchUpInside];
  return button;
}

// Returns a plain button.
// TODO(crbug.com/1312124): Consolidate button creation code.
- (UIButton*)plainGotItButton {
  UIButton* button = [[UIButton alloc] init];
  [button setTitleColor:[UIColor colorNamed:kBlueColor]
               forState:UIControlStateNormal];
  button.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  button.titleLabel.adjustsFontForContentSizeCategory = YES;
  button.titleLabel.textAlignment = NSTextAlignmentCenter;
  [button setTranslatesAutoresizingMaskIntoConstraints:NO];

  [button setTitle:l10n_util::GetNSString(IDS_IOS_FIRST_FOLLOW_GOT_IT)
          forState:UIControlStateNormal];
  [button setAccessibilityIdentifier:kFirstFollowGotItButtonIdentifier];
  [button addTarget:self
                action:@selector(handleGotItTapped)
      forControlEvents:UIControlEventTouchUpInside];
  return button;
}

@end
