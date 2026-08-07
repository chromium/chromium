// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/wallet_reminder_notice/ui/wallet_reminder_notice_view_controller.h"

#import "ios/chrome/browser/autofill/model/message/save_card_message_with_links.h"
#import "ios/chrome/browser/autofill/ui_bundled/util/autofill_credit_card_util.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

namespace {

// Spacing before and after the top image illustration.
const CGFloat kCustomSpacingBeforeImage = 32.0;
const CGFloat kCustomSpacingAfterImage = 32.0;

}  // namespace

@interface WalletReminderNoticeViewController () <
    ConfirmationAlertActionHandler,
    UITextViewDelegate>
@end

@implementation WalletReminderNoticeViewController {
  NSArray<SaveCardMessageWithLinks*>* _disclaimerTextLines;
  UIStackView* _disclaimerStackView;
}

- (void)viewDidLoad {
  self.image = [UIImage imageNamed:@"wallet_reminder_branding_illustration"];
  self.customSpacingBeforeImage = kCustomSpacingBeforeImage;
  self.customSpacingAfterImage = kCustomSpacingAfterImage;
  self.customSpacing = 20.0;
  self.actionHandler = self;

  [super viewDidLoad];
}

#pragma mark - WalletReminderNoticeConsumer

- (void)setPrimaryActionString:(NSString*)primaryActionString {
  self.configuration.primaryActionString = primaryActionString;
  [self reloadConfiguration];
}

- (void)setDisclaimerText:(NSArray<SaveCardMessageWithLinks*>*)disclaimerText {
  _disclaimerTextLines = disclaimerText;
  if (!_disclaimerStackView) {
    _disclaimerStackView = [[UIStackView alloc] initWithFrame:CGRectZero];
    _disclaimerStackView.axis = UILayoutConstraintAxisVertical;
    _disclaimerStackView.spacing = 10.0;
    _disclaimerStackView.layoutMarginsRelativeArrangement = YES;
  } else {
    for (UIView* view in _disclaimerStackView.arrangedSubviews) {
      [view removeFromSuperview];
    }
  }

  for (SaveCardMessageWithLinks* message in _disclaimerTextLines) {
    UITextView* legalMessageTextView =
        [AutofillCreditCardUtil createTextViewForLegalMessage:message];
    legalMessageTextView.delegate = self;

    NSMutableAttributedString* attributedText =
        [legalMessageTextView.attributedText mutableCopy];
    if (attributedText) {
      NSRange fullRange = NSMakeRange(0, attributedText.length);
      UIFont* font = [UIFont systemFontOfSize:15.0 weight:UIFontWeightRegular];
      UIFontMetrics* fontMetrics =
          [UIFontMetrics metricsForTextStyle:UIFontTextStyleSubheadline];
      [attributedText addAttribute:NSFontAttributeName
                             value:[fontMetrics scaledFontForFont:font]
                             range:fullRange];
      [attributedText addAttribute:NSKernAttributeName
                             value:@(-0.33)
                             range:fullRange];
      legalMessageTextView.attributedText = attributedText;
    }

    [_disclaimerStackView addArrangedSubview:legalMessageTextView];
  }

  self.underTitleView = _disclaimerStackView;
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self.delegate walletReminderNoticeViewControllerDidTapPrimaryAction:self];
}

#pragma mark - UITextViewDelegate

- (UIAction*)textView:(UITextView*)textView
    primaryActionForTextItem:(UITextItem*)textItem
               defaultAction:(UIAction*)defaultAction {
  __weak __typeof__(self) weakSelf = self;
  return [UIAction actionWithHandler:^(UIAction* action) {
    [weakSelf.delegate walletReminderNoticeViewController:weakSelf
                                            didTapLinkURL:textItem.link];
  }];
}

@end
