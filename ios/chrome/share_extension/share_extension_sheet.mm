// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/share_extension/share_extension_sheet.h"

#import <UIKit/UIKit.h>

#import "base/apple/bundle_locations.h"
#import "base/check.h"
#import "build/branding_buildflags.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/app_group/app_group_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/branded_navigation_item_title_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/share_extension/account_picker_delegate.h"
#import "ios/chrome/share_extension/account_picker_table.h"
#import "ios/chrome/share_extension/share_extension_delegate.h"

namespace {

enum SharedItemType {
  kURL,
  kImage,
  kText,
};

CGFloat const kInnerViewWidthPadding = 40;
CGFloat const kMainViewHeightPadding = 34;
CGFloat const kMainViewCornerRadius = 12;
CGFloat const kSnapshotViewSize = 150;
CGFloat const kURLStackSpacing = 8;
CGFloat const kDefaultSnapshotViewSize = 60;
CGFloat const kLinkIconSize = 26.0;
CGFloat const kQuoteIconSize = 26.0;
CGFloat const kTextStackSpacing = 30.0;

// The horizontal spacing between image preview and the URL stack.
CGFloat const kInnerViewSpacing = 30;

// Custom radius for the half sheet presentation.
CGFloat const kHalfSheetCornerRadius = 20;

// The coefficient to multiply the title view font with to get the logo size.
constexpr CGFloat kLogoTitleFontMultiplier = 1.25;

// The spacing between the sheet's title and icon.
CGFloat const kTitleViewSpacing = 3.0;

// Custom detent identifier for when the bottom sheet is minimized.
NSString* const kCustomMinimizedDetentIdentifier = @"customMinimizedDetent";

// Constants for the MIM configuration.
CGFloat const kMIMStackSpacing = 16.0;
CGFloat const kAccountRowHeight = 57.0;
CGFloat const kMIMViewCornerRadius = 25.0;
CGFloat const kAccountCellCornerRadius = 10.0;
CGFloat const kAvatarImageDimension = 30.0;
CGFloat const kUpdatedMainViewCornerRadius = 32.0;

}  // namespace

@interface ShareExtensionSheet () <AccountPickerDelegate,
                                   UITableViewDataSource,
                                   UITableViewDelegate>
@end

@implementation ShareExtensionSheet {
  NSString* _primaryString;
  NSString* _secondaryString;
  NSString* _appName;
  SharedItemType _sharedItemType;
  NSArray<AccountInfo*>* _accounts;
  UISheetPresentationControllerDetent* _customDetent;
  UITableView* _accountTableView;
  NSLayoutConstraint* _tableViewHeightConstraint;
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
  self.scrollEnabled = YES;

  self.customScrollViewBottomInsets = 0;
  self.customGradientViewHeight = 0;

  self.titleView = [self configureSheetTitleView];

  self.dismissBarButtonSystemItem = UIBarButtonSystemItemClose;

  if (app_group::MultiProfileShareExtensionEnabled()) {
    self.mainBackgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
    self.underTitleView = [self createUnderTitleViewWithMIM];
  } else {
    self.underTitleView = [self configureMainView];
  }

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

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];

  if (_tableViewHeightConstraint) {
    CGFloat newHeight = _accountTableView.contentSize.height;
    if (_tableViewHeightConstraint.constant != newHeight) {
      _tableViewHeightConstraint.constant = newHeight;
    }
  }

  if (app_group::MultiProfileShareExtensionEnabled() &&
      ![self isScrolledToBottom]) {
    [self scrollToBottom];
  }
}

- (void)setAccounts:(NSArray<AccountInfo*>*)accounts {
  _accounts = [accounts copy];
}

#pragma mark - UITableViewDataSource

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return 1;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return 1;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell =
      [tableView dequeueReusableCellWithIdentifier:@"Account"];
  return [self configureAccountCell:cell];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  AccountPickerTable* accountPickerView =
      [[AccountPickerTable alloc] initWithAccounts:_accounts
                                   selectedAccount:self.selectedAccountInfo];
  accountPickerView.customDetent = _customDetent;
  accountPickerView.delegate = self;
  UINavigationController* presentingNavController =
      [[UINavigationController alloc]
          initWithRootViewController:accountPickerView];
  [self presentViewController:presentingNavController
                     animated:YES
                   completion:nil];
}

