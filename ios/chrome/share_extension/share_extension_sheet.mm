// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/share_extension/share_extension_sheet.h"

#import <UIKit/UIKit.h>

#import "base/apple/bundle_locations.h"
#import "base/check.h"
#import "build/branding_buildflags.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/branded_navigation_item_title_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/share_extension/share_extension_delegate.h"

namespace {

enum SharedItemType {
  kURL,
  kImage,
  kText,
};

CGFloat const kInnerViewWidthPadding = 32;
CGFloat const kMainViewHeightPadding = 34;
CGFloat const kMainViewCornerRadius = 12;
CGFloat const kSnapshotViewSize = 72;
CGFloat const kURLStackSpacing = 2;

// The horizontal spacing between image preview and the URL stack.
CGFloat const kInnerViewSpacing = 16;

CGFloat const kDismissButtonSize = 28;
CGFloat const kSharedImageHeight = 181;

// Custom radius for the half sheet presentation.
CGFloat const kHalfSheetCornerRadius = 20;

// The coefficient to multiply the title view font with to get the logo size.
constexpr CGFloat kLogoTitleFontMultiplier = 1.25;

// The spacing between the sheet's title and icon.
CGFloat const kTitleViewSpacing = 3.0;

// Custom detent identifier for when the bottom sheet is minimized.
NSString* const kCustomMinimizedDetentIdentifier = @"customMinimizedDetent";

}  // namespace

@implementation ShareExtensionSheet {
  NSString* _primaryString;
  NSString* _secondaryString;
  NSString* _appName;
  SharedItemType _sharedItemType;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _appName = [base::apple::FrameworkBundle()
        objectForInfoDictionaryKey:@"CFBundleDisplayName"];
  }
  return self;
}

- (void)viewDidLoad {
  self.actionHandler = self;
  self.primaryActionString = _primaryString;
  self.secondaryActionString = _secondaryString;

  self.scrollEnabled = NO;
  self.showDismissBarButton = YES;
  self.alwaysShowImage = YES;
  self.topAlignedLayout = YES;

  self.customScrollViewBottomInsets = 0;
  self.customGradientViewHeight = 0;

  self.titleView = [self configureSheetTitleView];

  self.underTitleView = [self configureMainView];
  self.dismissBarButtonSystemItem = UIBarButtonSystemItemClose;
  self.customDismissBarButtonImage = [self configureDismissButtonIcon];

  [super viewDidLoad];
  [self setUpBottomSheetPresentationController];
  [self setUpBottomSheetDetents];
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  if (self.isBeingDismissed) {
    if (!self.dismissedFromSheetAction) {
      [self.delegate shareExtensionSheetDidDisappear:self];
    }
  }
}

#pragma mark - Public

- (void)setSharedURL:(NSURL*)sharedURL {
  CHECK(!_sharedImage && !_sharedText);
  _sharedURL = sharedURL;
  _sharedItemType = kURL;
  _primaryString =
      [NSString stringWithFormat:
                    @"%@ %@",
                    NSLocalizedString(
                        @"IDS_IOS_OPEN_IN_BUTTON_SHARE_EXTENSION",
                        @"The label of theopen in button in share extension."),
                    _appName];
  _secondaryString = NSLocalizedString(
      @"IDS_IOS_MORE_OPTIONS_BUTTON_SHARE_EXTENSION",
      @"The label of the more options button in share extension.");
}

- (void)setSharedTitle:(NSString*)sharedTitle {
  CHECK(!_sharedImage && !_sharedText);
  _sharedTitle = sharedTitle;
}

- (void)setSharedURLPreview:(UIImage*)sharedURLPreview {
  CHECK(!_sharedImage && !_sharedText);
  _sharedURLPreview = sharedURLPreview;
}

