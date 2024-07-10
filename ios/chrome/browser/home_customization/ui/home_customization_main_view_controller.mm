// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_main_view_controller.h"

#import "ios/chrome/browser/home_customization/ui/home_customization_toggle_cell.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The height of a toggle cell.
// TODO(crbug.com/350990359): Update this once we have the finalized specs.
const CGFloat kToggleCellHeight = 80;

}  // namespace

@interface HomeCustomizationMainViewController () <UICollectionViewDelegate>

@end

@implementation HomeCustomizationMainViewController {
  // The collection view containing this menu page's content.
  UICollectionView* _collectionView;

  // The diffable data source for the collection view.
  UICollectionViewDiffableDataSource<CustomizationSection*, NSString*>*
      _diffableDataSource;

  // Registration for the HomeCustomizationToggleCells.
  UICollectionViewCellRegistration* _toggleCellRegistration;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // The primary view is set as the collection view for better integration with
  // the UISheetPresentationController which presents it.
  [self createCollectionView];
  self.view = _collectionView;

  [self configureNavigationBar];
}

#pragma mark - Private

// Creates and returns the collection view for the main menu page.
- (void)createCollectionView {
  _collectionView =
      [[UICollectionView alloc] initWithFrame:CGRectZero
                         collectionViewLayout:[self collectionViewLayout]];
  _collectionView.translatesAutoresizingMaskIntoConstraints = NO;
  _collectionView.delegate = self;
  _collectionView.dataSource = [self createDiffableDataSource];

  _toggleCellRegistration = [UICollectionViewCellRegistration
      registrationWithCellClass:[HomeCustomizationToggleCell class]
           configurationHandler:^(HomeCustomizationToggleCell* cell,
                                  NSIndexPath* indexPath,
                                  NSString* itemIdentifier){
               // TODO(crbug.com/350990359): Configure cell.
           }];
}

// Defines the layout for the collection view.
- (UICollectionViewLayout*)collectionViewLayout {
  UICollectionViewCompositionalLayoutConfiguration* configuration =
      [[UICollectionViewCompositionalLayoutConfiguration alloc] init];
  __weak __typeof(self) weakSelf = self;
  return [[UICollectionViewCompositionalLayout alloc]
      initWithSectionProvider:^(
          NSInteger sectionIndex,
          id<NSCollectionLayoutEnvironment> layoutEnvironment) {
        return [weakSelf sectionForIndex:sectionIndex];
      }
                configuration:configuration];
}

// Returns the section for a given `sectionIndex`.
- (NSCollectionLayoutSection*)sectionForIndex:(NSInteger)sectionIndex {
  // Toggles section.
  if (sectionIndex ==
      [_diffableDataSource.snapshot
          indexOfSectionIdentifier:kCustomizationSectionToggles]) {
    NSCollectionLayoutSize* itemSize = [NSCollectionLayoutSize
        sizeWithWidthDimension:[NSCollectionLayoutDimension
                                   fractionalWidthDimension:1.]
               heightDimension:[NSCollectionLayoutDimension
                                   fractionalHeightDimension:1.]];
    NSCollectionLayoutItem* item =
        [NSCollectionLayoutItem itemWithLayoutSize:itemSize];

    NSCollectionLayoutSize* groupSize = [NSCollectionLayoutSize
        sizeWithWidthDimension:[NSCollectionLayoutDimension
                                   fractionalWidthDimension:1.]
               heightDimension:[NSCollectionLayoutDimension
                                   estimatedDimension:kToggleCellHeight]];
    NSCollectionLayoutGroup* togglesGroup =
        [NSCollectionLayoutGroup verticalGroupWithLayoutSize:groupSize
                                                    subitems:@[ item ]];

    NSCollectionLayoutSection* togglesSection =
        [NSCollectionLayoutSection sectionWithGroup:togglesGroup];
    return togglesSection;
  }
  return nil;
}

// Creates and returns the diffable data source for the collection view.
- (UICollectionViewDiffableDataSource*)createDiffableDataSource {
  // Creates the diffable data source with a cell provider used to configure
  // each cell.
  __weak __typeof(self) weakSelf = self;
  auto cellProvider =
      ^UICollectionViewCell*(UICollectionView* collectionView,
                             NSIndexPath* indexPath, NSString* itemIdentifier) {
        return [weakSelf configuredCellForIndexPath:indexPath
                                     itemIdentifier:itemIdentifier];
      };
  UICollectionViewDiffableDataSource* diffableDataSource =
      [[UICollectionViewDiffableDataSource alloc]
          initWithCollectionView:_collectionView
                    cellProvider:cellProvider];

  // Sets initial data.
  [diffableDataSource applySnapshot:[self initialDataSnapshot]
               animatingDifferences:NO];

  return diffableDataSource;
}

// Returns a configured cell for the given path in the collection view.
- (UICollectionViewCell*)configuredCellForIndexPath:(NSIndexPath*)indexPath
                                     itemIdentifier:(NSString*)itemIdentifier {
  if (kCustomizationSectionToggles ==
      [_diffableDataSource.snapshot
          sectionIdentifierForSectionContainingItemIdentifier:itemIdentifier]) {
    return [_collectionView
        dequeueConfiguredReusableCellWithRegistration:_toggleCellRegistration
                                         forIndexPath:indexPath
                                                 item:itemIdentifier];
  }
  return nil;
}

// Creates a data snapshot representing the initial content of the collection
// view.
- (NSDiffableDataSourceSnapshot<CustomizationSection*, NSString*>*)
    initialDataSnapshot {
  NSDiffableDataSourceSnapshot<CustomizationSection*, NSString*>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[ kCustomizationSectionToggles ]];
  return snapshot;
}

// Sets the title and button to the navigation bar on top of the presenting menu
// page.
- (void)configureNavigationBar {
  self.title = l10n_util::GetNSString(
      IDS_IOS_HOME_CUSTOMIZATION_MAIN_PAGE_NAVIGATION_TITLE);
  UIBarButtonItem* dismissButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemClose
                           target:self
                           action:@selector(dismissCustomizationMenu)];
  dismissButton.accessibilityIdentifier = kNavigationBarDismissButtonIdentifier;
  self.navigationItem.rightBarButtonItem = dismissButton;
}

// Dismisses the presenting view controller.
- (void)dismissCustomizationMenu {
  [self.presentingViewController dismissViewControllerAnimated:YES
                                                    completion:nil];
}

@end