#pragma mark - AccountPickerDelegate

- (void)didSelectAccountInTable:(AccountPickerTable*)table
                selectedAccount:(AccountInfo*)selectedAccount {
  _selectedAccountInfo = selectedAccount;
  [_accountTableView reloadData];
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
  CHECK(!_sharedText);
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
  NSString* gaiaID = self.selectedAccountInfo.gaiaID;
  switch (_sharedItemType) {
    case kURL:
      [self.delegate didTapOpenInChromeShareExtensionSheet:self gaiaID:gaiaID];
      return;
    case kImage:
    case kText:
      [self.delegate didTapSearchInChromeShareExtensionSheet:self
                                                      gaiaID:gaiaID];
      return;
  }
}

- (void)confirmationAlertSecondaryAction {
  NSString* gaiaID = self.selectedAccountInfo.gaiaID;
  switch (_sharedItemType) {
    case kURL:
      [self.delegate didTapMoreOptionsShareExtensionSheet:self gaiaID:gaiaID];
      return;
    case kImage:
    case kText:
      [self.delegate didTapSearchInIncognitoShareExtensionSheet:self
                                                         gaiaID:gaiaID];
      return;
  }
}

#pragma mark - Private

// Configures the account cell with the appropriate configuration based on
// `selectedAccountInfo`.
- (UITableViewCell*)configureAccountCell:(UITableViewCell*)cell {
  CHECK(self.selectedAccountInfo);

  UIListContentConfiguration* content = cell.defaultContentConfiguration;
  if ([self.selectedAccountInfo.gaiaID isEqual:app_group::kNoAccount]) {
    content.text = NSLocalizedString(
        @"IDS_IOS_SIGNED_OUT_USER_TITLE_SHARE_EXTENSION",
        @"The title of the item representing a signed out user.");
    content.image = [[UIImage systemImageNamed:@"person.crop.circle"]
        imageWithTintColor:[UIColor colorNamed:kGrey400Color]
             renderingMode:UIImageRenderingModeAlwaysOriginal];

  } else {
    content.text = self.selectedAccountInfo.fullName;
    content.textProperties.numberOfLines = 1;
    content.textProperties.lineBreakMode = NSLineBreakByTruncatingTail;

    content.secondaryText = self.selectedAccountInfo.email;
    content.secondaryTextProperties.numberOfLines = 1;
    content.secondaryTextProperties.lineBreakMode = NSLineBreakByTruncatingTail;

    content.directionalLayoutMargins = NSDirectionalEdgeInsetsMake(8, 0, 8, 0);

    content.image = self.selectedAccountInfo.avatar;

    UIListContentImageProperties* imageProperties = content.imageProperties;
    imageProperties.cornerRadius = kAvatarImageDimension / 2.0;
    imageProperties.maximumSize =
        CGSize(kAvatarImageDimension, kAvatarImageDimension);
  }
  cell.contentConfiguration = content;
  cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  return cell;
}

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
  _customDetent = [UISheetPresentationControllerDetent
      customDetentWithIdentifier:kCustomMinimizedDetentIdentifier
                        resolver:resolver];
  presentationController.detents =
      @[ [UISheetPresentationControllerDetent largeDetent] ];
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

  mainView.backgroundColor = [UIColor colorNamed:kGrey100Color];
  mainView.layer.cornerRadius = kMainViewCornerRadius;
  if (@available(iOS 26, *)) {
    mainView.layer.cornerRadius = kUpdatedMainViewCornerRadius;
  }

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

- (UIStackView*)createUnderTitleViewWithMIM {
  UIView* mainView = [self configureMainView];
  mainView.backgroundColor = [UIColor colorNamed:kGrey200Color];
  mainView.layer.cornerRadius = kMIMViewCornerRadius;
  if (@available(iOS 26, *)) {
    mainView.layer.cornerRadius = kUpdatedMainViewCornerRadius;
  }

  _accountTableView = [self createSelectedAccountTableView];
  UIStackView* underTitleView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ mainView, _accountTableView ]];
  underTitleView.axis = UILayoutConstraintAxisVertical;
  underTitleView.spacing = kMIMStackSpacing;

  return underTitleView;
}

