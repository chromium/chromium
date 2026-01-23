// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/ui/passkey_creation_bottom_sheet_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/settings/ui_bundled/password/create_password_manager_title_view.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/branded_navigation_item_title_view.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

// Corner radius of the account info view.
constexpr CGFloat kAccountInfoViewCornerRadius = 16;

// Margin for the content within the account info view.
constexpr CGFloat kAccountInfoViewContentMargin = 16;

// Horizontal spacing between the icon and the text in the account info view.
constexpr CGFloat kAccountInfoViewHorizontalSpacing = 16;

// Vertical spacing between the account info view and the subtitle.
constexpr CGFloat kAccountInfoToSubtitleSpacing = 12;

// Size of the favicon/icon in the account info view.
constexpr CGFloat kFaviconSize = 30;

// Vertical spacing between the branding view and the passkey logo.
constexpr CGFloat kBrandingAndPasskeyLogoSpacing = 16;

// The spacing from the top of the bottom sheet to the GPM branding view.
constexpr CGFloat kCustomSpacingBeforeImage = 16;

// Custom spacing of the bottom sheet.
constexpr CGFloat kCustomSpacing = 12;

// Configures the title view of this ViewController.
UIView* SetUpTitleView() {
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_CREDENTIAL_BOTTOM_SHEET_TITLE);
  UIView* title_view = password_manager::CreatePasswordManagerTitleView(title);
  return title_view;
}

ButtonStackConfiguration* SetUpButtons() {
  ButtonStackConfiguration* configuration =
      [[ButtonStackConfiguration alloc] init];
  configuration.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_PASSKEY_CREATION_BOTTOM_SHEET_CREATE);
  configuration.secondaryActionString = l10n_util::GetNSString(
      IDS_IOS_PASSKEY_CREATION_BOTTOM_SHEET_SAVE_ANOTHER_WAY);
  return configuration;
}

// Creates the view sitting on above the title view, containing the GPM branding
// and the passkey banner.
UIView* CreateAboveTitleView() {
  UIView* brandingView = SetUpTitleView();

  UIImage* passkeyBanner = [UIImage imageNamed:@"passkey_generic_banner"];
  CHECK(passkeyBanner);
  UIImageView* imageView = [[UIImageView alloc] initWithImage:passkeyBanner];
  imageView.contentMode = UIViewContentModeScaleAspectFit;
  imageView.translatesAutoresizingMaskIntoConstraints = NO;

  UIStackView* stackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ brandingView, imageView ]];
  stackView.axis = UILayoutConstraintAxisVertical;
  stackView.alignment = UIStackViewAlignmentCenter;
  stackView.spacing = kBrandingAndPasskeyLogoSpacing;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;

  return stackView;
}

// Creates the main horizontal stack for the account info box.
UIStackView* CreateAccountInfoStackView() {
  UIStackView* horizontalStack = [[UIStackView alloc] init];
  horizontalStack.translatesAutoresizingMaskIntoConstraints = NO;
  horizontalStack.axis = UILayoutConstraintAxisHorizontal;
  horizontalStack.spacing = kAccountInfoViewHorizontalSpacing;
  horizontalStack.alignment = UIStackViewAlignmentCenter;
  horizontalStack.backgroundColor =
      [UIColor colorNamed:kSecondaryBackgroundColor];
  horizontalStack.layer.cornerRadius = kAccountInfoViewCornerRadius;
  horizontalStack.layoutMarginsRelativeArrangement = YES;
  horizontalStack.directionalLayoutMargins = NSDirectionalEdgeInsetsMake(
      kAccountInfoViewContentMargin, kAccountInfoViewContentMargin,
      kAccountInfoViewContentMargin, kAccountInfoViewContentMargin);
  return horizontalStack;
}

// Creates the email label.
UILabel* CreateUsernameLabel() {
  UILabel* emailLabel = [[UILabel alloc] init];
  emailLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  emailLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  emailLabel.adjustsFontForContentSizeCategory = YES;
  emailLabel.numberOfLines = 1;
  emailLabel.lineBreakMode = NSLineBreakByTruncatingMiddle;
  return emailLabel;
}

// Creates the domain label.
UILabel* CreateDomainLabel() {
  UILabel* domainLabel = [[UILabel alloc] init];
  domainLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  domainLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  domainLabel.adjustsFontForContentSizeCategory = YES;
  domainLabel.numberOfLines = 1;
  domainLabel.lineBreakMode = NSLineBreakByTruncatingMiddle;
  return domainLabel;
}

// Creates the vertical stack for the email and domain labels.
UIStackView* CreateLabelsStackView(NSArray<UIView*>* views) {
  UIStackView* textStack = [[UIStackView alloc] initWithArrangedSubviews:views];
  textStack.translatesAutoresizingMaskIntoConstraints = NO;
  textStack.axis = UILayoutConstraintAxisVertical;
  textStack.spacing = 0;
  textStack.alignment = UIStackViewAlignmentLeading;
  return textStack;
}

