// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/signin/signin_screen_view_controller.h"

#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/authentication/views/identity_button_control.h"
#import "ios/chrome/browser/ui/commands/tos_commands.h"
#import "ios/chrome/browser/ui/elements/activity_overlay_view.h"
#import "ios/chrome/browser/ui/first_run/first_run_constants.h"
#import "ios/chrome/browser/ui/settings/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_google_chrome_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Width of the identity control if nothing is contraining it.
constexpr CGFloat kIdentityControlMaxWidth = 327.;
// Margin above the identity button.
constexpr CGFloat kIdentityTopMargin = 0.;
// Margin between elements in the bottom view.
constexpr CGFloat kBottomViewInnerVerticalMargin = 8.;

// Banner at the top of the view.
NSString* const kSigninBannerName = @"signin_banner";
// Enterprise icon in the bottom view.
NSString* const kEnterpriseIconName = @"enterprise_icon";

// URL for the terms of service text.
NSString* const kTermsOfServiceURL = @"internal://terms-of-service";
// URL for the metric reporting text.
NSString* const kMetricReportingURL = @"internal://metric-reporting";

// Returns the attribute for the footer UITextView.
NSDictionary* FooterTextAttributes() {
  NSMutableParagraphStyle* paragraph_style =
      [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
  paragraph_style.alignment = NSTextAlignmentCenter;

  return @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote],
    NSParagraphStyleAttributeName : paragraph_style
  };
}

// Adds |attributedString| as a new inline in |footer_attributed_string|.
void AddNewLineToFooterString(
    NSAttributedString* attributed_string,
    NSMutableAttributedString* footer_attributed_string) {
  DCHECK(footer_attributed_string);
  if (footer_attributed_string.length > 0) {
    NSDictionary* attributes = FooterTextAttributes();
    NSAttributedString* end_of_line =
        [[NSAttributedString alloc] initWithString:@" " attributes:attributes];
    [footer_attributed_string appendAttributedString:end_of_line];
  }
  [footer_attributed_string appendAttributedString:attributed_string];
}

// Creates an attributed string with the footer style based on |message_id|.
NSAttributedString* FooterAttributedStringWithMessageID(int message_id) {
  NSString* string = l10n_util::GetNSString(message_id);
  NSDictionary* textAttributes = FooterTextAttributes();
  return [[NSAttributedString alloc] initWithString:string
                                         attributes:textAttributes];
}

// Creates an attributed string with the footer style based on |message_id|
// (with an URL in it).
NSAttributedString* FooterAttributedStringWithMessageIDAndURL(
    int message_id,
    NSString* url_string) {
  NSString* string = l10n_util::GetNSString(message_id);
  NSDictionary* textAttributes = FooterTextAttributes();
  NSDictionary* linkAttributes =
      @{NSLinkAttributeName : [NSURL URLWithString:url_string]};
  return AttributedStringFromStringWithLink(string, textAttributes,
                                            linkAttributes);
}

}  // namespace

@interface SigninScreenViewController () <UITextViewDelegate>

// Button controlling the display of the selected identity.
@property(nonatomic, strong) IdentityButtonControl* identityControl;
// The string to be displayed in the "Cotinue" button to personalize it. Usually
// the given name, or the email address if no given name.
@property(nonatomic, copy) NSString* personalizedButtonPrompt;
// Scrim displayed above the view when the UI is disabled.
@property(nonatomic, strong) ActivityOverlayView* overlay;
// View with all the bottom details (image and text).
@property(nonatomic, strong) UIStackView* bottomView;

@end

@implementation SigninScreenViewController

