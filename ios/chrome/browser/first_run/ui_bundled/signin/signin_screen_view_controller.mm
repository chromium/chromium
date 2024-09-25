// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/signin/signin_screen_view_controller.h"

#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/public/commands/tos_commands.h"
#import "ios/chrome/browser/shared/ui/elements/activity_overlay_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/authentication/views/identity_button_control.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_constants.h"
#import "ios/chrome/browser/ui/settings/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Top margin for the managed icon in the enteprised image view
constexpr CGFloat kTopMarginForManagedIcon = 16.;

// Enterprise icon in the bottom view.
NSString* const kEnterpriseIconName = @"enterprise_icon";

}  // namespace

@interface SigninScreenViewController ()

// Button controlling the display of the selected identity.
@property(nonatomic, strong) IdentityButtonControl* identityControl;
// The string to be displayed in the "Continue" button to personalize it.
// Usually the given name, or the email address if no given name.
@property(nonatomic, copy) NSString* personalizedButtonPrompt;
// Scrim displayed above the view when the UI is disabled.
@property(nonatomic, strong) ActivityOverlayView* overlay;

@end

@implementation SigninScreenViewController

@dynamic delegate;
@synthesize hasPlatformPolicies = _hasPlatformPolicies;
@synthesize screenIntent = _screenIntent;
@synthesize signinStatus = _signinStatus;
@synthesize syncEnabled = _syncEnabled;

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.view.accessibilityIdentifier =
      first_run::kFirstRunSignInScreenAccessibilityIdentifier;
  self.bannerSize = BannerImageSizeType::kStandard;
  self.scrollToEndMandatory = YES;
  self.readMoreString =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SCREEN_READ_MORE);

  // Set banner.
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  self.bannerName = kChromeSigninBannerImage;
#else
  self.bannerName = kChromiumSigninBannerImage;
#endif

  // Set `self.titleText` and `self.subtitleText`.
  switch (self.signinStatus) {
    case SigninScreenConsumerSigninStatusAvailable: {
      switch (self.screenIntent) {
        case SigninScreenConsumerScreenIntentSigninOnly:
          // Use in the context of the upgrade promo dialog.
          self.titleText =
              l10n_util::GetNSString(IDS_IOS_UNO_UPGRADE_PROMO_SIGNIN_TITLE);
          self.subtitleText =
              self.syncEnabled
                  ? l10n_util::GetNSString(
                        IDS_IOS_UNO_UPGRADE_PROMO_SIGNIN_SUBTITLE)
                  : l10n_util::GetNSString(
                        IDS_IOS_UNO_UPGRADE_PROMO_SIGNIN_SUBTITLE_SYNC_DISABLED);

          break;
        case SigninScreenConsumerScreenIntentWelcomeAndSignin:
        case SigninScreenConsumerScreenIntentWelcomeWithoutUMAAndSignin:
          // Use in the context of the FRE dialog.
          self.titleText =
              l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_TITLE);
          self.subtitleText =
              self.syncEnabled
                  ? l10n_util::GetNSString(
                        IDS_IOS_FIRST_RUN_SIGNIN_BENEFITS_SUBTITLE_SHORT)
                  : l10n_util::GetNSString(
                        IDS_IOS_FIRST_RUN_SIGNIN_SUBTITLE_SHORT);
          break;
      }
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
  [self generateDisclaimer];

  // Add `self.identityControl` if needed.
  if (self.signinStatus != SigninScreenConsumerSigninStatusDisabled) {
    [self.specificContentView addSubview:self.identityControl];

    [NSLayoutConstraint activateConstraints:@[
      [self.identityControl.topAnchor
          constraintEqualToAnchor:self.specificContentView.topAnchor],
      [self.identityControl.centerXAnchor
          constraintEqualToAnchor:self.specificContentView.centerXAnchor],
      [self.identityControl.widthAnchor
          constraintEqualToAnchor:self.specificContentView.widthAnchor],
      [self.identityControl.bottomAnchor
          constraintLessThanOrEqualToAnchor:self.specificContentView
                                                .bottomAnchor],
    ]];
  }

  // Add enterprise image view.
  if (self.hasPlatformPolicies) {
    NSLayoutYAxisAnchor* topAnchorForEnterpriseIcon =
        self.signinStatus == SigninScreenConsumerSigninStatusDisabled
            ? self.specificContentView.topAnchor
            : self.identityControl.bottomAnchor;
    UIImage* image = [UIImage imageNamed:kEnterpriseIconName];
    UIImageView* enterpriseImageView =
        [[UIImageView alloc] initWithImage:image];
    enterpriseImageView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.specificContentView addSubview:enterpriseImageView];
    [NSLayoutConstraint activateConstraints:@[
      [enterpriseImageView.topAnchor
          constraintGreaterThanOrEqualToAnchor:topAnchorForEnterpriseIcon
                                      constant:kTopMarginForManagedIcon],
      [enterpriseImageView.bottomAnchor
          constraintEqualToAnchor:self.specificContentView.bottomAnchor],
      [enterpriseImageView.centerXAnchor
          constraintEqualToAnchor:self.specificContentView.centerXAnchor],
      [enterpriseImageView.widthAnchor
          constraintLessThanOrEqualToAnchor:self.specificContentView
                                                .widthAnchor],
    ]];
  }

  // Set primary button if sign-in is disabled. For other cases, the primary
  // button is set with `setSelectedIdentityUserName:email:givenName:avatar:`
  // or `noIdentityAvailable`.
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

#pragma mark - Private

// Generates the footer string.
- (void)generateDisclaimer {
  NSMutableArray<NSString*>* array = [NSMutableArray array];
  NSMutableArray<NSURL*>* urls = [NSMutableArray array];
  if (self.hasPlatformPolicies) {
    [array addObject:l10n_util::GetNSString(
                         IDS_IOS_FIRST_RUN_WELCOME_SCREEN_BROWSER_MANAGED)];
  }
  switch (self.screenIntent) {
    case SigninScreenConsumerScreenIntentSigninOnly: {
      break;
    }
    case SigninScreenConsumerScreenIntentWelcomeAndSignin: {
      [array addObject:l10n_util::GetNSString(
                           IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TERMS_OF_SERVICE)];
      [urls addObject:[NSURL URLWithString:first_run::kTermsOfServiceURL]];
      [array addObject:l10n_util::GetNSString(
                           IDS_IOS_FIRST_RUN_WELCOME_SCREEN_METRIC_REPORTING)];
      [urls addObject:[NSURL URLWithString:first_run::kMetricReportingURL]];
      break;
    }
    case SigninScreenConsumerScreenIntentWelcomeWithoutUMAAndSignin: {
      [array addObject:l10n_util::GetNSString(
                           IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TERMS_OF_SERVICE)];
      [urls addObject:[NSURL URLWithString:first_run::kTermsOfServiceURL]];
      break;
    }
  }
  self.disclaimerText = [array componentsJoinedByString:@" "];
  self.disclaimerURLs = urls;
}

// Callback for `identityControl`.
- (void)identityButtonControlTapped:(id)sender forEvent:(UIEvent*)event {
  UITouch* touch = event.allTouches.anyObject;
  [self.delegate showAccountPickerFromPoint:[touch locationInView:nil]];
}

// Updates the UI to adapt for `identityAvailable` or not.
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
  // For the disabled UI, show a spinner in the primary button.
  self.primaryButtonSpinnerEnabled = !UIEnabled;
  self.view.userInteractionEnabled = UIEnabled;
}

@end
