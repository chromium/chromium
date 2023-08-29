// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/family_promo_view_controller.h"

#import "base/check_op.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/family_promo_action_handler.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface FamilyPromoViewController () <UITextViewDelegate>

// The action handler for interactions in this view controller.
@property(nonatomic, weak) id<FamilyPromoActionHandler> actionHandler;

@end

@implementation FamilyPromoViewController

@dynamic actionHandler;

- (instancetype)initWithActionHandler:
    (id<FamilyPromoActionHandler>)actionHandler {
  self = [super init];
  if (!self) {
    return nil;
  }

  self.actionHandler = actionHandler;
  self.modalPresentationStyle = UIModalPresentationPageSheet;
  self.sheetPresentationController.preferredCornerRadius = 20;
  self.sheetPresentationController.prefersEdgeAttachedInCompactHeight = YES;

  if (@available(iOS 16, *)) {
    self.sheetPresentationController.detents = @[
      self.preferredHeightDetent,
      UISheetPresentationControllerDetent.largeDetent,
    ];
  } else {
    self.sheetPresentationController.detents = @[
      UISheetPresentationControllerDetent.mediumDetent,
      UISheetPresentationControllerDetent.largeDetent
    ];
  }

  return self;
}

- (void)viewDidLoad {
  self.image = [UIImage imageNamed:@"password_sharing_family_promo"];
  self.customSpacingAfterImage = 32;
  self.customSpacingBeforeImageIfNoNavigationBar = 24;
  self.showDismissBarButton = NO;
  self.titleTextStyle = UIFontTextStyleTitle2;
  self.topAlignedLayout = YES;
  self.titleString =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SHARING_FAMILY_PROMO_TITLE);
  self.subtitleString = [self subtitleStringWithTag].string;
  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SHARING_FAMILY_PROMO_BUTTON);

  [super viewDidLoad];
}

#pragma mark - ConfirmationAlertViewController

- (void)customizeSubtitle:(UITextView*)subtitle {
  subtitle.delegate = self;
  subtitle.selectable = YES;

  // Inherits the default styling already applied to `subtitle`.
  NSMutableAttributedString* newSubtitle = [[NSMutableAttributedString alloc]
      initWithAttributedString:subtitle.attributedText];
  [newSubtitle addAttribute:NSLinkAttributeName
                      value:@""
                      range:[self subtitleStringWithTag].range];
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

#pragma mark - Private

- (StringWithTag)subtitleStringWithTag {
  StringWithTags stringWithTags = ParseStringWithLinks(
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SHARING_FAMILY_PROMO_SUBTITLE));
  CHECK_EQ(stringWithTags.ranges.size(), 1u);
  return {stringWithTags.string, stringWithTags.ranges[0]};
}

@end