@dynamic delegate;
@synthesize managedEnabled = _managedEnabled;
@synthesize screenIntent = _screenIntent;
@synthesize signinStatus = _signinStatus;

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.view.accessibilityIdentifier =
      first_run::kFirstRunSignInScreenAccessibilityIdentifier;
  self.isTallBanner = NO;
  self.scrollToEndMandatory = YES;
  self.readMoreString =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SCREEN_READ_MORE);

  // Set banner.
  self.bannerImage = [UIImage imageNamed:kSigninBannerName];

  // Set |self.titleText| and |self.subtitleText|.
  switch (self.signinStatus) {
    case SigninScreenConsumerSigninStatusAvailable: {
      self.titleText = l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_TITLE);
      self.subtitleText =
          l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_SUBTITLE);
      break;
    }
    case SigninScreenConsumerSigninStatusForced: {
      self.titleText =
          l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_TITLE_SIGNIN_FORCED);
      self.subtitleText = l10n_util::GetNSString(
          IDS_IOS_FIRST_RUN_SIGNIN_SUBTITLE_SIGNIN_FORCED);
      break;
    }
    case SigninScreenConsumerSigninStatusDisabled: {
      UIUserInterfaceIdiom idiom =
          [[UIDevice currentDevice] userInterfaceIdiom];
      if (idiom == UIUserInterfaceIdiomPad) {
        self.titleText =
            l10n_util::GetNSString(IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TITLE_IPAD);
      } else {
        self.titleText = l10n_util::GetNSString(
            IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TITLE_IPHONE);
      }
      self.subtitleText =
          l10n_util::GetNSString(IDS_IOS_FIRST_RUN_WELCOME_SCREEN_SUBTITLE);
      break;
    }
  }

  NSLayoutYAxisAnchor* topAnchorForBottomView =
      self.specificContentView.topAnchor;
  // Add |self.identityControl| if needed.
  if (self.signinStatus != SigninScreenConsumerSigninStatusDisabled) {
    [self.specificContentView addSubview:self.identityControl];

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
          constraintLessThanOrEqualToAnchor:self.specificContentView
                                                .widthAnchor],
      widthConstraint,
      [self.identityControl.bottomAnchor
          constraintLessThanOrEqualToAnchor:self.specificContentView
                                                .bottomAnchor],
    ]];

    topAnchorForBottomView = self.identityControl.bottomAnchor;
  }

  // Add bottom view.
  [self.specificContentView addSubview:self.bottomView];
  [NSLayoutConstraint activateConstraints:@[
    [self.bottomView.topAnchor
        constraintGreaterThanOrEqualToAnchor:topAnchorForBottomView
                                    constant:kIdentityTopMargin],
    [self.bottomView.bottomAnchor
        constraintEqualToAnchor:self.specificContentView.bottomAnchor],
    [self.bottomView.centerXAnchor
        constraintEqualToAnchor:self.specificContentView.centerXAnchor],
    [self.bottomView.widthAnchor
        constraintLessThanOrEqualToAnchor:self.specificContentView.widthAnchor],
  ]];

  // Set primary button if sign-in is disabled. For other cases, the primary
  // button is set with |setSelectedIdentityUserName:email:givenName:avatar:|
  // or |noIdentityAvailable|.
  DCHECK(self.primaryActionString ||
         self.signinStatus == SigninScreenConsumerSigninStatusDisabled);
  if (self.signinStatus == SigninScreenConsumerSigninStatusDisabled) {
    self.primaryActionString =
        l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_CONTINUE);
  }
  // Set secondary button.
  if (self.signinStatus == SigninScreenConsumerSigninStatusAvailable) {
    self.secondaryActionString =
        l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_DONT_SIGN_IN);
  }

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

- (UIStackView*)bottomView {
  if (!_bottomView) {
    _bottomView = [[UIStackView alloc] init];
    _bottomView.translatesAutoresizingMaskIntoConstraints = NO;
    _bottomView.axis = UILayoutConstraintAxisVertical;
    _bottomView.alignment = UIStackViewAlignmentCenter;
    _bottomView.distribution = UIStackViewDistributionEqualSpacing;
    _bottomView.spacing = kBottomViewInnerVerticalMargin;
    // Add the enterprise icon if needed.
    if (self.managedEnabled) {
      UIImage* image = [UIImage imageNamed:kEnterpriseIconName];
      UIImageView* enterpriseImageView =
          [[UIImageView alloc] initWithImage:image];
      [_bottomView addArrangedSubview:enterpriseImageView];
    }
    // Add the footer string if needed.
    NSAttributedString* footerAttributedString =
        [self generateFooterAttributedString];
    if (footerAttributedString.length > 0) {
      UITextView* footerTextView = [[UITextView alloc] init];
      footerTextView.textContainerInset = UIEdgeInsetsMake(0, 0, 0, 0);
      footerTextView.scrollEnabled = NO;
      footerTextView.editable = NO;
      footerTextView.adjustsFontForContentSizeCategory = YES;
      footerTextView.delegate = self;
      footerTextView.backgroundColor = UIColor.clearColor;
      footerTextView.linkTextAttributes =
          @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]};
      footerTextView.translatesAutoresizingMaskIntoConstraints = NO;
      footerTextView.attributedText = footerAttributedString;
      [_bottomView addArrangedSubview:footerTextView];
    }
  }
  return _bottomView;
}