- (void)setSharedImage:(UIImage*)sharedImage {
  CHECK(!_sharedURL && !_sharedTitle && !_sharedText);
  _sharedImage = sharedImage;
  _sharedItemType = kImage;
  _primaryString = [NSString
      stringWithFormat:
          @"%@ %@",
          NSLocalizedString(
              @"IDS_IOS_SEARCH_IN_BUTTON_SHARE_EXTENSION",
              @"The label of the search in button in share extension."),
          _appName];
  _secondaryString = NSLocalizedString(
      @"IDS_IOS_SEARCH_IN_INCOGNITO_BUTTON_SHARE_EXTENSION",
      @"The label of the search in incognito button in share extension.");
}

- (void)setSharedText:(NSString*)sharedText {
  CHECK(!_sharedURL && !_sharedTitle && !_sharedImage);
  _sharedText = sharedText;
  _sharedItemType = kText;
  _primaryString = [NSString
      stringWithFormat:
          @"%@ %@",
          NSLocalizedString(
              @"IDS_IOS_SEARCH_IN_BUTTON_SHARE_EXTENSION",
              @"The label of the search in button in share extension."),
          _appName];
  _secondaryString = NSLocalizedString(
      @"IDS_IOS_SEARCH_IN_INCOGNITO_BUTTON_SHARE_EXTENSION",
      @"The label of the search in incognito button in share extension.");
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertDismissAction {
  [self.delegate didTapCloseShareExtensionSheet:self];
}

- (void)confirmationAlertPrimaryAction {
  switch (_sharedItemType) {
    case kURL:
      [self.delegate didTapOpenInChromeShareExtensionSheet:self];
      return;
    case kImage:
    case kText:
      [self.delegate didTapSearchInChromeShareExtensionSheet:self];
      return;
  }
}

- (void)confirmationAlertSecondaryAction {
  switch (_sharedItemType) {
    case kURL:
      [self.delegate didTapMoreOptionsShareExtensionSheet:self];
      return;
    case kImage:
    case kText:
      [self.delegate didTapSearchInIncognitoShareExtensionSheet:self];
      return;
  }
}

#pragma mark - Private

// Configures the bottom sheet's presentation controller appearance.
- (void)setUpBottomSheetPresentationController {
  self.modalPresentationStyle = UIModalPresentationFormSheet;
  UISheetPresentationController* presentationController =
      self.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.widthFollowsPreferredContentSizeWhenEdgeAttached = YES;
  presentationController.preferredCornerRadius = kHalfSheetCornerRadius;
}

// Configures the bottom sheet's detents.
- (void)setUpBottomSheetDetents {
  UISheetPresentationController* presentationController =
      self.sheetPresentationController;
  CGFloat bottomSheetHeight = [self preferredHeightForContent];
  auto resolver = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    return bottomSheetHeight;
  };
  UISheetPresentationControllerDetent* customDetent =
      [UISheetPresentationControllerDetent
          customDetentWithIdentifier:kCustomMinimizedDetentIdentifier
                            resolver:resolver];
  presentationController.detents = @[ customDetent ];
  presentationController.selectedDetentIdentifier =
      kCustomMinimizedDetentIdentifier;
}

- (UIView*)configureSheetTitleView {
  BrandedNavigationItemTitleView* titleView =
      [[BrandedNavigationItemTitleView alloc]
          initWithFont:[UIFont systemFontOfSize:UIFont.labelFontSize]];
  titleView.title = _appName;
  UIImageSymbolConfiguration* titleViewIconConfiguration =
      [UIImageSymbolConfiguration
          configurationWithPointSize:UIFont.labelFontSize *
                                     kLogoTitleFontMultiplier
                              weight:UIImageSymbolWeightMedium
                               scale:UIImageSymbolScaleMedium];
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  UIImage* titleViewSymbol = [UIImage imageNamed:@"multicolor_chromeball"
                                        inBundle:nil
                               withConfiguration:titleViewIconConfiguration];
  titleView.imageLogo = [titleViewSymbol
      imageByApplyingSymbolConfiguration:
          [UIImageSymbolConfiguration configurationPreferringMulticolor]];

#else
  titleView.imageLogo = [UIImage imageNamed:@"chrome_product"
                                   inBundle:nil
                          withConfiguration:titleViewIconConfiguration];
#endif

  titleView.titleLogoSpacing = kTitleViewSpacing;
  titleView.accessibilityLabel = [NSString
      stringWithFormat:
          @"%@ %@",
          NSLocalizedString(
              @"IDS_IOS_ACCESSIBILITY_LABEL_SHARE_EXTENSION",
              @"The accessible name for the Chrome logo in the header."),
          _appName];

  return titleView;
}

