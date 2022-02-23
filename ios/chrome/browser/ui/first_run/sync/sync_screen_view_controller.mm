// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/sync/sync_screen_view_controller.h"

#include "base/check.h"
#import "ios/chrome/browser/ui/elements/activity_overlay_view.h"
#import "ios/chrome/browser/ui/first_run/first_run_constants.h"
#import "ios/chrome/browser/ui/settings/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
constexpr CGFloat kMarginBetweenContents = 12;

// URL for the Settings link.
const char* const kSettingsSyncURL = "internal://settings-sync";

NSString* const kLearnMoreTextViewAccessibilityIdentifier =
    @"kLearnMoreTextViewAccessibilityIdentifier";

// URL for the learn more text.
// Need to set a value so the delegate gets called.
NSString* const kLearnMoreUrl = @"internal://learn-more";

}  // namespace

@interface SyncScreenViewController () <UITextViewDelegate>

// Scrim displayed above the view when the UI is disabled.
@property(nonatomic, strong) ActivityOverlayView* overlay;

// Text view that displays an attributed string with the "Learn More" link that
// opens a popover.
@property(nonatomic, strong) UITextView* learnMoreTextView;

@end

@implementation SyncScreenViewController

@dynamic delegate;

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.view.accessibilityIdentifier =
      first_run::kFirstRunSyncScreenAccessibilityIdentifier;
  self.titleText =
      [self contentTextWithStringID:IDS_IOS_FIRST_RUN_SYNC_SCREEN_TITLE];
  self.subtitleText =
      [self contentTextWithStringID:IDS_IOS_FIRST_RUN_SYNC_SCREEN_SUBTITLE];
  self.secondaryActionString = [self
      contentTextWithStringID:IDS_IOS_FIRST_RUN_SYNC_SCREEN_SECONDARY_ACTION];
  self.activateSyncButtonID = IDS_IOS_FIRST_RUN_SYNC_SCREEN_PRIMARY_ACTION;

  self.primaryActionString =
      [self contentTextWithStringID:self.activateSyncButtonID];

  self.bannerImage = [UIImage imageNamed:@"sync_screen_banner"];
  self.isTallBanner = NO;
  self.scrollToEndMandatory = YES;
  self.readMoreString =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SCREEN_READ_MORE);

  // Add sync screen-specific content
  UILabel* contentText = [self createContentText];
  [self.specificContentView addSubview:contentText];

  UIView* advanceSyncSettingsButton = [self createAdvanceSyncSettingsButton];

  [self.specificContentView addSubview:advanceSyncSettingsButton];


  if (self.syncTypesRestricted) {
    self.learnMoreTextView.delegate = self;
    [self.specificContentView addSubview:self.learnMoreTextView];

    [NSLayoutConstraint activateConstraints:@[
      [self.learnMoreTextView.topAnchor
          constraintGreaterThanOrEqualToAnchor:advanceSyncSettingsButton
                                                   .bottomAnchor],
      [self.learnMoreTextView.bottomAnchor
          constraintEqualToAnchor:self.specificContentView.bottomAnchor],
      [self.learnMoreTextView.centerXAnchor
          constraintEqualToAnchor:self.specificContentView.centerXAnchor],
      [self.learnMoreTextView.widthAnchor
          constraintLessThanOrEqualToAnchor:self.specificContentView
                                                .widthAnchor],
    ]];
  }

  // Sync screen-specific constraints.
  [NSLayoutConstraint activateConstraints:@[
    [contentText.topAnchor
        constraintEqualToAnchor:self.specificContentView.topAnchor],
    [contentText.centerXAnchor
        constraintEqualToAnchor:self.specificContentView.centerXAnchor],
    [contentText.widthAnchor
        constraintLessThanOrEqualToAnchor:self.specificContentView.widthAnchor],
    [advanceSyncSettingsButton.topAnchor
        constraintEqualToAnchor:contentText.bottomAnchor
                       constant:kMarginBetweenContents],
    [advanceSyncSettingsButton.centerXAnchor
        constraintEqualToAnchor:self.specificContentView.centerXAnchor],
    [advanceSyncSettingsButton.widthAnchor
        constraintLessThanOrEqualToAnchor:self.specificContentView.widthAnchor],
    [advanceSyncSettingsButton.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.specificContentView
                                              .bottomAnchor],
  ]];

  [super viewDidLoad];
}

#pragma mark - Properties

- (ActivityOverlayView*)overlay {
  if (!_overlay) {
    _overlay = [[ActivityOverlayView alloc] init];
    _overlay.translatesAutoresizingMaskIntoConstraints = NO;
  }
  return _overlay;
}

