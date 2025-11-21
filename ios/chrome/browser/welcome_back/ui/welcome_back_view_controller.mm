// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/welcome_back/ui/welcome_back_view_controller.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/first_run/public/best_features_item.h"
#import "ios/chrome/browser/first_run/ui_bundled/best_features/ui/best_features_cell.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/welcome_back/ui/welcome_back_action_handler.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierFeatures,
  // Add other features here if needed.
};

// Size of the icon image view.
constexpr CGFloat kIconSize = 45.0f;
// Side length for the custom favicon.
constexpr CGFloat kCustomFaviconSideLength = 52.0f;

// Spacing Constants
constexpr CGFloat kSpacingBeforeImageNoNav = 24.0f;
constexpr CGFloat kSpacingAfterImage = 16.0f;
constexpr CGFloat kTableViewHorizontalPadding = 6.0f;

}  // namespace

@interface WelcomeBackViewController () <UITableViewDelegate>
@end

@implementation WelcomeBackViewController {
  // List of items to display.
  NSArray<BestFeaturesItem*>* _welcomeBackItems;
  // Table view displaying a Welcome Back Item in each row.
  UITableView* _tableView;
  // Data source for the table view.
  UITableViewDiffableDataSource<NSNumber*, BestFeaturesItem*>* _dataSource;
  // Height constraint for the table view.
  NSLayoutConstraint* _tableViewHeightConstraint;
  // The title string.
  NSString* _title;
  // The user's avatar.
  UIImage* _userAvatar;
  // Reference to the subtitle view to apply correct constraints.
  UITextView* _subtitleView;
}

#pragma mark - UIViewController

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  // Ensure the sheet is at the correct custom detent.
  if (self.navigationController.sheetPresentationController) {
    [self.navigationController.sheetPresentationController animateChanges:^{
      self.navigationController.sheetPresentationController
          .selectedDetentIdentifier = @"preferred_height";
    }];
  }
}

- (void)viewDidLoad {
  // Configure the strings and layout.
  self.titleString = _title;
  self.subtitleString = l10n_util::GetNSString(IDS_IOS_WELCOME_BACK_SUBTITLE);

  // Create the table view.
  _tableView = [self createTableView];
  _dataSource = [self createAndFillDataSource];
  _tableView.dataSource = _dataSource;
  self.underTitleView = _tableView;

  // Set up the action button.
  self.configuration.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_WELCOME_BACK_KEEP_BROWSING_BUTTON);

  // Configure the title style.
  self.titleTextStyle = UIFontTextStyleTitle2;

  // Configure image.
  if (_userAvatar) {
    self.aboveTitleView = [self createCompositeAvatarImage];
  } else {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
    UIImage* logo = MakeSymbolMulticolor(
        CustomSymbolWithPointSize(kMulticolorChromeballSymbol, kIconSize));
#else
    UIImage* logo = CustomSymbolWithPointSize(kChromeProductSymbol, kIconSize);
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
    self.image = logo;
    self.imageBackgroundColor = [UIColor colorNamed:kBackgroundColor];
    self.imageEnclosedWithShadowWithoutBadge = YES;
    self.customFaviconSideLength = kCustomFaviconSideLength;
    self.customSpacingAfterImage = kSpacingAfterImage;
    self.imageEnclosedWithShadowAndBadge = NO;
  }

  self.imageHasFixedSize = YES;
  self.customSpacingBeforeImageIfNoNavigationBar = kSpacingBeforeImageNoNav;

  // Configure layout preferences.
  self.topAlignedLayout = YES;
  self.scrollEnabled = YES;

  [super viewDidLoad];

  _tableViewHeightConstraint =
      [_tableView.heightAnchor constraintEqualToConstant:0];

  [NSLayoutConstraint activateConstraints:@[
    _tableViewHeightConstraint,
    [_tableView.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor
                       constant:kTableViewHorizontalPadding],
    [_tableView.trailingAnchor
        constraintEqualToAnchor:self.contentView.trailingAnchor
                       constant:-kTableViewHorizontalPadding],
  ]];

  // Initial height update.
  [self.view layoutIfNeeded];
  [self updateTableViewHeightConstraint];
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];
  [self updateTableViewHeightConstraint];
  __weak __typeof(self) weakSelf = self;
  [weakSelf.sheetPresentationController animateChanges:^{
    [weakSelf.sheetPresentationController invalidateDetents];
  }];
}

#pragma mark - ConfirmationAlertViewController

- (void)customizeSubtitle:(UITextView*)subtitle {
  [super customizeSubtitle:subtitle];
  // Keep a reference to the subtitle view to apply custom layout.
  _subtitleView = subtitle;
  _subtitleView.translatesAutoresizingMaskIntoConstraints = NO;
  self.subtitleTextStyle = UIFontTextStyleSubheadline;
}

#pragma mark - WelcomeBackScreenConsumer

- (void)setWelcomeBackItems:(NSArray<BestFeaturesItem*>*)items {
  _welcomeBackItems = [items copy];
}

