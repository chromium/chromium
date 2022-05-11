// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/follow/first_follow_view_controller.h"

#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/follow/first_follow_favicon_data_source.h"
#import "ios/chrome/browser/ui/follow/first_follow_view_delegate.h"
#import "ios/chrome/browser/ui/follow/followed_web_channel.h"
#import "ios/chrome/browser/ui/icons/chrome_symbol.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_container_view.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
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

// Optimal width of content. This is also the maxium.
constexpr CGFloat kContentOptimalWidth = 327;

// Length of each side of the favicon frame (which contains the favicon and the
// surrounding whitespace).
constexpr CGFloat kFaviconFrameSideLength = 60;

// Length of each side of the favicon.
constexpr CGFloat kFaviconSideLength = 30;

// Length of each side of the favicon badge.
constexpr CGFloat kFaviconBadgeSideLength = 24;

// Vertical inset of views inside |self.view|.
constexpr CGFloat kVerticalInset = 10.0;

// Horizontal inset of views inside |self.view|.
constexpr CGFloat kHorizontalInset = 10.0;

// Vertical space between favicon and labels.
constexpr CGFloat kVerticalSpacing = 10.0;

// Vertical spacing between labels and buttons in stack views.
constexpr CGFloat kStackViewVerticalSpacing = 10.0;

// The size of the symbol badge image.
NSInteger kSymbolBadgeImagePointSize = 13;

// Properties of the favicon.
constexpr CGFloat kFaviconCornerRadius = 10;
constexpr CGFloat kFaviconShadowOffsetX = 0;
constexpr CGFloat kFaviconShadowOffsetY = 0;
constexpr CGFloat kFaviconShadowRadius = 10;
constexpr CGFloat kFaviconShadowOpacity = 0.2;

}  // namespace

@implementation FirstFollowViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];

  UIView* containerView = self.view;
  UIView* faviconView = [self faviconView];
  UIView* labelsView = [self labelsView];
  UIView* buttonsView = [self buttonsView];
  [containerView addSubview:faviconView];
  [containerView addSubview:labelsView];
  [containerView addSubview:buttonsView];

  [NSLayoutConstraint activateConstraints:@[
    // faviconView constraints.
    [faviconView.centerXAnchor
        constraintEqualToAnchor:containerView.centerXAnchor],
    [faviconView.topAnchor constraintEqualToAnchor:containerView.topAnchor
                                          constant:kVerticalInset],

    // labelsView constraints.
    [labelsView.centerXAnchor
        constraintEqualToAnchor:containerView.centerXAnchor],
    [labelsView.topAnchor constraintEqualToAnchor:faviconView.bottomAnchor
                                         constant:kVerticalSpacing],
    [labelsView.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:containerView.leadingAnchor
                                    constant:kHorizontalInset],
    [labelsView.trailingAnchor
        constraintLessThanOrEqualToAnchor:containerView.trailingAnchor
                                 constant:-kHorizontalInset],

    // buttonsView constraints.
    [buttonsView.centerXAnchor
        constraintEqualToAnchor:containerView.centerXAnchor],
    [buttonsView.topAnchor
        constraintGreaterThanOrEqualToAnchor:labelsView.bottomAnchor
                                    constant:kVerticalSpacing],
    [buttonsView.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:containerView.leadingAnchor
                                    constant:kHorizontalInset],
    [buttonsView.trailingAnchor
        constraintLessThanOrEqualToAnchor:containerView.trailingAnchor
                                 constant:-kHorizontalInset],
    [buttonsView.bottomAnchor
        constraintEqualToAnchor:containerView.safeAreaLayoutGuide.bottomAnchor
                       constant:-kVerticalInset],
  ]];

  // This constraint ensures a max width by setting the priority slightly
  // higher.
  NSLayoutConstraint* labelsOptimalWidthConstraint =
      [labelsView.widthAnchor constraintEqualToConstant:kContentOptimalWidth];
  NSLayoutConstraint* buttonsOptimalWidthConstraint =
      [buttonsView.widthAnchor constraintEqualToConstant:kContentOptimalWidth];
  labelsOptimalWidthConstraint.priority = UILayoutPriorityDefaultHigh + 1;
  buttonsOptimalWidthConstraint.priority = UILayoutPriorityDefaultHigh + 1;
  [NSLayoutConstraint activateConstraints:@[
    labelsOptimalWidthConstraint, buttonsOptimalWidthConstraint
  ]];
}