- (UITextView*)learnMoreTextView {
  if (!_learnMoreTextView) {
    _learnMoreTextView = [[UITextView alloc] init];
    _learnMoreTextView.scrollEnabled = NO;
    _learnMoreTextView.editable = NO;
    _learnMoreTextView.adjustsFontForContentSizeCategory = YES;
    _learnMoreTextView.accessibilityIdentifier =
        kLearnMoreTextViewAccessibilityIdentifier;
    _learnMoreTextView.backgroundColor = UIColor.clearColor;

    _learnMoreTextView.linkTextAttributes =
        @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]};
    _learnMoreTextView.translatesAutoresizingMaskIntoConstraints = NO;

    NSMutableParagraphStyle* paragraphStyle =
        [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
    paragraphStyle.alignment = NSTextAlignmentCenter;

    NSDictionary* textAttributes = @{
      NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
      NSFontAttributeName :
          [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote],
      NSParagraphStyleAttributeName : paragraphStyle
    };

    NSDictionary* linkAttributes =
        @{NSLinkAttributeName : [NSURL URLWithString:kLearnMoreUrl]};

    NSAttributedString* learnMoreTextAttributedString =
        AttributedStringFromStringWithLink(
            l10n_util::GetNSString(IDS_IOS_ENTERPRISE_MANAGED_SIGNIN_DETAILS),
            textAttributes, linkAttributes);

    _learnMoreTextView.attributedText = learnMoreTextAttributedString;
  }
  return _learnMoreTextView;
}

#pragma mark - SyncInScreenConsumer

- (void)setUIEnabled:(BOOL)UIEnabled {
  if (UIEnabled) {
    [self.overlay removeFromSuperview];
  } else {
    [self.view addSubview:self.overlay];
    AddSameConstraints(self.view, self.overlay);
    [self.overlay.indicator startAnimating];
  }
}

#pragma mark - AuthenticationFlowDelegate

- (void)didPresentDialog {
  [self.overlay.indicator stopAnimating];
}

- (void)didDismissDialog {
  [self.overlay.indicator startAnimating];
}

#pragma mark - Private

// Creates and configures the text of sync screen
- (UILabel*)createContentText {
  UILabel* label = [[UILabel alloc] init];
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  label.numberOfLines = 0;
  label.textAlignment = NSTextAlignmentCenter;
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.adjustsFontForContentSizeCategory = YES;
  label.text =
      [self contentTextWithStringID:IDS_IOS_FIRST_RUN_SYNC_SCREEN_CONTENT];
  label.textColor = [UIColor colorNamed:kGrey600Color];
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  return label;
}

// Creates and configures the sync settings button.
- (UIButton*)createAdvanceSyncSettingsButton {
  UIButton* button = [[UIButton alloc] init];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.titleLabel.numberOfLines = 0;
  button.titleLabel.adjustsFontForContentSizeCategory = YES;
  [button.titleLabel
      setFont:[UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline]];
  self.openSettingsStringID = IDS_IOS_FIRST_RUN_SYNC_SCREEN_ADVANCE_SETTINGS;
  [button setTitle:[self contentTextWithStringID:self.openSettingsStringID]
          forState:UIControlStateNormal];
  [button setTitleColor:[UIColor colorNamed:kBlueColor]
               forState:UIControlStateNormal];

  [button addTarget:self
                action:@selector(showAdvanceSyncSettings)
      forControlEvents:UIControlEventTouchUpInside];
  return button;
}

// Push the string id to |_contentStringIds| and returns NSString.
- (NSString*)contentTextWithStringID:(const int)stringID {
  [self.delegate addConsentStringID:stringID];
  return l10n_util::GetNSString(stringID);
}

// Called when the sync settings button is tapped
- (void)showAdvanceSyncSettings {
  [self.delegate showSyncSettings];
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  if ([URL isEqual:net::NSURLWithGURL(GURL(kSettingsSyncURL))]) {
    [self showAdvanceSyncSettings];
    // The handler is already handling the tap.
    return NO;
  }
  DCHECK(textView == self.learnMoreTextView);

  // Open signin popover.
  EnterpriseInfoPopoverViewController* bubbleViewController =
      [[EnterpriseInfoPopoverViewController alloc]
                 initWithMessage:l10n_util::GetNSString(
                                     IDS_IOS_ENTERPRISE_MANAGED_SYNC)
                  enterpriseName:nil  // TODO(crbug.com/1251986): Remove this
                                      // variable.
          isPresentingFromButton:NO
                addLearnMoreLink:NO];
  [self presentViewController:bubbleViewController animated:YES completion:nil];

  // Set the anchor and arrow direction of the bubble.
  bubbleViewController.popoverPresentationController.sourceView =
      self.learnMoreTextView;
  bubbleViewController.popoverPresentationController.sourceRect =
      TextViewLinkBound(textView, characterRange);
  bubbleViewController.popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionUp | UIPopoverArrowDirectionDown;

  // The handler is already handling the tap.
  return NO;
}

- (void)textViewDidChangeSelection:(UITextView*)textView {
  // Always force the |selectedTextRange| to |nil| to prevent users from
  // selecting text. Setting the |selectable| property to |NO| doesn't help
  // since it makes links inside the text view untappable.
  textView.selectedTextRange = nil;
}

@end