- (void)setTitle:(NSString*)title {
  _title = [title copy];
}

- (void)setAvatar:(UIImage*)userAvatar {
  _userAvatar = userAvatar;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [_tableView deselectRowAtIndexPath:indexPath animated:YES];
}

- (void)tableView:(UITableView*)tableView
    performPrimaryActionForRowAtIndexPath:(NSIndexPath*)indexPath {
  BestFeaturesItem* item = [_dataSource itemIdentifierForIndexPath:indexPath];
  if (!item) {
    return;
  }

  [self.welcomeBackActionHandler didTapBestFeatureItem:item];
}

#pragma mark - Private

// Creates a composite image with the user's avatar centered over the confetti
// background.
- (UIView*)createCompositeAvatarImage {
  UIImage* confettiImage = [UIImage imageNamed:@"confetti_burst_frame"];

  UIImageView* compositeImageView =
      [[UIImageView alloc] initWithImage:confettiImage];
  compositeImageView.translatesAutoresizingMaskIntoConstraints = NO;

  UIImageView* avatarImageView =
      [[UIImageView alloc] initWithImage:_userAvatar];

  avatarImageView.frame = CGRectMake(
      (confettiImage.size.width - kIconSize) / 2.0,
      (confettiImage.size.height - kIconSize) / 2.0, kIconSize, kIconSize);

  avatarImageView.layer.cornerRadius = kIconSize / 2.0f;
  avatarImageView.clipsToBounds = YES;
  avatarImageView.contentMode = UIViewContentModeScaleAspectFill;

  [compositeImageView addSubview:avatarImageView];

  UIView* containerView = [[UIView alloc] init];
  containerView.translatesAutoresizingMaskIntoConstraints = NO;

  [containerView addSubview:compositeImageView];

  [NSLayoutConstraint activateConstraints:@[
    [compositeImageView.leadingAnchor
        constraintEqualToAnchor:containerView.leadingAnchor],
    [compositeImageView.trailingAnchor
        constraintEqualToAnchor:containerView.trailingAnchor],
    [compositeImageView.topAnchor
        constraintEqualToAnchor:containerView.topAnchor
                       constant:kSpacingBeforeImageNoNav],
    [compositeImageView.bottomAnchor
        constraintEqualToAnchor:containerView.bottomAnchor
                       constant:-kSpacingAfterImage]
  ]];

  return containerView;
}

// Creates the table view.
- (UITableView*)createTableView {
  UITableView* tableView =
      [[UITableView alloc] initWithFrame:CGRectZero
                                   style:UITableViewStylePlain];
  tableView.translatesAutoresizingMaskIntoConstraints = NO;
  tableView.delegate = self;
  tableView.showsVerticalScrollIndicator = NO;
  tableView.scrollEnabled = NO;
  tableView.tableFooterView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];

  tableView.accessibilityIdentifier =
      @"WelcomeBackTableViewAccessibilityIdentifier";

  RegisterTableViewCell<BestFeaturesCell>(tableView);

  return tableView;
}

// Creates the data source for the table view.
- (UITableViewDiffableDataSource<NSNumber*, BestFeaturesItem*>*)
    createAndFillDataSource {
  CHECK(_welcomeBackItems);
  __weak __typeof(self) weakSelf = self;
  UITableViewDiffableDataSource<NSNumber*, BestFeaturesItem*>* dataSource =
      [[UITableViewDiffableDataSource alloc]
          initWithTableView:_tableView
               cellProvider:^UITableViewCell*(
                   UITableView* view, NSIndexPath* indexPath,
                   BestFeaturesItem* itemIdentifier) {
                 return [weakSelf cellForTableView:view
                                         indexPath:indexPath
                                    itemIdentifier:itemIdentifier];
               }];

  NSDiffableDataSourceSnapshot<NSNumber*, BestFeaturesItem*>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[ @(SectionIdentifierFeatures) ]];
  [snapshot appendItemsWithIdentifiers:_welcomeBackItems
             intoSectionWithIdentifier:@(SectionIdentifierFeatures)];

  [dataSource applySnapshot:snapshot animatingDifferences:NO];

  return dataSource;
}

// Configures the table view cells.
- (UITableViewCell*)cellForTableView:(UITableView*)tableView
                           indexPath:(NSIndexPath*)indexPath
                      itemIdentifier:(BestFeaturesItem*)itemIdentifier {
  BestFeaturesCell* cell = DequeueTableViewCell<BestFeaturesCell>(tableView);
  [cell setBestFeaturesItem:itemIdentifier];
  cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;

  return cell;
}

// Updates the tableView's height constraint based on its content size.
// This is necessary because scrolling is disabled.
- (void)updateTableViewHeightConstraint {
  [_tableView layoutIfNeeded];
  CGFloat newHeight = _tableView.contentSize.height;
  if (_tableViewHeightConstraint.constant != newHeight) {
    _tableViewHeightConstraint.constant = newHeight;
  }
}

@end
