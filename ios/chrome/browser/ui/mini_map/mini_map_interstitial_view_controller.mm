// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/mini_map/mini_map_interstitial_view_controller.h"

#import "base/values.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/mac/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The extra spacing between element.
constexpr CGFloat kSpacing = 0;

// The spacing at the top of the view.
constexpr CGFloat kTopSpacing = 10;

// The spacing between image and text.
constexpr CGFloat kSpacingAfterImage = 10;

// Corner radius of the "alert" part.
constexpr CGFloat kCornerRadius = 20;

// The distance from the top of the screen in compact mode
constexpr CGFloat kCompactTopOffset = 50;

// The width ratio in compact mode
constexpr CGFloat kCompactWidthRatio = 0.8;

// Internal URL to catch "open Content Settings" tap.
const char* const kSettingsContentsURL = "internal://settings-contents";

// The size of the Map logo.
constexpr CGFloat kSymbolImagePointSize = 50;

// Returns the branded version of the Google maps symbol.
UIImage* GetBrandedGoogleMapsSymbol() {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  return MakeSymbolMulticolor(
      CustomSymbolWithPointSize(kGoogleMapsSymbol, kSymbolImagePointSize));
#else
  return DefaultSymbolWithPointSize(kMapSymbol, kSymbolImagePointSize);
#endif
}

}  // namespace

@interface MiniMapInterstitialViewController () <ConfirmationAlertActionHandler,
                                                 UITextViewDelegate>

// Child view controller used to display the alert screen for the bottom half
// of the view.
@property(nonatomic, strong) ConfirmationAlertViewController* alertScreen;

// The subview for the upper half of the view
@property(nonatomic, strong) UIImageView* imageView;

// Constraints to hide the upper view in compact vertical mode.
@property(nonatomic, strong) NSLayoutConstraint* topAlertConstraint;
@property(nonatomic, strong) NSLayoutConstraint* widthAlertConstraint;
@property(nonatomic, strong) NSLayoutConstraint* bottomImageConstraint;

@end

@implementation MiniMapInterstitialViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  [self createConfirmationAlertScreen];

  [self configureHeaderView];
  [self layoutAlertScreen];
  [self updateLimit];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self updateLimit];
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  [self.actionHandler dismissed];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self.actionHandler userPressedConsent];
}

- (void)confirmationAlertSecondaryAction {
  [self.actionHandler userPressedNoThanks];
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  [self.actionHandler userPressedContentSettings];
  // The handler is already handling the tap.
  return NO;
}

- (void)textViewDidChangeSelection:(UITextView*)textView {
  textView.selectedTextRange = nil;
}

#pragma mark - Private

