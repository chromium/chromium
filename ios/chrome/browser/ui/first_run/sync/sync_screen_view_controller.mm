// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/sync/sync_screen_view_controller.h"

#import "ios/chrome/browser/ui/elements/activity_overlay_view.h"
#import "ios/chrome/browser/ui/first_run/first_run_constants.h"
#import "ios/chrome/browser/ui/settings/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/text_view_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/mac/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

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
      [self contentTextWithStringID:IDS_IOS_ACCOUNT_UNIFIED_CONSENT_TITLE];
  self.subtitleText =
      [self contentTextWithStringID:IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SYNC_TITLE];
  self.secondaryActionString =
      [self contentTextWithStringID:
                IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SECONDARY_ACTION];
  self.activateSyncButtonID = IDS_IOS_ACCOUNT_UNIFIED_CONSENT_OK_BUTTON;

  self.primaryActionString =
      [self contentTextWithStringID:self.activateSyncButtonID];

  self.bannerName = @"sync_screen_banner";
  self.isTallBanner = NO;
  self.scrollToEndMandatory = YES;
  self.readMoreString =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SCREEN_READ_MORE);

  if (self.syncTypesRestricted) {
    self.learnMoreTextView.delegate = self;
    [self.specificContentView addSubview:self.learnMoreTextView];

    [NSLayoutConstraint activateConstraints:@[
      [self.learnMoreTextView.topAnchor
          constraintGreaterThanOrEqualToAnchor:self.specificContentView
                                                   .topAnchor],
      [self.learnMoreTextView.bottomAnchor
          constraintEqualToAnchor:self.specificContentView.bottomAnchor],
      [self.learnMoreTextView.centerXAnchor
          constraintEqualToAnchor:self.specificContentView.centerXAnchor],
      [self.learnMoreTextView.widthAnchor
          constraintLessThanOrEqualToAnchor:self.specificContentView
                                                .widthAnchor],
    ]];
  }

  self.disclaimerText =
      [self contentTextWithStringID:
                IDS_IOS_FIRST_RUN_SYNC_SCREEN_CONTENT_WITH_LINK_TO_SETTINGS];
  self.disclaimerURLs = @[ net::NSURLWithGURL(GURL(kSettingsSyncURL)) ];

  [super viewDidLoad];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self.delegate logScrollButtonVisible:!self.didReachBottom];
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
    _learnMoreTextView = CreateUITextViewWithTextKit1();
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
          [UIFont preferredFontForTextStyle:UIFontTextStyleCaption2],
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

// Push the string id to `_contentStringIds` and returns NSString.
- (NSString*)contentTextWithStringID:(const int)stringID {
  [self.delegate addConsentStringID:stringID];
  return l10n_util::GetNSString(stringID);
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  if (textView != self.learnMoreTextView) {
    // The text view being tapped is not the learnMoreTextView. Defer to the
    // handler in the superclass.
    [super textView:textView
        shouldInteractWithURL:URL
                      inRange:characterRange
                  interaction:interaction];
    return NO;
  }

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

@end
