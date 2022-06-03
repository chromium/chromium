// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/consent_view_controller.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
NSString* const kConsentViewControllerIdentifier =
    @"ConsentViewControllerIdentifier";
}  // namespace

@implementation ConsentViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.view.accessibilityIdentifier = kConsentViewControllerIdentifier;
  self.titleText =
      NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_CONSENT_TITLE",
                        @"The title in the consent screen.");
  self.subtitleText =
      NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_CONSENT_SUBTITLE",
                        @"The subtitle in the consent screen.");
  self.bannerImage = [UIImage imageNamed:@"consent_view_controller"];
  self.isTallBanner = NO;
  self.shouldShowLearnMoreButton = YES;
  self.primaryActionString = NSLocalizedString(
      @"IDS_IOS_CREDENTIAL_PROVIDER_CONSENT_BUTTON_TITLE",
      @"The primary action title in the consent screen. Used to enable the "
      @"extension and dismiss the view");

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
  captionLabel.textColor = [UIColor colorNamed:kGrey600Color];
  captionLabel.translatesAutoresizingMaskIntoConstraints = NO;
  captionLabel.adjustsFontForContentSizeCategory = YES;
  return captionLabel;
}
@end