- (UITableView*)createSelectedAccountTableView {
  UITableView* containerTable = [[UITableView alloc] initWithFrame:CGRectZero];
  containerTable.estimatedRowHeight = kAccountRowHeight;
  containerTable.rowHeight = UITableViewAutomaticDimension;
  containerTable.separatorStyle = UITableViewCellSeparatorStyleNone;
  containerTable.layer.cornerRadius = kAccountCellCornerRadius;
  if (@available(iOS 26, *)) {
    containerTable.layer.cornerRadius = kUpdatedMainViewCornerRadius;
  }
  containerTable.scrollEnabled = NO;
  [containerTable registerClass:[UITableViewCell class]
         forCellReuseIdentifier:@"Account"];
  containerTable.dataSource = self;
  containerTable.delegate = self;
  _tableViewHeightConstraint =
      [containerTable.heightAnchor constraintEqualToConstant:kAccountRowHeight];
  _tableViewHeightConstraint.active = YES;
  return containerTable;
}

- (UIStackView*)configureSharedURLView {
  UIImageView* snapshotView = [self configureSnapshotView];
  UIStackView* URLStackView = [self configureURLView];

  UIStackView* containerStack;
  if (snapshotView) {
    containerStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ snapshotView, URLStackView ]];
  } else {
    containerStack =
        [[UIStackView alloc] initWithArrangedSubviews:@[ URLStackView ]];
  }

  containerStack.axis = UILayoutConstraintAxisVertical;
  containerStack.translatesAutoresizingMaskIntoConstraints = NO;
  containerStack.spacing = kInnerViewSpacing;
  containerStack.alignment = UIStackViewAlignmentCenter;

  return containerStack;
}

- (UIView*)configureSharedImageView {
  UIImageView* sharedImageView =
      [[UIImageView alloc] initWithImage:_sharedImage];
  sharedImageView.backgroundColor = [UIColor clearColor];
  sharedImageView.contentMode = UIViewContentModeScaleAspectFit;
  sharedImageView.layer.cornerRadius = kMainViewCornerRadius;
  sharedImageView.clipsToBounds = YES;
  sharedImageView.translatesAutoresizingMaskIntoConstraints = NO;

  // The container view will act as a bounding box for the image view.
  UIView* imageContainerView = [[UIView alloc] init];
  imageContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  [imageContainerView addSubview:sharedImageView];

  // The image view MUST maintain the image's aspect ratio for the corner radius
  // to look correct.
  if (_sharedImage.size.height > 0) {
    CGFloat aspectRatio = _sharedImage.size.width / _sharedImage.size.height;
    [sharedImageView.widthAnchor
        constraintEqualToAnchor:sharedImageView.heightAnchor
                     multiplier:aspectRatio]
        .active = YES;
  }

  AddSameConstraints(imageContainerView, sharedImageView);

  return imageContainerView;
}

