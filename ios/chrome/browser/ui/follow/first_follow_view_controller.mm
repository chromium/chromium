// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/follow/first_follow_view_controller.h"

#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/ui/follow/first_follow_view_delegate.h"
#import "ios/chrome/browser/ui/follow/followed_web_channel.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "ios/chrome/common/ui/favicon/favicon_container_view.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
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

  UIView* faviconView = [self faviconView];

  // TODO(crbug.com/1312124): Polish this UI and add favicon.
  UILabel* titleLabel = [self
      labelWithText:l10n_util::GetNSStringF(
                        IDS_IOS_FIRST_FOLLOW_TITLE,
                        base::SysNSStringToUTF16(self.followedWebChannel.title))
          textStyle:UIFontTextStyleTitle1];
  UILabel* subTitleLabel =
      [self labelWithText:l10n_util::GetNSString(IDS_IOS_FIRST_FOLLOW_SUBTITLE)
                textStyle:UIFontTextStyleHeadline];
  UILabel* bodyLabel =
      [self labelWithText:l10n_util::GetNSString(IDS_IOS_FIRST_FOLLOW_BODY)
                textStyle:UIFontTextStyleBody];
  UIButton* goToFeedButton = [self filledGoToFeedButton];
  UIButton* gotItButton = [self plainGotItButton];

  // Set colors.
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  subTitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  bodyLabel.textColor = [UIColor colorNamed:kTextTertiaryColor];
  goToFeedButton.backgroundColor = [UIColor colorNamed:kBlueColor];
  gotItButton.backgroundColor = [UIColor clearColor];
  [goToFeedButton setTitleColor:[UIColor colorNamed:kSolidButtonTextColor]
                       forState:UIControlStateNormal];
  [gotItButton setTitleColor:[UIColor colorNamed:kBlueColor]
                    forState:UIControlStateNormal];

  // Go To Feed button is only displayed if the web channel is available.
  NSArray* subviews = nil;
  if (self.followedWebChannel.available) {
    subviews = @[
      faviconView, titleLabel, subTitleLabel, bodyLabel, goToFeedButton,
      gotItButton
    ];
  } else {
    subviews =
        @[ faviconView, titleLabel, subTitleLabel, bodyLabel, gotItButton ];
  }
  UIStackView* verticalStack =
      [[UIStackView alloc] initWithArrangedSubviews:subviews];
  verticalStack.axis = UILayoutConstraintAxisVertical;
  verticalStack.distribution = UIStackViewDistributionFill;
  verticalStack.alignment = UIStackViewAlignmentCenter;
  verticalStack.spacing = kStackViewSubViewSpacing;
  verticalStack.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:verticalStack];

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

// Calls delegate to go to feed and dismisses the sheet.
- (void)handleGoToFeedTapped {
  [self.delegate handleGoToFeedTapped];
  [self.presentingViewController dismissViewControllerAnimated:YES
                                                    completion:nil];
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

- (UIView*)faviconView {
  FaviconContainerView* faviconContainerView =
      [[FaviconContainerView alloc] init];
  faviconContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  self.faviconLoader->FaviconForPageUrl(
      self.followedWebChannel.channelURL.gurl, kDesiredSmallFaviconSizePt,
      kMinFaviconSizePt,
      /*fallback_to_google_server=*/true, ^(FaviconAttributes* attributes) {
        [faviconContainerView.faviconView configureWithAttributes:attributes];
      });

  UIImageView* faviconBadgeView = [[UIImageView alloc] init];
  faviconBadgeView.translatesAutoresizingMaskIntoConstraints = NO;
  faviconBadgeView.image = [UIImage imageNamed:@"table_view_cell_check_mark"];

  UIView* view = [[UIView alloc] init];
  [view addSubview:faviconContainerView];
  [view addSubview:faviconBadgeView];

  [NSLayoutConstraint activateConstraints:@[
    [view.leadingAnchor
        constraintEqualToAnchor:faviconContainerView.leadingAnchor],
    [view.trailingAnchor
        constraintEqualToAnchor:faviconBadgeView.trailingAnchor],
    [view.topAnchor constraintEqualToAnchor:faviconBadgeView.topAnchor],
    [view.bottomAnchor
        constraintEqualToAnchor:faviconContainerView.bottomAnchor],
    [faviconBadgeView.centerYAnchor
        constraintEqualToAnchor:faviconContainerView.topAnchor],
    [faviconBadgeView.centerXAnchor
        constraintEqualToAnchor:faviconContainerView.trailingAnchor],
  ]];
  return view;
}

// Returns a filled button.
- (UIButton*)filledGoToFeedButton {
  UIButton* button = [[UIButton alloc] init];
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
