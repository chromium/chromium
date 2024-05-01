// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/ui/panel_content_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/public/commands/contextual_sheet_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
// Height of the top header.
const CGFloat kHeaderHeight = 58;

// Size of the close button.
const CGFloat kCloseButtonIconSize = 30;

// Top margin for the close button.
const CGFloat kCloseButtonTopMargin = 10;

// Margin between the close button and the trailing edge of the screen.
const CGFloat kCloseButtonTrailingMargin = 16;

// Identifier for the one section in this collection view.
NSString* const kSectionIdentifier = @"section1";

}  // namespace

@implementation PanelContentViewController {
  // The header view at the top of the panel.
  UIView* _headerView;

  // The button to close the view.
  UIButton* _closeButton;

  // The collection view managed by this view controller
  UICollectionView* _collectionView;

  // The data source for this collection view.
  UICollectionViewDiffableDataSource<NSString*, NSString*>* _diffableDataSource;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  [self createCollectionView];

  // Set up some initial test data for the collection view.
  NSDiffableDataSourceSnapshot<NSString*, NSString*>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[ kSectionIdentifier ]];
  [snapshot appendItemsWithIdentifiers:@[ @"testItem1", @"testItem2" ]];
  [_diffableDataSource applySnapshot:snapshot animatingDifferences:NO];

  [self.view addSubview:_collectionView];
  AddSameConstraints(self.view, _collectionView);

  // Create and set up the header view. This should be added after the
  // collection view because the header should go above the collection view.
  _headerView = [[UIView alloc] init];
  _headerView.translatesAutoresizingMaskIntoConstraints = NO;
  _headerView.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  [self.view addSubview:_headerView];

  [NSLayoutConstraint activateConstraints:@[
    [self.view.leadingAnchor constraintEqualToAnchor:_headerView.leadingAnchor],
    [self.view.trailingAnchor
        constraintEqualToAnchor:_headerView.trailingAnchor],
    [self.view.topAnchor constraintEqualToAnchor:_headerView.topAnchor],
    [_headerView.heightAnchor constraintEqualToConstant:kHeaderHeight],
  ]];

  [self createCloseButton];

  [_headerView addSubview:_closeButton];
  [NSLayoutConstraint activateConstraints:@[
    [_closeButton.topAnchor constraintEqualToAnchor:_headerView.topAnchor
                                           constant:kCloseButtonTopMargin],
    [_headerView.trailingAnchor
        constraintEqualToAnchor:_closeButton.trailingAnchor
                       constant:kCloseButtonTrailingMargin],
  ]];
}

- (void)closeButtonTapped {
  [self.contextualSheetCommandHandler hideContextualSheet];
}

#pragma mark - Private

// Creates the layout for the collection view.
- (UICollectionViewLayout*)createLayout {
  UICollectionLayoutListConfiguration* configuration =
      [[UICollectionLayoutListConfiguration alloc]
          initWithAppearance:UICollectionLayoutListAppearancePlain];
  return [UICollectionViewCompositionalLayout
      layoutWithListConfiguration:configuration];
}

// Creates and initializes `_collectionView`.
- (void)createCollectionView {
  _collectionView =
      [[UICollectionView alloc] initWithFrame:CGRectZero
                         collectionViewLayout:[self createLayout]];
  _collectionView.translatesAutoresizingMaskIntoConstraints = NO;
  _collectionView.backgroundColor = UIColor.greenColor;
  _collectionView.contentInset = UIEdgeInsetsMake(kHeaderHeight, 0, 0, 0);

  UICollectionViewCellRegistration* registration =
      [UICollectionViewCellRegistration
          registrationWithCellClass:[UICollectionViewListCell class]
               configurationHandler:^(UICollectionViewListCell* cell,
                                      NSIndexPath* indexPath, id item) {
                 UIListContentConfiguration* configuration =
                     cell.defaultContentConfiguration;

                 configuration.text = @"Test Test";

                 cell.contentConfiguration = configuration;
               }];

  auto cellProvider =
      ^UICollectionViewCell*(UICollectionView* collectionView,
                             NSIndexPath* indexPath, id itemIdentifier) {
        return [collectionView
            dequeueConfiguredReusableCellWithRegistration:registration
                                             forIndexPath:indexPath
                                                     item:itemIdentifier];
      };

  _diffableDataSource = [[UICollectionViewDiffableDataSource alloc]
      initWithCollectionView:_collectionView
                cellProvider:cellProvider];

  _collectionView.dataSource = _diffableDataSource;
}

// Creates and initializes `_closeButton`.
- (void)createCloseButton {
  UIImage* closeButtonImage = SymbolWithPalette(
      DefaultSymbolWithPointSize(kXMarkCircleFillSymbol, kCloseButtonIconSize),
      @[
        [UIColor colorNamed:kGrey600Color],
        [UIColor colorNamed:kGrey200Color],
      ]);
  UIButtonConfiguration* closeButtonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  closeButtonConfiguration.image = closeButtonImage;
  closeButtonConfiguration.contentInsets = NSDirectionalEdgeInsetsZero;
  closeButtonConfiguration.buttonSize = UIButtonConfigurationSizeSmall;
  closeButtonConfiguration.accessibilityLabel =
      l10n_util::GetNSString(IDS_CLOSE);
  __weak __typeof(self) weakSelf = self;
  _closeButton = [UIButton
      buttonWithConfiguration:closeButtonConfiguration
                primaryAction:[UIAction actionWithHandler:^(UIAction* action) {
                  [weakSelf closeButtonTapped];
                }]];
  _closeButton.translatesAutoresizingMaskIntoConstraints = NO;
}

@end