// Creates a confirmation alert view controller and sets the strings.
- (void)createConfirmationAlertScreen {
  DCHECK(!self.alertScreen);
  ConfirmationAlertViewController* alertScreen =
      [[ConfirmationAlertViewController alloc] init];
  alertScreen.customSpacingBeforeImageIfNoNavigationBar = kTopSpacing;
  alertScreen.customSpacing = kSpacing;
  alertScreen.customSpacingAfterImage = kSpacingAfterImage;
  alertScreen.scrollEnabled = NO;
  alertScreen.topAlignedLayout = NO;
  alertScreen.titleString =
      l10n_util::GetNSString(IDS_IOS_MINI_MAP_CONSENT_TITLE);
  alertScreen.subtitleString =
      l10n_util::GetNSString(IDS_IOS_MINI_MAP_CONSENT_SUBTITLE);
  alertScreen.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_MINI_MAP_CONSENT_ACCEPT);
  alertScreen.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_MINI_MAP_CONSENT_NO_THANKS);
  alertScreen.image = GetBrandedGoogleMapsSymbol();
  alertScreen.imageEnclosedWithShadowWithoutBadge = YES;
  alertScreen.imageHasFixedSize = YES;
  alertScreen.customFaviconSideLength = kSymbolImagePointSize;

  // Create a footer view with a link to content settings.
  NSString* footerString =
      l10n_util::GetNSString(IDS_IOS_MINI_MAP_CONSENT_FOOTER);
  NSDictionary* textAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote]
  };
  NSDictionary* linkAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote],
    NSLinkAttributeName : net::NSURLWithGURL(GURL(kSettingsContentsURL)),
  };
  NSAttributedString* footerAttributedString =
      AttributedStringFromStringWithLink(footerString, textAttributes,
                                         linkAttributes);

  UITextView* footerView = [[UITextView alloc] init];
  footerView.translatesAutoresizingMaskIntoConstraints = NO;
  footerView.adjustsFontForContentSizeCategory = YES;
  footerView.editable = NO;
  footerView.scrollEnabled = NO;
  footerView.delegate = self;

  footerView.attributedText = footerAttributedString;
  footerView.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  footerView.textAlignment = NSTextAlignmentCenter;
  alertScreen.underTitleView = footerView;

  alertScreen.actionHandler = self;
  self.alertScreen = alertScreen;

  alertScreen.showDismissBarButton = NO;
  alertScreen.titleTextStyle = UIFontTextStyleTitle2;

  // Add a corner radius between the alert and the image
  alertScreen.view.layer.cornerRadius = kCornerRadius;
  alertScreen.view.layer.maskedCorners =
      kCALayerMaxXMinYCorner | kCALayerMinXMinYCorner;
  alertScreen.view.layer.masksToBounds = true;
  [self addChildViewController:alertScreen];
  [self.view addSubview:alertScreen.view];
  [alertScreen didMoveToParentViewController:self];
}

// Sets the layout of the alertScreen view.
- (void)layoutAlertScreen {
  self.alertScreen.view.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [self.alertScreen.view.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [self.alertScreen.view.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
  ]];
}

// Configures the image view and its constraints.
- (void)configureHeaderView {
  self.imageView =
      [[UIImageView alloc] initWithImage:[UIImage imageNamed:@"map_blur"]];
  [self.view insertSubview:self.imageView belowSubview:self.alertScreen.view];

  self.imageView.translatesAutoresizingMaskIntoConstraints = NO;

  [NSLayoutConstraint activateConstraints:@[
    [self.imageView.leadingAnchor
        constraintEqualToAnchor:self.alertScreen.view.leadingAnchor],
    [self.imageView.trailingAnchor
        constraintEqualToAnchor:self.alertScreen.view.trailingAnchor],
    [self.imageView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
  ]];
}

// Update layout for compact/normal vertical classes.
- (void)updateLimit {
  self.topAlertConstraint.active = NO;
  self.bottomImageConstraint.active = NO;
  self.widthAlertConstraint.active = NO;
  if (self.traitCollection.verticalSizeClass ==
      UIUserInterfaceSizeClassCompact) {
    self.bottomImageConstraint = [self.imageView.bottomAnchor
        constraintEqualToAnchor:self.view.topAnchor];
    self.topAlertConstraint = [self.alertScreen.view.topAnchor
        constraintEqualToAnchor:self.view.topAnchor
                       constant:kCompactTopOffset];
    self.widthAlertConstraint = [self.alertScreen.view.widthAnchor
        constraintEqualToAnchor:self.view.widthAnchor
                     multiplier:kCompactWidthRatio];

  } else {
    self.bottomImageConstraint = [self.imageView.bottomAnchor
        constraintEqualToAnchor:self.view.centerYAnchor
                       constant:kCornerRadius];
    self.topAlertConstraint = [self.alertScreen.view.topAnchor
        constraintEqualToAnchor:self.view.centerYAnchor];
    self.widthAlertConstraint = [self.alertScreen.view.widthAnchor
        constraintEqualToAnchor:self.view.widthAnchor];
  }
  self.widthAlertConstraint.active = YES;
  self.topAlertConstraint.active = YES;
  self.bottomImageConstraint.active = YES;
}

@end