// Creates the subtitle label.
UILabel* CreateSubtitleLabel() {
  UILabel* subtitleLabel = [[UILabel alloc] init];
  subtitleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  subtitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  subtitleLabel.adjustsFontForContentSizeCategory = YES;
  subtitleLabel.numberOfLines = 0;
  subtitleLabel.lineBreakMode = NSLineBreakByWordWrapping;
  subtitleLabel.textAlignment = NSTextAlignmentCenter;
  return subtitleLabel;
}

}  // namespace

@interface PasskeyCreationBottomSheetViewController () {
  // The username for the passkey request.
  NSString* _username;

  // The email for the passkey request.
  NSString* _email;

  // URL of the current page the bottom sheet is being displayed on.
  GURL _url;

  // The passkey creation handler for user actions.
  __weak id<BrowserCoordinatorCommands> _handler;

  // The favicon loader.
  raw_ptr<FaviconLoader> _faviconLoader;
}

@end

@implementation PasskeyCreationBottomSheetViewController

- (instancetype)initWithHandler:(id<BrowserCoordinatorCommands>)handler
                  faviconLoader:(FaviconLoader*)faviconLoader {
  self = [super initWithConfiguration:SetUpButtons()];
  if (self) {
    _handler = handler;
    _faviconLoader = faviconLoader;
  }
  return self;
}

- (void)viewDidLoad {
  self.view.accessibilityViewIsModal = YES;

  // Set the properties read by the base class when constructing the
  // views in `-[ConfirmationAlertViewController viewDidLoad]`.
  self.imageHasFixedSize = YES;
  self.showsVerticalScrollIndicator = NO;
  self.topAlignedLayout = YES;

  self.aboveTitleView = CreateAboveTitleView();
  self.customSpacingBeforeImage = kCustomSpacingBeforeImage;
  self.customSpacing = kCustomSpacing;
  self.shouldFillInformationStack = YES;

  self.titleString =
      l10n_util::GetNSString(IDS_IOS_PASSKEY_CREATION_BOTTOM_SHEET_TITLE);
  self.titleTextStyle = UIFontTextStyleTitle2;

  // Using underTitleView to combine account info view and subtitle together.
  self.underTitleView = [self createUnderTitleView];

  [super viewDidLoad];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [_handler dismissPasskeyCreation];
  }
}

#pragma mark - PasskeyCreationBottomSheetConsumer

- (void)setUsername:(NSString*)username email:(NSString*)email url:(GURL)url {
  _username = username;
  _email = email;
  _url = url;
}

#pragma mark - Private

// Configures the subtitle string of this ViewController.
- (NSString*)subtitle {
  NSString* subtitle =
      l10n_util::GetNSString(IDS_IOS_PASSKEY_CREATION_BOTTOM_SHEET_SUBTITLE);
  NSString* accountInfo =
      l10n_util::GetNSString(IDS_IOS_PASSKEY_CREATION_BOTTOM_SHEET_ACCOUNT);
  NSString* email = _email;
  return
      [NSString stringWithFormat:@"%@\n%@\n%@", subtitle, accountInfo, email];
}

// Creates the view containing the account information (Icon, Email, Domain).
- (UIView*)createAccountInfoView {
  UIStackView* horizontalStack = CreateAccountInfoStackView();

  if (_faviconLoader) {
    FaviconView* faviconView = [[FaviconView alloc] init];
    faviconView.translatesAutoresizingMaskIntoConstraints = NO;
    faviconView.contentMode = UIViewContentModeScaleAspectFit;
    faviconView.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];

    faviconView.contentMode = UIViewContentModeScaleAspectFit;
    faviconView.tintColor = [UIColor colorNamed:kTextPrimaryColor];
    _faviconLoader->FaviconForPageUrl(
        _url, kFaviconSize, kFaviconSize,
        /*fallback_to_google_server=*/true,
        ^(FaviconAttributes* attributes, bool cached) {
          [faviconView configureWithAttributes:attributes];
        });
    [horizontalStack addArrangedSubview:faviconView];
  }

  UILabel* usernameLabel = CreateUsernameLabel();
  usernameLabel.text = _username;

  UILabel* domainLabel = CreateDomainLabel();
  domainLabel.text = base::SysUTF8ToNSString(_url.host());

  UIStackView* textStack =
      CreateLabelsStackView(@[ usernameLabel, domainLabel ]);

  [horizontalStack addArrangedSubview:textStack];

  return horizontalStack;
}

// Creates the view containing the subtitle and account information.
- (UIView*)createUnderTitleView {
  UILabel* subtitleLabel = CreateSubtitleLabel();
  subtitleLabel.text = [self subtitle];

  UIStackView* stackView = [[UIStackView alloc] initWithArrangedSubviews:@[
    [self createAccountInfoView], subtitleLabel
  ]];
  stackView.axis = UILayoutConstraintAxisVertical;
  stackView.spacing = kAccountInfoToSubtitleSpacing;
  stackView.alignment = UIStackViewAlignmentFill;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  return stackView;
}

@end
