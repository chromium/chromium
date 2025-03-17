// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/youtube_incognito/ui/youtube_incognito_enterprise_sheet.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/youtube_incognito/ui/youtube_incognito_sheet_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

CGFloat const kVerticalSpacing = 20;
CGFloat const kTitleContainerCornerRadius = 15;
CGFloat const kTitleContainerTopPadding = 33;
CGFloat const kTitleContainerViewSize = 64;
CGFloat const kIconSize = 32;

}  // namespace

@implementation YoutubeIncognitoEnterpriseSheet

- (instancetype)init {
  self = [super init];
  return self;
}

- (void)viewDidLoad {
  self.actionHandler = self;
  self.showDismissBarButton = NO;
  self.overrideUserInterfaceStyle = UIUserInterfaceStyleLight;
  self.aboveTitleView = [self enterpriseTitleView];

  self.titleString =
      l10n_util::GetNSString(IDS_IOS_YOUTUBE_INCOGNITO_ENTERPRISE_TITLE);
  self.secondaryTitleString =
      l10n_util::GetNSString(IDS_IOS_YOUTUBE_INCOGNITO_ENTERPRISE_SUBTITLE);
  self.subtitleString = self.URLText;
  self.primaryActionString = l10n_util::GetNSString(
      IDS_IOS_YOUTUBE_INCOGNITO_ENTERPRISE_PRIMARY_BUTTON_TITLE);
  self.secondaryActionString = l10n_util::GetNSString(
      IDS_IOS_YOUTUBE_INCOGNITO_ENTERPRISE_SECONDARY_BUTTON_TITLE);
  self.customSpacing = kVerticalSpacing;
  self.titleTextStyle = UIFontTextStyleTitle3;
  self.scrollEnabled = YES;
  self.topAlignedLayout = YES;

  [self displayGradientView:YES];

  [super viewDidLoad];
}

- (void)customizeSecondaryTitle:(UITextView*)secondaryTitle {
  secondaryTitle.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  secondaryTitle.textColor = [UIColor colorNamed:kTextSecondaryColor];
}

- (void)customizeSubtitle:(UITextView*)subtitle {
  subtitle.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self.delegate didTapPrimaryActionButton];
}

- (void)confirmationAlertSecondaryAction {
  [self.delegate didTapSecondaryActionButton];
}

#pragma mark - Private

- (UIView*)enterpriseTitleView {
  UIView* iconContainerView = [[UIView alloc] init];
  iconContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  iconContainerView.layer.cornerRadius = kTitleContainerCornerRadius;
  iconContainerView.backgroundColor = [UIColor colorNamed:kGrey400Color];

  UIImageView* icon = [[UIImageView alloc]
      initWithImage:CustomSymbolWithPointSize(kEnterpriseSymbol, kIconSize)];
  icon.translatesAutoresizingMaskIntoConstraints = NO;
  icon.tintColor = [UIColor colorNamed:kSolidWhiteColor];
  [iconContainerView addSubview:icon];

  [NSLayoutConstraint activateConstraints:@[
    [iconContainerView.widthAnchor
        constraintEqualToConstant:kTitleContainerViewSize],
    [iconContainerView.heightAnchor
        constraintEqualToConstant:kTitleContainerViewSize],
  ]];
  AddSameCenterConstraints(iconContainerView, icon);

  // Padding for the icon container view.
  UIView* outerView = [[UIView alloc] init];
  [outerView addSubview:iconContainerView];
  AddSameCenterXConstraint(outerView, iconContainerView);
  AddSameConstraintsToSidesWithInsets(
      iconContainerView, outerView, LayoutSides::kTop | LayoutSides::kBottom,
      NSDirectionalEdgeInsetsMake(kTitleContainerTopPadding, 0, 0, 0));

  return outerView;
}

@end