- (UIView*)configureSharedTextView {
  UIImageSymbolConfiguration* configuration = [UIImageSymbolConfiguration
      configurationWithPointSize:kQuoteIconSize
                          weight:UIImageSymbolWeightRegular
                           scale:UIImageSymbolScaleMedium];
  UIImageView* quoteImageView = [[UIImageView alloc]
      initWithImage:[UIImage systemImageNamed:@"quote.opening"
                            withConfiguration:configuration]];
  quoteImageView.contentMode = UIViewContentModeCenter;

  UIView* imageContainer = [[UIView alloc] init];
  imageContainer.backgroundColor = [UIColor whiteColor];
  imageContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [imageContainer addSubview:quoteImageView];
  quoteImageView.translatesAutoresizingMaskIntoConstraints = NO;
  imageContainer.layer.cornerRadius = kMainViewCornerRadius;
  imageContainer.clipsToBounds = YES;

  [NSLayoutConstraint activateConstraints:@[
    [imageContainer.widthAnchor
        constraintEqualToConstant:kDefaultSnapshotViewSize],
    [imageContainer.heightAnchor
        constraintEqualToConstant:kDefaultSnapshotViewSize],
    [quoteImageView.centerXAnchor
        constraintEqualToAnchor:imageContainer.centerXAnchor],
    [quoteImageView.centerYAnchor
        constraintEqualToAnchor:imageContainer.centerYAnchor],
  ]];

  UILabel* sharedTextLabel = [[UILabel alloc] init];
  sharedTextLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  sharedTextLabel.adjustsFontForContentSizeCategory = YES;
  sharedTextLabel.numberOfLines = 0;
  if (!self.displayMaxLimit) {
    sharedTextLabel.text = self.sharedText;
  } else {
    NSMutableAttributedString* sharedTextAttributedString =
        [[NSMutableAttributedString alloc] initWithString:self.sharedText];

    NSMutableAttributedString* attributedSpace =
        [[NSMutableAttributedString alloc] initWithString:@" "];
    NSMutableAttributedString* maxLimitString =
        [[NSMutableAttributedString alloc]
            initWithString:NSLocalizedString(
                               @"IDS_IOS_SEARCH_MAX_LIMIT",
                               @"The text at the end of the shared text.")
                attributes:@{
                  NSForegroundColorAttributeName :
                      [UIColor colorNamed:kTextTertiaryColor],
                  NSFontAttributeName : [UIFont
                      preferredFontForTextStyle:UIFontTextStyleFootnote],
                }];

    [sharedTextAttributedString appendAttributedString:attributedSpace];
    [sharedTextAttributedString appendAttributedString:maxLimitString];
    sharedTextLabel.attributedText = sharedTextAttributedString;
    sharedTextLabel.textAlignment = NSTextAlignmentCenter;
  }

  UIStackView* stackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ imageContainer, sharedTextLabel ]];
  stackView.axis = UILayoutConstraintAxisVertical;
  stackView.spacing = kTextStackSpacing;
  stackView.alignment = UIStackViewAlignmentCenter;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;

  return stackView;
}

- (UIImageView*)configureSnapshotView {
  if (!_sharedURLPreview) {
    return [self configureDefaultSnapshotView];
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
      preferredFontDescriptorWithTextStyle:UIFontTextStyleHeadline];
  titleLabel.font = [UIFont systemFontOfSize:fontDescriptor.pointSize
                                      weight:UIFontWeightSemibold];
  titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
  titleLabel.textAlignment = NSTextAlignmentCenter;
  titleLabel.numberOfLines = 2;

  URLLabel.text = [_sharedURL absoluteString];
  UIFontDescriptor* URLfontDescriptor = [UIFontDescriptor
      preferredFontDescriptorWithTextStyle:UIFontTextStyleCallout];
  URLLabel.font = [UIFont systemFontOfSize:URLfontDescriptor.pointSize
                                    weight:UIFontWeightRegular];
  URLLabel.textColor = [UIColor colorNamed:kTextTertiaryColor];
  URLLabel.lineBreakMode = NSLineBreakByTruncatingTail;
  URLLabel.textAlignment = NSTextAlignmentCenter;
  URLLabel.numberOfLines = 2;

  UIStackView* URLStackView =
      [[UIStackView alloc] initWithArrangedSubviews:@[ titleLabel, URLLabel ]];

  URLStackView.axis = UILayoutConstraintAxisVertical;
  URLStackView.alignment = UIStackViewAlignmentCenter;
  URLStackView.distribution = UIStackViewDistributionEqualCentering;
  URLStackView.spacing = kURLStackSpacing;
  URLStackView.translatesAutoresizingMaskIntoConstraints = NO;

  return URLStackView;
}

- (UIImageView*)configureDefaultSnapshotView {
  CHECK(!_sharedURLPreview);
  UIImageSymbolConfiguration* configuration = [UIImageSymbolConfiguration
      configurationWithPointSize:kLinkIconSize
                          weight:UIImageSymbolWeightMedium
                           scale:UIImageSymbolScaleMedium];
  _sharedURLPreview = [UIImage systemImageNamed:@"link"
                              withConfiguration:configuration];
  UIImageView* snapshotView =
      [[UIImageView alloc] initWithImage:_sharedURLPreview];
  snapshotView.backgroundColor = [UIColor whiteColor];
  snapshotView.layer.cornerRadius = kMainViewCornerRadius;

  snapshotView.contentMode = UIViewContentModeCenter;
  snapshotView.layer.masksToBounds = YES;
  snapshotView.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [snapshotView.widthAnchor
        constraintEqualToConstant:kDefaultSnapshotViewSize],
    [snapshotView.heightAnchor
        constraintEqualToConstant:kDefaultSnapshotViewSize],
  ]];
  return snapshotView;
}

@end