#pragma mark - Helpers

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

// Returns a view with a favicon and a check mark badge on the upper right
// corner.
- (UIView*)faviconView {
  FaviconContainerView* faviconContainerView =
      [[FaviconContainerView alloc] init];
  faviconContainerView.translatesAutoresizingMaskIntoConstraints = NO;

  [self.faviconDataSource faviconForURL:self.followedWebChannel.faviconURL
                             completion:^(FaviconAttributes* attributes) {
                               DCHECK(attributes);
                               [faviconContainerView.faviconView
                                   configureWithAttributes:attributes];
                             }];

  UIImageView* faviconBadgeView = [[UIImageView alloc] init];
  faviconBadgeView.translatesAutoresizingMaskIntoConstraints = NO;
  faviconBadgeView.image = DefaultSymbolTemplateWithPointSize(
      kCheckMarkCircleFillSymbol, kSymbolBadgeImagePointSize);
  faviconBadgeView.tintColor = [UIColor colorNamed:kGreenColor];

  UIView* view = [[UIView alloc] init];
  UIView* frameView = [[UIView alloc] init];
  [view addSubview:frameView];
  [view addSubview:faviconBadgeView];
  [frameView addSubview:faviconContainerView];
  view.translatesAutoresizingMaskIntoConstraints = NO;
  frameView.translatesAutoresizingMaskIntoConstraints = NO;

  frameView.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  frameView.layer.cornerRadius = kFaviconCornerRadius;
  frameView.layer.shadowOffset =
      CGSizeMake(kFaviconShadowOffsetX, kFaviconShadowOffsetY);
  frameView.layer.shadowRadius = kFaviconShadowRadius;
  frameView.layer.shadowOpacity = kFaviconShadowOpacity;

  [NSLayoutConstraint activateConstraints:@[
    // Size constraints.
    [frameView.widthAnchor constraintEqualToConstant:kFaviconFrameSideLength],
    [frameView.heightAnchor constraintEqualToConstant:kFaviconFrameSideLength],
    [faviconContainerView.widthAnchor
        constraintEqualToConstant:kFaviconSideLength],
    [faviconContainerView.heightAnchor
        constraintEqualToConstant:kFaviconSideLength],
    [faviconBadgeView.widthAnchor
        constraintEqualToConstant:kFaviconBadgeSideLength],
    [faviconBadgeView.heightAnchor
        constraintEqualToConstant:kFaviconBadgeSideLength],

    // Badge is on the upper right corner of the frame.
    [frameView.topAnchor
        constraintEqualToAnchor:faviconBadgeView.centerYAnchor],
    [frameView.trailingAnchor
        constraintEqualToAnchor:faviconBadgeView.centerXAnchor],

    // Favicon is centered in the frame.
    [frameView.centerXAnchor
        constraintEqualToAnchor:faviconContainerView.centerXAnchor],
    [frameView.centerYAnchor
        constraintEqualToAnchor:faviconContainerView.centerYAnchor],

    // Frame and badge define the whole view returned by this method.
    [view.leadingAnchor constraintEqualToAnchor:frameView.leadingAnchor],
    [view.bottomAnchor constraintEqualToAnchor:frameView.bottomAnchor],
    [view.topAnchor constraintEqualToAnchor:faviconBadgeView.topAnchor],
    [view.trailingAnchor
        constraintEqualToAnchor:faviconBadgeView.trailingAnchor],
  ]];
  return view;
}

