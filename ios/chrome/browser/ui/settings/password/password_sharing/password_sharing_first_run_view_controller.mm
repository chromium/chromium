// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_first_run_view_controller.h"

#import "base/check_op.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface PasswordSharingFirstRunViewController () <UITextViewDelegate>
@end

@implementation PasswordSharingFirstRunViewController

@dynamic actionHandler;

- (void)viewDidLoad {
  self.image = [UIImage imageNamed:@"password_sharing_family_promo"];
  self.customSpacingAfterImage = 32;
  self.customSpacingBeforeImageIfNoNavigationBar = 24;
  self.showDismissBarButton = NO;
  self.titleTextStyle = UIFontTextStyleTitle2;
  self.topAlignedLayout = YES;
  self.titleString =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SHARING_FIRST_RUN_TITLE);
  self.subtitleString = [self subtitleStringWithTag].string;
  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SHARING_SHARE_BUTTON);
  self.secondaryActionString = l10n_util::GetNSString(IDS_CANCEL);

  [super viewDidLoad];
}

#pragma mark - ConfirmationAlertViewController

// Sets up styling of the "Learn more" link in the `subtitle`.
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
  [newSubtitle addAttributes:linkAttributes
                       range:[self subtitleStringWithTag].range];
  subtitle.attributedText = newSubtitle;
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  [self.actionHandler learnMoreLinkWasTapped];
  return NO;
}

#pragma mark - Private

// Returns a subtitle string and an NSRange of its "Learn more" link.
- (StringWithTag)subtitleStringWithTag {
  StringWithTags stringWithTags = ParseStringWithLinks(
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SHARING_FIRST_RUN_SUBTITLE));
  CHECK_EQ(stringWithTags.ranges.size(), 1u);
  return {stringWithTags.string, stringWithTags.ranges[0]};
}

@end
