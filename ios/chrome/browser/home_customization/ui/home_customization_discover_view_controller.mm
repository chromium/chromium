// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_discover_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_link_cell.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"

namespace {

// The dimensions of each link cell.
// TODO(crbug.com/350990359): Update this once we have the finalized specs.
const CGFloat kLinkCellHeight = 80;
const CGFloat kLinkCellWidth = 343;

// The vertical spacing between link cells.
const CGFloat kSpacingBetweenCells = 12;

}  // namespace

@interface HomeCustomizationDiscoverViewController () <UICollectionViewDelegate>

// Contains the types of HomeCustomizationLinkCells that should be shown.
@property(nonatomic, assign) std::vector<CustomizationLinkType> linksVector;

@end

@implementation HomeCustomizationDiscoverViewController {
  // The collection view containing this menu page's content.
  UICollectionView* _collectionView;

  // The diffable data source for the collection view.
  UICollectionViewDiffableDataSource<NSString*, NSNumber*>* _diffableDataSource;

  // Registration for the HomeCustomizationLinkCell.
  UICollectionViewCellRegistration* _linkCellRegistration;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  [self registerCells];
  [self createCollectionView];

  // The primary view is set as the collection view for better integration with
  // the UISheetPresentationController which presents it.
  self.view = _collectionView;

  [self configureNavigationBar];
}

#pragma mark - Private

// Registers the different cells used by the collection view.
- (void)registerCells {
  __weak __typeof(self) weakSelf = self;
  _linkCellRegistration = [UICollectionViewCellRegistration
      registrationWithCellClass:[HomeCustomizationLinkCell class]
           configurationHandler:^(HomeCustomizationLinkCell* cell,
                                  NSIndexPath* indexPath,
                                  NSNumber* itemIdentifier) {
             CustomizationLinkType linkType =
                 (CustomizationLinkType)[itemIdentifier integerValue];
             [cell configureCellWithType:linkType];
             cell.mutator = weakSelf.mutator;
           }];
}

// Creates and returns the collection view for the main menu page.
- (void)createCollectionView {
  _collectionView =
      [[UICollectionView alloc] initWithFrame:CGRectZero
                         collectionViewLayout:[self collectionViewLayout]];
  _collectionView.translatesAutoresizingMaskIntoConstraints = NO;
  _collectionView.delegate = self;

  _diffableDataSource = [self createDiffableDataSource];
  _collectionView.dataSource = _diffableDataSource;

  // Sets initial data.
  [_diffableDataSource applySnapshot:[self dataSnapshot]
                animatingDifferences:NO];
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
  if (sectionIndex ==
      [_diffableDataSource.snapshot
          indexOfSectionIdentifier:kCustomizationSectionDiscoverLinks]) {
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
                                   estimatedDimension:kLinkCellHeight]];
    NSCollectionLayoutGroup* linksGroup =
        [NSCollectionLayoutGroup verticalGroupWithLayoutSize:groupSize
                                                    subitems:@[ item ]];

    NSCollectionLayoutSection* linksSection =
        [NSCollectionLayoutSection sectionWithGroup:linksGroup];

    // Adds spacing between cells, as well as content insets so that the cells
    // have the correct width.
    linksSection.interGroupSpacing = kSpacingBetweenCells;
    linksSection.contentInsets = NSDirectionalEdgeInsetsMake(
        0, (self.view.frame.size.width - kLinkCellWidth) / 2, 0,
        (self.view.frame.size.width - kLinkCellWidth) / 2);

    return linksSection;
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
                             NSIndexPath* indexPath, NSNumber* itemIdentifier) {
        return [weakSelf configuredCellForIndexPath:indexPath
                                     itemIdentifier:itemIdentifier];
      };
  UICollectionViewDiffableDataSource* diffableDataSource =
      [[UICollectionViewDiffableDataSource alloc]
          initWithCollectionView:_collectionView
                    cellProvider:cellProvider];

  return diffableDataSource;
}

// Returns a configured cell for the given path in the collection view.
- (UICollectionViewCell*)configuredCellForIndexPath:(NSIndexPath*)indexPath
                                     itemIdentifier:(NSNumber*)itemIdentifier {
  return [_collectionView
      dequeueConfiguredReusableCellWithRegistration:_linkCellRegistration
                                       forIndexPath:indexPath
                                               item:itemIdentifier];
}

// Creates a data snapshot representing the content of the collection view.
- (NSDiffableDataSourceSnapshot<CustomizationSection*, NSNumber*>*)
    dataSnapshot {
  NSDiffableDataSourceSnapshot<CustomizationSection*, NSNumber*>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];

  // Create links section and add items to it.
  [snapshot
      appendSectionsWithIdentifiers:@[ kCustomizationSectionDiscoverLinks ]];
  [snapshot
      appendItemsWithIdentifiers:[self
                                     identifiersForLinksVector:self.linksVector]
       intoSectionWithIdentifier:kCustomizationSectionDiscoverLinks];

  return snapshot;
}

// Sets the title and button to the navigation bar on top of the presenting menu
// page.
- (void)configureNavigationBar {
  // TODO(crbug.com/350990359): Confirm page title.
  self.title = @"Discover";
  UIBarButtonItem* dismissButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemClose
                           target:self
                           action:@selector(dismissCustomizationMenu)];
  dismissButton.accessibilityIdentifier = kNavigationBarDismissButtonIdentifier;
  self.navigationItem.rightBarButtonItem = dismissButton;
  self.navigationItem.leftBarButtonItem.accessibilityIdentifier =
      kNavigationBarBackButtonIdentifier;
}

// Dismisses the presenting view controller.
- (void)dismissCustomizationMenu {
  [self.presentingViewController dismissViewControllerAnimated:YES
                                                    completion:nil];
}

#pragma mark - HomeCustomizationDiscoverConsumer

- (void)populateDiscoverLinks:(std::vector<CustomizationLinkType>)linksVector {
  _linksVector = linksVector;

  // Recreate the snapshot with the new items to take into account all the
  // changes of items presence (add/remove).
  NSDiffableDataSourceSnapshot<CustomizationSection*, NSNumber*>* snapshot =
      [self dataSnapshot];

  // Reconfigure all present items to ensure that they are updated in case
  // their content changed.
  [snapshot reconfigureItemsWithIdentifiers:
                [self identifiersForLinksVector:linksVector]];

  [_diffableDataSource applySnapshot:snapshot animatingDifferences:YES];
}

#pragma mark - Helpers

// Returns an array of identifiers for a vector of link types, which can be
// used by the snapshot.
- (NSMutableArray<NSNumber*>*)identifiersForLinksVector:
    (std::vector<CustomizationLinkType>)types {
  NSMutableArray<NSNumber*>* linkDataIdentifiers =
      [[NSMutableArray alloc] init];
  for (auto const& type : types) {
    [linkDataIdentifiers addObject:@((int)type)];
  }
  return linkDataIdentifiers;
}

@end