- (UIView*)configureMainView {
  UIView* mainView = [[UIView alloc] init];
  UIView* innerView;
  if (_sharedURL) {
    innerView = [self configureSharedURLView];
  } else if (_sharedImage) {
    innerView = [self configureSharedImageView];
  } else if (_sharedText) {
    innerView = [self configureSharedTextView];
  }

  CHECK(innerView);
  [mainView addSubview:innerView];

  mainView.backgroundColor = [UIColor colorNamed:kTertiaryBackgroundColor];
  mainView.layer.cornerRadius = kMainViewCornerRadius;

  innerView.translatesAutoresizingMaskIntoConstraints = NO;
  mainView.translatesAutoresizingMaskIntoConstraints = NO;

  [NSLayoutConstraint activateConstraints:@[
    [innerView.widthAnchor constraintEqualToAnchor:mainView.widthAnchor
                                          constant:-kInnerViewWidthPadding],
    [mainView.heightAnchor
        constraintGreaterThanOrEqualToAnchor:innerView.heightAnchor
                                    constant:kMainViewHeightPadding],
  ]];

  AddSameCenterConstraints(mainView, innerView);

  return mainView;
}

- (UIStackView*)configureSharedURLView {
  UIImageView* snapshotView = [self configureSnapshotView];
  UIStackView* URLStackView = [self configureURLView];
  URLStackView.translatesAutoresizingMaskIntoConstraints = NO;
  UIView* URLView = [[UIView alloc] init];

  [URLView addSubview:URLStackView];

  UIStackView* containerStack;
  if (snapshotView) {
    containerStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ snapshotView, URLView ]];
  } else {
    containerStack =
        [[UIStackView alloc] initWithArrangedSubviews:@[ URLView ]];
  }

  containerStack.axis = UILayoutConstraintAxisHorizontal;
  containerStack.translatesAutoresizingMaskIntoConstraints = NO;
  containerStack.spacing = kInnerViewSpacing;
  containerStack.alignment = UIStackViewAlignmentCenter;
  URLView.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [URLView.heightAnchor
        constraintGreaterThanOrEqualToAnchor:snapshotView.heightAnchor],
    [URLView.heightAnchor
        constraintGreaterThanOrEqualToAnchor:URLStackView.heightAnchor],
    [URLStackView.widthAnchor constraintEqualToAnchor:URLView.widthAnchor],
    [containerStack.heightAnchor
        constraintGreaterThanOrEqualToAnchor:URLView.heightAnchor],
  ]];
  AddSameCenterConstraints(URLView, URLStackView);

  return containerStack;
}

- (UIView*)configureSharedImageView {
  UIImageView* sharedImageView =
      [[UIImageView alloc] initWithImage:_sharedImage];
  sharedImageView.backgroundColor = [UIColor clearColor];

  sharedImageView.layer.cornerRadius = kMainViewCornerRadius;
  sharedImageView.contentMode = UIViewContentModeScaleAspectFill;
  sharedImageView.layer.masksToBounds = YES;
  sharedImageView.translatesAutoresizingMaskIntoConstraints = NO;
  [sharedImageView.heightAnchor constraintEqualToConstant:kSharedImageHeight]
      .active = YES;
  return sharedImageView;
}

