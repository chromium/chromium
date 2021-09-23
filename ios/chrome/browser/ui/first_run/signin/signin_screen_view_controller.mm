// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/signin/signin_screen_view_controller.h"

#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/authentication/views/identity_button_control.h"
#import "ios/chrome/browser/ui/elements/activity_overlay_view.h"
#import "ios/chrome/browser/ui/first_run/first_run_constants.h"
#import "ios/chrome/browser/ui/settings/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_google_chrome_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Width of the identity control if nothing is contraining it.
const CGFloat kIdentityControlMaxWidth = 327;
const CGFloat kIdentityTopMargin = 16;

// URL for the learn more text.
// Need to set a value so the delegate gets called.
NSString* const kLearnMoreUrl = @"internal://learn-more";

NSString* const kLearnMoreTextViewAccessibilityIdentifier =
    @"kLearnMoreTextViewAccessibilityIdentifier";

}  // namespace

@interface SigninScreenViewController () <UITextViewDelegate>

// Button controlling the display of the selected identity.
@property(nonatomic, strong) IdentityButtonControl* identityControl;

// The string to be displayed in the "Cotinue" button to personalize it. Usually
// the given name, or the email address if no given name.
@property(nonatomic, copy) NSString* personalizedButtonPrompt;

// Scrim displayed above the view when the UI is disabled.
@property(nonatomic, strong) ActivityOverlayView* overlay;

// Text view that displays an attributed string with the "Learn More" link that
// opens a popover.
@property(nonatomic, strong) UITextView* learnMoreTextView;

@end

@implementation SigninScreenViewController
@dynamic delegate;

#pragma mark - Public

- (void)viewDidLoad {
  self.view.accessibilityIdentifier =
      first_run::kFirstRunSignInScreenAccessibilityIdentifier;
  self.bannerImage = [UIImage imageNamed:@"signin_screen_banner"];
  self.isTallBanner = NO;
  self.scrollToEndMandatory = YES;

  self.titleText = l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_TITLE);
  self.subtitleText = l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_SUBTITLE);
  if (!self.primaryActionString) {
    // |primaryActionString| could already be set using the consumer methods.
    self.primaryActionString =
        l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_SIGN_IN_ACTION);
  }

  [self.specificContentView addSubview:self.identityControl];

  // Add Learn More text label for the forced signin policy.
  if (self.forcedSignin) {
    self.learnMoreTextView.delegate = self;
    [self.specificContentView addSubview:self.learnMoreTextView];

    [NSLayoutConstraint activateConstraints:@[
      [self.learnMoreTextView.topAnchor
          constraintGreaterThanOrEqualToAnchor:self.identityControl
                                                   .bottomAnchor],
      [self.learnMoreTextView.bottomAnchor
          constraintEqualToAnchor:self.specificContentView.bottomAnchor],
      [self.learnMoreTextView.centerXAnchor
          constraintEqualToAnchor:self.specificContentView.centerXAnchor],
      [self.learnMoreTextView.widthAnchor
          constraintLessThanOrEqualToAnchor:self.specificContentView
                                                .widthAnchor],
    ]];
  } else {
    // Only add "Don't Sign In" button when signin is not required.
    self.secondaryActionString =
        l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_DONT_SIGN_IN);
  }

  NSLayoutConstraint* widthConstraint = [self.identityControl.widthAnchor
      constraintEqualToConstant:kIdentityControlMaxWidth];
  widthConstraint.priority = UILayoutPriorityDefaultHigh;

  [NSLayoutConstraint activateConstraints:@[
    [self.identityControl.topAnchor
        constraintEqualToAnchor:self.specificContentView.topAnchor
                       constant:kIdentityTopMargin],
    [self.identityControl.centerXAnchor
        constraintEqualToAnchor:self.specificContentView.centerXAnchor],
    [self.identityControl.widthAnchor
        constraintLessThanOrEqualToAnchor:self.specificContentView.widthAnchor],
    widthConstraint,
    [self.identityControl.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.specificContentView
                                              .bottomAnchor],
  ]];

  // Call super after setting up the strings and others, as required per super
  // class.
  [super viewDidLoad];
}

#pragma mark - Properties

- (IdentityButtonControl*)identityControl {
  if (!_identityControl) {
    _identityControl = [[IdentityButtonControl alloc] initWithFrame:CGRectZero];
    _identityControl.translatesAutoresizingMaskIntoConstraints = NO;
    [_identityControl addTarget:self
                         action:@selector(identityButtonControlTapped:forEvent:)
               forControlEvents:UIControlEventTouchUpInside];

    // Setting the content hugging priority isn't working, so creating a
    // low-priority constraint to make sure that the view is as small as
    // possible.
    NSLayoutConstraint* heightConstraint =
        [_identityControl.heightAnchor constraintEqualToConstant:0];
    heightConstraint.priority = UILayoutPriorityDefaultLow - 1;
    heightConstraint.active = YES;
  }
  return _identityControl;
}

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
            l10n_util::GetNSString(IDS_IOS_ENTERPRISE_MANAGED_REQUIRES_SIGNIN),
            textAttributes, linkAttributes);

    _learnMoreTextView.attributedText = learnMoreTextAttributedString;
  }
  return _learnMoreTextView;
}

#pragma mark - SignInScreenConsumer

- (void)setSelectedIdentityUserName:(NSString*)userName
                              email:(NSString*)email
                          givenName:(NSString*)givenName
                             avatar:(UIImage*)avatar {
  DCHECK(email);
  DCHECK(avatar);
  self.personalizedButtonPrompt = givenName ? givenName : email;
  [self updateUIForIdentityAvailable:YES];
  [self.identityControl setIdentityName:userName email:email];
  [self.identityControl setIdentityAvatar:avatar];
}

- (void)noIdentityAvailable {
  [self updateUIForIdentityAvailable:NO];
}

- (void)setUIEnabled:(BOOL)UIEnabled {
  if (UIEnabled) {
    [self.overlay removeFromSuperview];
  } else {
    [self.view addSubview:self.overlay];
    AddSameConstraints(self.view, self.overlay);
    [self.overlay.indicator startAnimating];
  }
}

#pragma mark - Private

// Callback for |identityControl|.
- (void)identityButtonControlTapped:(id)sender forEvent:(UIEvent*)event {
  UITouch* touch = event.allTouches.anyObject;
  [self.delegate showAccountPickerFromPoint:[touch locationInView:nil]];
}

// Updates the UI to adapt for |identityAvailable| or not.
- (void)updateUIForIdentityAvailable:(BOOL)identityAvailable {
  self.identityControl.hidden = !identityAvailable;
  if (identityAvailable) {
    self.primaryActionString = l10n_util::GetNSStringF(
        IDS_IOS_FIRST_RUN_SIGNIN_CONTINUE_AS,
        base::SysNSStringToUTF16(self.personalizedButtonPrompt));
    ;
  } else {
    self.primaryActionString =
        l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_SIGN_IN_ACTION);
  }
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  DCHECK(textView == self.learnMoreTextView);

  // Open signin popover.
  EnterpriseInfoPopoverViewController* bubbleViewController =
      [[EnterpriseInfoPopoverViewController alloc]
                 initWithMessage:l10n_util::GetNSString(
                                     IDS_IOS_ENTERPRISE_FORCED_SIGNIN_MESSAGE)
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
