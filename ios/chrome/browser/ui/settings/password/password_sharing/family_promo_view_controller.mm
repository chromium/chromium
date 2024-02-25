// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/family_promo_view_controller.h"

#import "base/check_op.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/family_promo_action_handler.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_constants.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface FamilyPromoViewController () <UITextViewDelegate>
@end

@implementation FamilyPromoViewController {
  // Range of the link in the `subtitleString`.
  NSRange _subtitleLinkRange;
}

@dynamic actionHandler;

- (void)viewDidLoad {
  self.image = [UIImage imageNamed:@"password_sharing_family_promo"];
  self.customSpacingAfterImage = 32;
  self.customSpacingBeforeImageIfNoNavigationBar = 24;
  self.showDismissBarButton = NO;
  self.titleTextStyle = UIFontTextStyleTitle2;
  self.topAlignedLayout = YES;
  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SHARING_FAMILY_PROMO_BUTTON);
  self.view.accessibilityIdentifier = kFamilyPromoViewID;

  [super viewDidLoad];
}

#pragma mark - ConfirmationAlertViewController

- (void)customizeSubtitle:(UITextView*)subtitle {
  subtitle.delegate = self;
  subtitle.selectable = YES;

  // Inherits the default styling already applied to `subtitle`.
  NSMutableAttributedString* newSubtitle = [[NSMutableAttributedString alloc]
      initWithAttributedString:subtitle.attributedText];
  NSDictionary* linkAttributes = @{
    NSLinkAttributeName : @"",
    NSUnderlineStyleAttributeName : @(NSUnderlineStyleSingle)
  };
  [newSubtitle addAttributes:linkAttributes range:_subtitleLinkRange];
  subtitle.attributedText = newSubtitle;
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  [self.actionHandler createFamilyGroupLinkWasTapped];
  return NO;
}

#pragma mark - FamilyPromoConsumer

- (void)setTitle:(NSString*)title subtitle:(NSString*)subtitle {
  self.titleString = title;
  StringWithTags stringWithTags = ParseStringWithLinks(subtitle);
  CHECK_EQ(stringWithTags.ranges.size(), 1u);
  self.subtitleString = stringWithTags.string;
  _subtitleLinkRange = stringWithTags.ranges[0];
}

@end