- (UIView*)configureSharedTextView {
  UILabel* sharedTextLabel = [[UILabel alloc] init];
  sharedTextLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  sharedTextLabel.adjustsFontForContentSizeCategory = YES;
  sharedTextLabel.numberOfLines = 0;
  if (!self.displayMaxLimit) {
    sharedTextLabel.text = self.sharedText;
    return sharedTextLabel;
  }

  NSMutableAttributedString* sharedTextAttributedString =
      [[NSMutableAttributedString alloc] initWithString:self.sharedText];

  NSMutableAttributedString* attributedSpace =
      [[NSMutableAttributedString alloc] initWithString:@" "];
  NSMutableAttributedString* maxLimitString = [[NSMutableAttributedString alloc]
      initWithString:NSLocalizedString(
                         @"IDS_IOS_SEARCH_MAX_LIMIT",
                         @"The text at the end of the shared text.")
          attributes:@{
            NSForegroundColorAttributeName :
                [UIColor colorNamed:kTextTertiaryColor],
            NSFontAttributeName :
                [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote],
          }];

  [sharedTextAttributedString appendAttributedString:attributedSpace];
  [sharedTextAttributedString appendAttributedString:maxLimitString];
  sharedTextLabel.attributedText = sharedTextAttributedString;
  return sharedTextLabel;
}

- (UIImageView*)configureSnapshotView {
  if (!_sharedURLPreview) {
    return nil;
  }

  UIImageView* snapshotView =
      [[UIImageView alloc] initWithImage:_sharedURLPreview];
  snapshotView.backgroundColor = [UIColor clearColor];

  snapshotView.layer.cornerRadius = kMainViewCornerRadius;
  snapshotView.contentMode = UIViewContentModeScaleAspectFill;
  snapshotView.layer.masksToBounds = YES;
  snapshotView.translatesAutoresizingMaskIntoConstraints = NO;

  [NSLayoutConstraint activateConstraints:@[
    [snapshotView.widthAnchor constraintEqualToConstant:kSnapshotViewSize],
    [snapshotView.heightAnchor constraintEqualToConstant:kSnapshotViewSize],
  ]];
  return snapshotView;
}

- (UIStackView*)configureURLView {
  CHECK(_sharedURL);
  UILabel* titleLabel = [[UILabel alloc] init];
  UILabel* URLLabel = [[UILabel alloc] init];

  titleLabel.text = _sharedTitle;
  UIFontDescriptor* fontDescriptor = [UIFontDescriptor
      preferredFontDescriptorWithTextStyle:UIFontTextStyleSubheadline];
  titleLabel.font = [UIFont systemFontOfSize:fontDescriptor.pointSize
                                      weight:UIFontWeightSemibold];
  titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
  titleLabel.numberOfLines = 2;

  URLLabel.text = [_sharedURL absoluteString];
  URLLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  URLLabel.textColor = [UIColor colorNamed:kTextTertiaryColor];
  URLLabel.lineBreakMode = NSLineBreakByTruncatingTail;
  URLLabel.numberOfLines = 2;

  UIStackView* URLStackView =
      [[UIStackView alloc] initWithArrangedSubviews:@[ titleLabel, URLLabel ]];

  URLStackView.axis = UILayoutConstraintAxisVertical;
  URLStackView.alignment = UIStackViewAlignmentLeading;
  URLStackView.spacing = kURLStackSpacing;
  URLStackView.translatesAutoresizingMaskIntoConstraints = NO;

  return URLStackView;
}

- (UIImage*)configureDismissButtonIcon {
  UIImageSymbolConfiguration* colorConfig =
      [UIImageSymbolConfiguration configurationWithPaletteColors:@[
        [UIColor colorNamed:kTextTertiaryColor],
        [UIColor colorNamed:kGrey200Color]
      ]];

  UIImageSymbolConfiguration* dismissButtonConfiguration =
      [UIImageSymbolConfiguration
          configurationWithPointSize:kDismissButtonSize
                              weight:UIImageSymbolWeightMedium
                               scale:UIImageSymbolScaleMedium];
  dismissButtonConfiguration = [dismissButtonConfiguration
      configurationByApplyingConfiguration:colorConfig];

  return [UIImage systemImageNamed:@"xmark.circle.fill"
                 withConfiguration:dismissButtonConfiguration];
}

@end
