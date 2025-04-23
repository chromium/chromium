// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/multi_profile_passkey_creation_view_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_container_view.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/credential_provider_extension/favicon_util.h"
#import "ios/chrome/credential_provider_extension/passkey_request_details.h"

namespace {

// Leading, trailing and top margin to use for the screen's title.
constexpr CGFloat kTitleHorizontalAndTopMargin = 24;

// Default font size.
constexpr CGFloat kDefaultFontSize = 14;

// Font size for the username.
constexpr CGFloat kUsernameFontSize = 18;

// Font size for the email address.
constexpr CGFloat kEmailFontSize = 15;

// Border around the passkey information stack view.
constexpr CGFloat kBorderWidth = 16;

// Height of the passkey information view.
constexpr CGFloat kInformationViewHeight = 70;

// The extra space between text lines.
constexpr CGFloat kVerticalSpacing = 4;

// The corner radius of the label.
constexpr CGFloat kCornerRadius = 14;

// Returns the background color for this view.
UIColor* GetBackgroundColor() {
  return [UIColor colorNamed:kPrimaryBackgroundColor];
}

// Creates an attributed string based on the input text and provided parameters.
NSAttributedString* AsAttributedString(NSString* text,
                                       bool is_first_line,
                                       CGFloat font_size,
                                       bool is_bold,
                                       NSString* text_color,
                                       NSTextAlignment alignment) {
  NSMutableParagraphStyle* style = [[NSMutableParagraphStyle alloc] init];
  [style setLineSpacing:kVerticalSpacing];
  [style setAlignment:alignment];

  UIFont* font = is_bold ? [UIFont boldSystemFontOfSize:font_size]
                         : [UIFont systemFontOfSize:font_size];
  NSString* formatted_text =
      is_first_line ? text : [NSString stringWithFormat:@"\n%@", text];
  return [[NSAttributedString alloc]
      initWithString:formatted_text
          attributes:@{
            NSFontAttributeName : font,
            NSForegroundColorAttributeName : [UIColor colorNamed:text_color],
            NSBackgroundColorAttributeName : [UIColor clearColor],
            NSParagraphStyleAttributeName : style
          }];
}

}  // namespace

@interface MultiProfilePasskeyCreationViewController () <
    PromoStyleViewControllerDelegate>

@end

@implementation MultiProfilePasskeyCreationViewController {
  // The view to be used as the navigation bar title view.
  UIView* _navigationItemTitleView;

  // Email address associated with the signed in account.
  NSString* _userEmail;

  // Information about a passkey credential request.
  PasskeyRequestDetails* _passkeyRequestDetails;

  // The favicon view shown by this view controller's view.
  FaviconView* _faviconView;

  // The favicon attributes, if a favicon is available. Nil otherwise.
  FaviconAttributes* _faviconAttributes;

  // The gaia ID associated with the current account
  NSString* _gaia;

  // Delegate for this view controller.
  __weak id<MultiProfilePasskeyCreationViewControllerDelegate>
      _multiProfilePasskeyCreationViewControllerDelegate;
}