// Returns a stack view with the title, subtitle, and body labels.
- (UIView*)labelsView {
  // Set strings.
  UILabel* titleLabel = [self
      labelWithText:l10n_util::GetNSStringF(
                        IDS_IOS_FIRST_FOLLOW_TITLE,
                        base::SysNSStringToUTF16(self.followedWebChannel.title))
          textStyle:UIFontTextStyleTitle1];
  UILabel* subTitleLabel = [self
      labelWithText:l10n_util::GetNSStringF(
                        IDS_IOS_FIRST_FOLLOW_SUBTITLE,
                        base::SysNSStringToUTF16(self.followedWebChannel.title))
          textStyle:UIFontTextStyleHeadline];
  UILabel* bodyLabel =
      [self labelWithText:l10n_util::GetNSString(IDS_IOS_FIRST_FOLLOW_BODY)
                textStyle:UIFontTextStyleBody];

  // Set colors.
  titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  subTitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  bodyLabel.textColor = [UIColor colorNamed:kTextTertiaryColor];

  UIStackView* verticalStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ titleLabel, subTitleLabel, bodyLabel ]];

  verticalStack.axis = UILayoutConstraintAxisVertical;
  verticalStack.distribution = UIStackViewDistributionFillProportionally;
  verticalStack.alignment = UIStackViewAlignmentCenter;
  verticalStack.spacing = kStackViewVerticalSpacing;
  verticalStack.translatesAutoresizingMaskIntoConstraints = NO;

  return verticalStack;
}

// Returns a label with |textStyle|.
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

// Returns a stack view with configured buttons.
- (UIView*)buttonsView {
  UIButton* goToFeedButton = nil;
  UIButton* gotItButton = nil;
  if (self.followedWebChannel.available) {
    // Go To Feed button is only displayed if the web channel is available.
    goToFeedButton = PrimaryActionButton(YES);
    gotItButton = [self plainButton];
  } else {
    // Only one button is visible, and it is a primary action button (with a
    // solid background color).
    gotItButton = PrimaryActionButton(YES);
  }

  // Configure buttons.
  if (goToFeedButton) {
    [goToFeedButton
        setTitle:l10n_util::GetNSString(IDS_IOS_FIRST_FOLLOW_GO_TO_FEED)
        forState:UIControlStateNormal];
    [goToFeedButton
        setAccessibilityIdentifier:kFirstFollowGoToFeedButtonIdentifier];
    [goToFeedButton addTarget:self
                       action:@selector(handleGoToFeedTapped)
             forControlEvents:UIControlEventTouchUpInside];
  }
  [gotItButton setTitle:l10n_util::GetNSString(IDS_IOS_FIRST_FOLLOW_GOT_IT)
               forState:UIControlStateNormal];
  [gotItButton setAccessibilityIdentifier:kFirstFollowGotItButtonIdentifier];
  [gotItButton addTarget:self
                  action:@selector(handleGotItTapped)
        forControlEvents:UIControlEventTouchUpInside];

  NSArray* buttons =
      (goToFeedButton) ? @[ goToFeedButton, gotItButton ] : @[ gotItButton ];
  UIStackView* verticalStack =
      [[UIStackView alloc] initWithArrangedSubviews:buttons];
  verticalStack.axis = UILayoutConstraintAxisVertical;
  verticalStack.distribution = UIStackViewDistributionFill;
  verticalStack.alignment = UIStackViewAlignmentFill;
  verticalStack.spacing = kStackViewVerticalSpacing;
  verticalStack.translatesAutoresizingMaskIntoConstraints = NO;
  return verticalStack;
}

// Returns a plain button.
- (UIButton*)plainButton {
  UIButton* button = [[UIButton alloc] init];
  button.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  button.titleLabel.adjustsFontForContentSizeCategory = YES;
  button.titleLabel.textAlignment = NSTextAlignmentCenter;
  [button setTranslatesAutoresizingMaskIntoConstraints:NO];
  [button setTitleColor:[UIColor colorNamed:kBlueColor]
               forState:UIControlStateNormal];
  return button;
}

@end