#pragma mark - Private

// Generates the footer string.
- (NSAttributedString*)generateFooterAttributedString {
  NSMutableAttributedString* footerAttributedString =
      [[NSMutableAttributedString alloc] init];
  if (self.managedEnabled) {
    NSAttributedString* footerLine = FooterAttributedStringWithMessageID(
        IDS_IOS_FIRST_RUN_WELCOME_SCREEN_BROWSER_MANAGED);
    AddNewLineToFooterString(footerLine, footerAttributedString);
  }
  switch (self.screenIntent) {
    case SigninScreenConsumerScreenIntentSigninOnly: {
      break;
    }
    case SigninScreenConsumerScreenIntentWelcomeAndSignin: {
      NSAttributedString* footerLine =
          FooterAttributedStringWithMessageIDAndURL(
              IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TERMS_OF_SERVICE,
              kTermsOfServiceURL);
      AddNewLineToFooterString(footerLine, footerAttributedString);
      footerLine = FooterAttributedStringWithMessageIDAndURL(
          IDS_IOS_FIRST_RUN_WELCOME_SCREEN_METRIC_REPORTING,
          kMetricReportingURL);
      AddNewLineToFooterString(footerLine, footerAttributedString);
      break;
    }
    case SigninScreenConsumerScreenIntentWelcomeWithoutUMAAndSignin: {
      NSAttributedString* footerLine =
          FooterAttributedStringWithMessageIDAndURL(
              IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TERMS_OF_SERVICE,
              kTermsOfServiceURL);
      AddNewLineToFooterString(footerLine, footerAttributedString);
      break;
    }
  }
  return footerAttributedString;
}

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
  } else {
    self.primaryActionString =
        l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_SIGN_IN_ACTION);
  }
}

#pragma mark - SigninScreenConsumer

- (void)setSelectedIdentityUserName:(NSString*)userName
                              email:(NSString*)email
                          givenName:(NSString*)givenName
                             avatar:(UIImage*)avatar {
  DCHECK_NE(self.signinStatus, SigninScreenConsumerSigninStatusDisabled);
  DCHECK(email);
  DCHECK(avatar);
  self.personalizedButtonPrompt = givenName ? givenName : email;
  [self updateUIForIdentityAvailable:YES];
  [self.identityControl setIdentityName:userName email:email];
  [self.identityControl setIdentityAvatar:avatar];
}

- (void)noIdentityAvailable {
  DCHECK_NE(self.signinStatus, SigninScreenConsumerSigninStatusDisabled);
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

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  if ([URL.absoluteString isEqualToString:kTermsOfServiceURL]) {
    [self.TOSHandler showTOSPage];
  } else if ([URL.absoluteString isEqualToString:kMetricReportingURL]) {
    [self.delegate showUMADialog];
  } else {
    NOTREACHED() << std::string("Unknown URL ")
                 << base::SysNSStringToUTF8(URL.absoluteString);
  }
  return NO;
}

- (void)textViewDidChangeSelection:(UITextView*)textView {
  // Always force the |selectedTextRange| to |nil| to prevent users from
  // selecting text. Setting the |selectable| property to |NO| doesn't help
  // since it makes links inside the text view untappable. Another solution is
  // to subclass |UITextView| and override |canBecomeFirstResponder| to return
  // NO, but that workaround only works on iOS 13.5+. This is the simplest
  // approach that works well on iOS 12, 13 & 14.
  textView.selectedTextRange = nil;
}

@end