- (instancetype)
            initWithDetails:(PasskeyRequestDetails*)passkeyRequestDetails
                       gaia:(NSString*)gaia
                  userEmail:(NSString*)userEmail
                    favicon:(NSString*)favicon
    navigationItemTitleView:(UIView*)navigationItemTitleView
                   delegate:
                       (id<MultiProfilePasskeyCreationViewControllerDelegate>)
                           delegate {
  self = [super initWithTaskRunner:nullptr];
  if (self) {
    _userEmail = userEmail;
    _passkeyRequestDetails = passkeyRequestDetails;
    _gaia = gaia;
    _multiProfilePasskeyCreationViewControllerDelegate = delegate;
    _navigationItemTitleView = navigationItemTitleView;

    // Use the default world icon as the default favicon.
    _faviconAttributes = [FaviconAttributes
        attributesWithImage:
            [[UIImage imageNamed:@"default_world_favicon"]
                imageWithTintColor:[UIColor colorNamed:kTextQuaternaryColor]
                     renderingMode:UIImageRenderingModeAlwaysOriginal]];

    // Attempt to fetch the favicon.
    if (favicon) {
      __weak __typeof(self) weakSelf = self;
      FetchFaviconAsync(favicon, ^(FaviconAttributes* attributes) {
        [weakSelf setFaviconAttributes:attributes];
      });
    }
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.bannerName = @"passkey_generic_banner";
  self.bannerSize = BannerImageSizeType::kExtraShort;

  self.titleText = NSLocalizedString(
      @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEYS_CREATE", @"Create a passkey for");

  self.titleTopMarginWhenNoHeaderImage = kTitleHorizontalAndTopMargin;
  self.titleHorizontalMargin = kTitleHorizontalAndTopMargin;

  self.specificContentView = [self createSpecificContentView];
  self.subtitleBottomMargin = 0;

  self.primaryActionString = NSLocalizedString(
      @"IDS_IOS_CREDENTIAL_PROVIDER_EXTENSION_CREATE", @"Create");
  self.secondaryActionString = NSLocalizedString(
      @"IDS_IOS_CREDENTIAL_PROVIDER_EXTENSION_CANCEL", @"Cancel");

  [super viewDidLoad];

  self.view.backgroundColor = GetBackgroundColor();
  self.navigationItem.titleView = _navigationItemTitleView;

  [_faviconView configureWithAttributes:_faviconAttributes];
}

#pragma mark - PromoStyleViewController

- (UIFontTextStyle)titleLabelFontTextStyle {
  return UIFontTextStyleTitle2;
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  [_multiProfilePasskeyCreationViewControllerDelegate
      validateUserAndCreatePasskeyWithDetails:_passkeyRequestDetails
                                         gaia:_gaia];
}

- (void)didTapSecondaryActionButton {
  [_multiProfilePasskeyCreationViewControllerDelegate
      multiProfilePasskeyCreationViewControllerShouldBeDismissed:self];
}

#pragma mark - Private

// Creates the content view.
- (UIView*)createSpecificContentView {
  UIView* specificContentView = [[UIView alloc] init];
  specificContentView.translatesAutoresizingMaskIntoConstraints = NO;

  UIView* passkeyInformationView = [self createPasskeyInformationView];
  [specificContentView addSubview:passkeyInformationView];

  // Position the passkey information view at the top of the content view.
  AddSameConstraintsToSides(
      passkeyInformationView, specificContentView,
      LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing);

  // Set the height of the passkey information view.
  [passkeyInformationView.bottomAnchor
      constraintEqualToAnchor:passkeyInformationView.topAnchor
                     constant:kInformationViewHeight]
      .active = YES;

  UIView* accountInformationView = [self accountInformationLabel];
  [specificContentView addSubview:accountInformationView];

  // Position the account information view at the bottom of the content view.
  AddSameConstraintsToSides(
      accountInformationView, specificContentView,
      LayoutSides::kLeading | LayoutSides::kTrailing | LayoutSides::kBottom);

  return specificContentView;
}

// Sets the favicon attributes.
- (void)setFaviconAttributes:(FaviconAttributes*)attributes {
  _faviconAttributes = attributes;
}

// Creates the favicon view for the passkey information view.
- (UIView*)iconView {
  FaviconContainerView* faviconContainerView =
      [[FaviconContainerView alloc] init];
  _faviconView = faviconContainerView.faviconView;
  return faviconContainerView;
}

// Creates the view containing the information about the passkey to be created.
- (UIView*)newPasskeyInformationLabel {
  NSMutableAttributedString* full_text =
      [[NSMutableAttributedString alloc] init];

  // Display the passkey's user name and relying party identifier.
  [full_text
      appendAttributedString:AsAttributedString(
                                 _passkeyRequestDetails.userName,
                                 /*is_first_line=*/true, kUsernameFontSize,
                                 /*is_bold=*/false, kTextPrimaryColor,
                                 NSTextAlignmentNatural)];
  [full_text
      appendAttributedString:AsAttributedString(
                                 _passkeyRequestDetails.relyingPartyIdentifier,
                                 /*is_first_line=*/false, kDefaultFontSize,
                                 /*is_bold=*/false, kTextSecondaryColor,
                                 NSTextAlignmentNatural)];

  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.numberOfLines = 2;
  label.attributedText = full_text;
  return label;
}

// Creates and configures the passkey information view.
- (UIView*)createPasskeyInformationView {
  UIStackView* stackView = [[UIStackView alloc] initWithArrangedSubviews:@[]];
  stackView.axis = UILayoutConstraintAxisHorizontal;
  stackView.alignment = UIStackViewAlignmentCenter;
  stackView.layoutMarginsRelativeArrangement = YES;
  stackView.layoutMargins = UIEdgeInsetsMake(0, kBorderWidth, 0, kBorderWidth);
  stackView.spacing = kBorderWidth;
  stackView.layer.cornerRadius = kCornerRadius;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.backgroundColor = [UIColor colorNamed:kGrey100Color];

  [stackView addArrangedSubview:[self iconView]];
  [stackView addArrangedSubview:[self newPasskeyInformationLabel]];

  return stackView;
}

// Creates and configures the account information label.
- (UIView*)accountInformationLabel {
  CHECK(_userEmail);
  NSString* firstLine =
      NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEYS_ACCOUNT",
                        @"Passkeys are saved in Google Password Manager for");

  NSMutableAttributedString* full_text =
      [[NSMutableAttributedString alloc] init];

  // Display the user account used by the CPE to save passkeys.
  [full_text
      appendAttributedString:AsAttributedString(
                                 firstLine, /*is_first_line=*/true,
                                 kDefaultFontSize, /*is_bold=*/false,
                                 kTextSecondaryColor, NSTextAlignmentCenter)];
  [full_text
      appendAttributedString:AsAttributedString(
                                 _userEmail, /*is_first_line=*/false,
                                 kEmailFontSize, /*is_bold=*/true,
                                 kTextPrimaryColor, NSTextAlignmentCenter)];

  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.numberOfLines = 2;
  label.attributedText = full_text;
  return label;
}

@end
