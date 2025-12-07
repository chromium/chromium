// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/consent_view_controller.h"

#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/app_group/app_group_utils.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {
NSString* const kConsentViewControllerIdentifier =
    @"ConsentViewControllerIdentifier";
}  // namespace

@implementation ConsentViewController

#pragma mark - Initialization

- (instancetype)init {
  return [super initWithTaskRunner:nullptr];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.view.accessibilityIdentifier = kConsentViewControllerIdentifier;
  self.bannerName = @"consent_view_controller";

  NSString* userEmail = app_group::UserDefaultsStringForKey(
      AppGroupUserDefaultsCredentialProviderUserEmail(),
      /*default_value=*/@"");

  if (userEmail.length) {
    NSString* baseLocalizedString = NSLocalizedString(
        @"IDS_IOS_CREDENTIAL_PROVIDER_CONSENT_SUBTITLE_BRANDED_SYNC",
        @"The subtitle in the consent screen.");
    self.subtitleText =
        [baseLocalizedString stringByReplacingOccurrencesOfString:@"$1"
                                                       withString:userEmail];
  } else {
    self.subtitleText = NSLocalizedString(
        @"IDS_IOS_CREDENTIAL_PROVIDER_CONSENT_SUBTITLE_BRANDED_NO_SYNC",
        @"The subtitle in the consent screen.");
  }

  self.titleText =
      NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_CONSENT_TITLE",
                        @"The title in the consent screen.");
  self.bannerSize = BannerImageSizeType::kStandard;
  self.shouldShowLearnMoreButton = YES;
  // Primary action button is initialized regardless of the visibility set and
  // the view crashes without this value set.
  self.configuration.primaryActionString = @"";
  self.actionButtonsVisibility = ActionButtonsVisibility::kHidden;
  self.shouldShowDismissButton = YES;
  self.dismissButtonString = NSLocalizedString(
      @"IDS_IOS_CREDENTIAL_PROVIDER_DONE", @"The label of the done button.");

  // Add consent view specific content.
  UILabel* captionLabel = [self drawCaptionLabel];
  [self.specificContentView addSubview:captionLabel];
  [NSLayoutConstraint activateConstraints:@[
    [captionLabel.topAnchor
        constraintEqualToAnchor:self.specificContentView.topAnchor],
    [captionLabel.centerXAnchor
        constraintEqualToAnchor:self.specificContentView.centerXAnchor],
    [captionLabel.widthAnchor
        constraintLessThanOrEqualToAnchor:self.specificContentView.widthAnchor],
    [captionLabel.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.specificContentView
                                              .bottomAnchor],
  ]];
  [super viewDidLoad];
}

#pragma mark - Private

- (UILabel*)drawCaptionLabel {
  UILabel* captionLabel = [[UILabel alloc] init];
  captionLabel.text = NSLocalizedString(
      @"IDS_IOS_CREDENTIAL_PROVIDER_CONSENT_CAPTION",
      @"Caption below subtitle to show when enabling the extension");
  captionLabel.numberOfLines = 0;
  captionLabel.textAlignment = NSTextAlignmentCenter;
  captionLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  captionLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  captionLabel.translatesAutoresizingMaskIntoConstraints = NO;
  captionLabel.adjustsFontForContentSizeCategory = YES;
  return captionLabel;
}

@end
