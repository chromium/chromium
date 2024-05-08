// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/ui/panel_content_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/contextual_panel/ui/panel_block_data.h"
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
  UIVisualEffectView* _headerView;

  // The button to close the view.
  UIButton* _closeButton;

  // The collection view managed by this view controller
  UICollectionView* _collectionView;

  // The data source for this collection view.
  UICollectionViewDiffableDataSource<NSString*, NSString*>* _diffableDataSource;

  // The blocks currently being displayed.
  NSArray<PanelBlockData*>* _panelBlocks;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  [self createCollectionView];

  [self.view addSubview:_collectionView];
  AddSameConstraints(self.view, _collectionView);

  // Create and set up the header view. This should be added after the
  // collection view because the header should go above the collection view.
  UIBlurEffect* blurEffect =
      [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemMaterial];
  _headerView = [[UIVisualEffectView alloc] initWithEffect:blurEffect];
  _headerView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_headerView];

  [NSLayoutConstraint activateConstraints:@[
    [self.view.leadingAnchor constraintEqualToAnchor:_headerView.leadingAnchor],
    [self.view.trailingAnchor
        constraintEqualToAnchor:_headerView.trailingAnchor],
    [self.view.topAnchor constraintEqualToAnchor:_headerView.topAnchor],
    [_headerView.heightAnchor constraintEqualToConstant:kHeaderHeight],
  ]];

  [self createCloseButton];

  [_headerView.contentView addSubview:_closeButton];
  [NSLayoutConstraint activateConstraints:@[
    [_closeButton.topAnchor
        constraintEqualToAnchor:_headerView.contentView.topAnchor
                       constant:kCloseButtonTopMargin],
    [_headerView.contentView.trailingAnchor
        constraintEqualToAnchor:_closeButton.trailingAnchor
                       constant:kCloseButtonTrailingMargin],
  ]];
}

#pragma mark - Public methods

- (void)setPanelBlocks:(NSArray<PanelBlockData*>*)panelBlocks {
  _panelBlocks = [panelBlocks copy];

  if (_diffableDataSource) {
    [_diffableDataSource applySnapshot:[self dataSnapshot]
                  animatingDifferences:NO];
  }
}

#pragma mark - Private

// Generates and returns a data source snapshot for the current data.
- (NSDiffableDataSourceSnapshot<NSString*, NSString*>*)dataSnapshot {
  NSDiffableDataSourceSnapshot<NSString*, NSString*>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[ kSectionIdentifier ]];
  NSMutableArray<NSString*>* itemIdentifiers = [[NSMutableArray alloc] init];
  for (PanelBlockData* data in _panelBlocks) {
    [itemIdentifiers addObject:data.blockType];
  }
  [snapshot appendItemsWithIdentifiers:itemIdentifiers];
  return snapshot;
}

// Target for the close button.
- (void)closeButtonTapped {
  [self.contextualSheetCommandHandler hideContextualSheet];
}

// Looks up the correct registration for the provided item. Wrapper for
// UICollectionViewDiffableDataSource's cellProvider parameter.
- (UICollectionViewCell*)
    diffableDataSourceCellProviderForCollectionView:
        (UICollectionView*)collectionView
                                          indexPath:(NSIndexPath*)indexPath
                                     itemIdentifier:(id)itemIdentifier {
  UICollectionViewCellRegistration* registration =
      [_panelBlocks[indexPath.row] cellRegistration];
  return [collectionView
      dequeueConfiguredReusableCellWithRegistration:registration
                                       forIndexPath:indexPath
                                               item:itemIdentifier];
}

#pragma mark - View Initialization

// Creates the layout for the collection view.
- (UICollectionViewLayout*)createLayout {
  NSCollectionLayoutSize* itemSize = [NSCollectionLayoutSize
      sizeWithWidthDimension:[NSCollectionLayoutDimension
                                 fractionalWidthDimension:1.]
             heightDimension:[NSCollectionLayoutDimension
                                 estimatedDimension:200]];
  NSCollectionLayoutItem* item =
      [NSCollectionLayoutItem itemWithLayoutSize:itemSize];

  NSCollectionLayoutSize* groupSize = [NSCollectionLayoutSize
      sizeWithWidthDimension:[NSCollectionLayoutDimension
                                 fractionalWidthDimension:1.]
             heightDimension:[NSCollectionLayoutDimension
                                 estimatedDimension:200]];
  NSCollectionLayoutGroup* group =
      [NSCollectionLayoutGroup verticalGroupWithLayoutSize:groupSize
                                                  subitems:@[ item ]];

  NSCollectionLayoutSection* section =
      [NSCollectionLayoutSection sectionWithGroup:group];
  return [[UICollectionViewCompositionalLayout alloc] initWithSection:section];
}

// Creates and initializes `_collectionView`.
- (void)createCollectionView {
  _collectionView =
      [[UICollectionView alloc] initWithFrame:CGRectZero
                         collectionViewLayout:[self createLayout]];
  _collectionView.translatesAutoresizingMaskIntoConstraints = NO;
  _collectionView.contentInset = UIEdgeInsetsMake(kHeaderHeight, 0, 0, 0);

  __weak __typeof(self) weakSelf = self;
  auto cellProvider =
      ^UICollectionViewCell*(UICollectionView* collectionView,
                             NSIndexPath* indexPath, id itemIdentifier) {
        return [weakSelf
            diffableDataSourceCellProviderForCollectionView:collectionView
                                                  indexPath:indexPath
                                             itemIdentifier:itemIdentifier];
      };

  _diffableDataSource = [[UICollectionViewDiffableDataSource alloc]
      initWithCollectionView:_collectionView
                cellProvider:cellProvider];

  _collectionView.dataSource = _diffableDataSource;

  [_diffableDataSource applySnapshot:[self dataSnapshot]
                animatingDifferences:NO];
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
