// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_sync/signin_sync_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/authentication/authentication_constants.h"
#import "ios/chrome/browser/ui/authentication/views/identity_button_control.h"
#import "ios/chrome/browser/ui/elements/activity_overlay_view.h"
#import "ios/chrome/browser/ui/settings/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/text_view_util.h"
#import "ios/chrome/grit/ios_google_chrome_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/mac/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Width of the identity control.
constexpr CGFloat kIdentityControlMarginDefault = 16;

// URL for the Settings link.
const char* const kSettingsSyncURL = "internal://settings-sync";

// URL for the learn more text.
// Need to set a value so the delegate gets called.
NSString* const kLearnMoreUrl = @"internal://learn-more";

NSString* const kLearnMoreTextViewAccessibilityIdentifier =
    @"kLearnMoreTextViewAccessibilityIdentifier";

}  // namespace

@interface SigninSyncViewController ()

// Button controlling the display of the selected identity.
@property(nonatomic, strong) IdentityButtonControl* identityControl;

// Layout guide determining the area for the identity control.
@property(nonatomic, strong) UILayoutGuide* identityControlArea;

// Scrim displayed above the view when the UI is disabled.
@property(nonatomic, strong) ActivityOverlayView* overlay;

// Text view that displays an attributed string with the "Learn More" link that
// opens a popover.
@property(nonatomic, strong) UITextView* learnMoreTextView;

// Popover shown when "Details" link is tapped.
@property(nonatomic, strong)
    EnterpriseInfoPopoverViewController* bubbleViewController;

// Bottom constraint of the identity control area.
@property(nonatomic, strong)
    NSLayoutConstraint* identityControlAreaBottomConstraint;

// YES when the sign-in or sign out action is done.
@property(nonatomic, assign) BOOL signinSignoutActionDone;

// YES when spinner overlay animation is done.
@property(nonatomic, assign) BOOL overlayAnimationDone;

@end

@implementation SigninSyncViewController
@dynamic delegate;

#pragma mark - Public

- (void)viewDidLoad {
  self.view.accessibilityIdentifier = kSigninSyncScreenAccessibilityIdentifier;
  self.isTallBanner = NO;
  self.scrollToEndMandatory = YES;
  self.readMoreString =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SCREEN_READ_MORE);

  [self.delegate signinSyncViewController:self
                       addConsentStringID:[self titleTextID]];
  self.titleText = l10n_util::GetNSString([self titleTextID]);

  [self.delegate signinSyncViewController:self
                       addConsentStringID:[self subtitleTextID]];
  self.subtitleText = l10n_util::GetNSString([self subtitleTextID]);

  if (!self.primaryActionString) {
    // `primaryActionString` could already be set using the consumer methods.
    self.primaryActionString =
        l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_SIGN_IN_ACTION);
  }
  // Set the consent ID associated with the primary action string to
  // `self.activateSyncButtonID` regardless of its current value because this
  // is the only string that will be used in the button when enabling sync.
  [self.delegate signinSyncViewController:self
                       addConsentStringID:self.activateSyncButtonID];

  [self.specificContentView addSubview:self.identityControl];
  [self.specificContentView addLayoutGuide:self.identityControlArea];

  // Add the Learn More text label if there are enterprise sign-in or sync
  // restrictions.
  if (self.enterpriseSignInRestrictions != kNoEnterpriseRestriction) {
    self.learnMoreTextView.delegate = self;
    [self.specificContentView addSubview:self.learnMoreTextView];

    [NSLayoutConstraint activateConstraints:@[
      [self.learnMoreTextView.bottomAnchor
          constraintEqualToAnchor:self.specificContentView.bottomAnchor],
      [self.learnMoreTextView.centerXAnchor
          constraintEqualToAnchor:self.specificContentView.centerXAnchor],
      [self.learnMoreTextView.widthAnchor
          constraintLessThanOrEqualToAnchor:self.specificContentView
                                                .widthAnchor],
    ]];
  }

  self.bannerName = @"sync_screen_banner";
  self.secondaryActionString =
      l10n_util::GetNSString([self secondaryActionStringID]);

  // Set constraints specific to the identity control button that don't change.
  NSLayoutConstraint* areaWidthConstraint = [self.identityControl.widthAnchor
      constraintEqualToAnchor:self.specificContentView.widthAnchor];
  areaWidthConstraint.priority = UILayoutPriorityDefaultHigh;
  [NSLayoutConstraint activateConstraints:@[
    [self.identityControlArea.centerXAnchor
        constraintEqualToAnchor:self.identityControlArea.owningView
                                    .centerXAnchor],
    [self.identityControlArea.widthAnchor
        constraintLessThanOrEqualToAnchor:self.identityControlArea.owningView
                                              .widthAnchor],
    [self.identityControlArea.topAnchor
        constraintEqualToAnchor:self.identityControl.topAnchor
                       constant:0],
    areaWidthConstraint,
    [self.identityControl.widthAnchor
        constraintEqualToAnchor:self.identityControlArea.widthAnchor],
    [self.identityControl.centerXAnchor
        constraintEqualToAnchor:self.identityControlArea.centerXAnchor],
    [self.identityControlArea.topAnchor
        constraintEqualToAnchor:self.specificContentView.topAnchor],
    [self.identityControlArea.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.specificContentView
                                              .bottomAnchor],
  ]];
  if (self.enterpriseSignInRestrictions != kNoEnterpriseRestriction) {
    [self.learnMoreTextView.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.identityControlArea
                                                 .bottomAnchor]
        .active = YES;
  }

  [self.delegate signinSyncViewController:self
                       addConsentStringID:[self disclaimerTextID]];
  if (self.identityControl.hidden) {
    // Since no one is logged in, the word "settings" should not be linkable;
    // retrieve raw text from the string with tags.
    self.disclaimerText =
        ParseStringWithLinks(l10n_util::GetNSString([self disclaimerTextID]))
            .string;
    self.disclaimerURLs = [NSArray array];
  } else {
    self.disclaimerText = l10n_util::GetNSString([self disclaimerTextID]);
    self.disclaimerURLs = @[ net::NSURLWithGURL(GURL(kSettingsSyncURL)) ];
  }

  [self updateIdentityControlButtonVerticalLayout];

  // Call super after setting up the strings and others, as required per super
  // class.
  [super viewDidLoad];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self.delegate signinSyncViewController:self
                   logScrollButtonVisible:!self.didReachBottom
                 withAccountPickerVisible:!self.identityControl.hidden];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];

  // Close popover when font size changed for accessibility because it does not
  // resize properly and the arrow is not aligned.
  if (self.bubbleViewController) {
    [self.bubbleViewController dismissViewControllerAnimated:YES
                                                  completion:nil];
  }
}

#pragma mark - Properties

- (void)setOverlayAnimationDone:(BOOL)overlayAnimationDone {
  _overlayAnimationDone = overlayAnimationDone;
  if (_overlayAnimationDone) {
    [self setUIEnabled:YES];
  }
}

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

- (UILayoutGuide*)identityControlArea {
  if (!_identityControlArea) {
    _identityControlArea = [[UILayoutGuide alloc] init];
  }
  return _identityControlArea;
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
    _learnMoreTextView = CreateUITextViewWithTextKit1();
    _learnMoreTextView.backgroundColor = UIColor.clearColor;
    _learnMoreTextView.scrollEnabled = NO;
    _learnMoreTextView.editable = NO;
    _learnMoreTextView.adjustsFontForContentSizeCategory = YES;
    _learnMoreTextView.textContainerInset = UIEdgeInsetsZero;
    _learnMoreTextView.textContainer.lineFragmentPadding = 0;
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

// Returns the ID of the string of the button that is used to activate sync.
- (int)activateSyncButtonID {
  return IDS_IOS_ACCOUNT_UNIFIED_CONSENT_OK_BUTTON;
}

#pragma mark - SignInSyncConsumer

- (void)setSelectedIdentityUserName:(NSString*)userName
                              email:(NSString*)email
                          givenName:(NSString*)givenName
                             avatar:(UIImage*)avatar {
  DCHECK(email);
  DCHECK(avatar);
  [self updateUIForIdentityAvailable:YES];
  [self.identityControl setIdentityName:userName email:email];
  [self.identityControl setIdentityAvatar:avatar];
}

- (void)noIdentityAvailable {
  [self updateUIForIdentityAvailable:NO];
}

- (void)setUIEnabled:(BOOL)UIEnabled {
  if (UIEnabled) {
    // Only remove the overlay when both the action and the animation are done.
    if (self.signinSignoutActionDone && self.overlayAnimationDone) {
      [self.overlay removeFromSuperview];
    }
  } else {
    // Handling the sign-in or sign out action and start the fade-in effect
    // along with the spinner animation.
    self.signinSignoutActionDone = NO;
    self.overlayAnimationDone = NO;

    self.overlay.indicator.alpha = 0.0;
    [self.view addSubview:self.overlay];
    AddSameConstraints(self.view, self.overlay);
    [self.overlay.indicator startAnimating];
    [UIView animateWithDuration:0.2
        animations:^{
          self.overlay.indicator.alpha = 1.0;
        }
        completion:^(BOOL finished) {
          self.overlayAnimationDone = YES;
        }];
  }
}

- (void)setActionToDone {
  self.signinSignoutActionDone = YES;
  [self setUIEnabled:YES];
}

#pragma mark - Private

// Callback for `identityControl`.
- (void)identityButtonControlTapped:(id)sender forEvent:(UIEvent*)event {
  UITouch* touch = event.allTouches.anyObject;
  [self.delegate signinSyncViewController:self
               showAccountPickerFromPoint:[touch locationInView:nil]];
}

// Updates the UI to adapt for `identityAvailable` or not.
- (void)updateUIForIdentityAvailable:(BOOL)identityAvailable {
  self.identityControl.hidden = !identityAvailable;
  [self updateIdentityControlButtonVerticalLayout];
  if (identityAvailable) {
    self.primaryActionString =
        l10n_util::GetNSString(self.activateSyncButtonID);
    self.disclaimerText = l10n_util::GetNSString([self disclaimerTextID]);
    self.disclaimerURLs = @[ net::NSURLWithGURL(GURL(kSettingsSyncURL)) ];
  } else {
    self.primaryActionString =
        l10n_util::GetNSString(IDS_IOS_ACCOUNT_UNIFIED_CONSENT_ADD_ACCOUNT);
    // Since no one is logged in, the word "settings" should not be linkable.
    self.disclaimerText =
        ParseStringWithLinks(l10n_util::GetNSString([self disclaimerTextID]))
            .string;
    self.disclaimerURLs = [NSArray array];
  }
}

// Appends `restrictionString` to `existingString`, adding padding if needed.
- (void)appendRestrictionString:(NSString*)restrictionString
                       toString:(NSMutableString*)existingString {
  NSString* padding = @"\n\n";
  if ([existingString length])
    [existingString appendString:padding];
  [existingString appendString:restrictionString];
}

// Returns the title string ID.
- (int)titleTextID {
  return IDS_IOS_ACCOUNT_UNIFIED_CONSENT_TITLE;
}

// Returns the subtitle string ID.
- (int)subtitleTextID {
  return IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SYNC_TITLE;
}

// Returns the secondary action string ID.
- (int)secondaryActionStringID {
  return IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SECONDARY_ACTION;
}

// Returns the disclaimer text string ID.
- (int)disclaimerTextID {
  return IDS_IOS_FIRST_RUN_SYNC_SCREEN_CONTENT_WITH_LINK_TO_SETTINGS;
}

// Updates the vertical layout of the identity control button according to its
// visibility.
- (void)updateIdentityControlButtonVerticalLayout {
  if (!self.viewLoaded) {
    // Don't update the constraints when the view isn't yet loaded because the
    // view tree as to be properly set before updating the constraints.
    return;
  }

  BOOL hidden = self.identityControl.hidden;

  // Clear constraint from the previous state.
  self.identityControlAreaBottomConstraint.active = NO;

  // Set the bottom margin between the area and identity control view to match
  // the state of the UI.
  int bottomMargin = kIdentityControlMarginDefault;
  if (!hidden) {
    if (self.enterpriseSignInRestrictions == kNoEnterpriseRestriction) {
      // Remove the bottom margin when the identity control is in bottom and
      // visible.
      bottomMargin = 0;
    }
  }

  // Limit the area to a specific height when the identity control is hidden.
  NSLayoutAnchor* bottomAnchor = hidden ? self.identityControlArea.topAnchor
                                        : self.identityControl.bottomAnchor;

  self.identityControlAreaBottomConstraint =
      [self.identityControlArea.bottomAnchor
          constraintEqualToAnchor:bottomAnchor
                         constant:bottomMargin];
  self.identityControlAreaBottomConstraint.active = YES;
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
  DCHECK(textView == self.learnMoreTextView);

  NSMutableString* detailsMessage = [[NSMutableString alloc] init];
  if (self.enterpriseSignInRestrictions & kEnterpriseForceSignIn) {
    [self appendRestrictionString:l10n_util::GetNSString(
                                      IDS_IOS_ENTERPRISE_FORCED_SIGNIN_MESSAGE)
                         toString:detailsMessage];
  }
  if (self.enterpriseSignInRestrictions & kEnterpriseRestrictAccounts) {
    [self appendRestrictionString:
              l10n_util::GetNSString(
                  IDS_IOS_ENTERPRISE_RESTRICTED_ACCOUNTS_TO_PATTERNS_MESSAGE)
                         toString:detailsMessage];
  }
  if (self.enterpriseSignInRestrictions & kEnterpriseSyncTypesListDisabled) {
    [self appendRestrictionString:l10n_util::GetNSString(
                                      IDS_IOS_ENTERPRISE_MANAGED_SYNC)
                         toString:detailsMessage];
  }

  // Open signin popover.
  self.bubbleViewController = [[EnterpriseInfoPopoverViewController alloc]
             initWithMessage:detailsMessage
              enterpriseName:nil  // TODO(crbug.com/1251986): Remove this
                                  // variable.
      isPresentingFromButton:NO
            addLearnMoreLink:NO];
  [self presentViewController:self.bubbleViewController
                     animated:YES
                   completion:nil];

  // Set the anchor and arrow direction of the bubble.
  self.bubbleViewController.popoverPresentationController.sourceView =
      self.learnMoreTextView;
  self.bubbleViewController.popoverPresentationController.sourceRect =
      TextViewLinkBound(textView, characterRange);
  self.bubbleViewController.popoverPresentationController
      .permittedArrowDirections =
      UIPopoverArrowDirectionUp | UIPopoverArrowDirectionDown;

  // The handler is already handling the tap.
  return NO;
}

@end
