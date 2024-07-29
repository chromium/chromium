// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_discover_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_collection_configurator.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_header_view.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_link_cell.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"

@interface HomeCustomizationDiscoverViewController () <UICollectionViewDelegate>

// Contains the types of HomeCustomizationLinkCells that should be shown.
@property(nonatomic, assign) std::vector<CustomizationLinkType> linksVector;

@end

@implementation HomeCustomizationDiscoverViewController {
  // The collection view containing this menu page's content.
  UICollectionView* _collectionView;

  // The configurator for the collection view.
  HomeCustomizationCollectionConfigurator* _collectionConfigurator;

  // The diffable data source for the collection view.
  UICollectionViewDiffableDataSource<NSString*, NSNumber*>* _diffableDataSource;

  // Registration for the HomeCustomizationLinkCell.
  UICollectionViewCellRegistration* _linkCellRegistration;

  // Registration for the collection's header.
  UICollectionViewSupplementaryRegistration* _headerRegistration;
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

  _headerRegistration = [UICollectionViewSupplementaryRegistration
      registrationWithSupplementaryClass:[HomeCustomizationHeaderView class]
                             elementKind:UICollectionElementKindSectionHeader
                    configurationHandler:^(HomeCustomizationHeaderView* header,
                                           NSString* elementKind,
                                           NSIndexPath* indexPath) {
                      header.page = CustomizationMenuPage::kDiscover;
                    }];
}

// Creates and returns the collection view for the main menu page.
- (void)createCollectionView {
  _collectionConfigurator = [[HomeCustomizationCollectionConfigurator alloc]
      initWithPage:CustomizationMenuPage::kDiscover];

  _collectionView = [[UICollectionView alloc]
             initWithFrame:CGRectZero
      collectionViewLayout:[_collectionConfigurator collectionViewLayout]];
  _collectionView.translatesAutoresizingMaskIntoConstraints = NO;
  _collectionView.delegate = self;

  _diffableDataSource = [self createDiffableDataSource];
  _collectionConfigurator.diffableDataSource = _diffableDataSource;
  _collectionView.dataSource = _diffableDataSource;

  // Sets initial data.
  [_diffableDataSource applySnapshot:[self dataSnapshot]
                animatingDifferences:NO];
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
  diffableDataSource.supplementaryViewProvider = ^UICollectionReusableView*(
      UICollectionView* collectionView, NSString* elementKind,
      NSIndexPath* indexPath) {
    return [weakSelf configuredHeaderForIndexPath:indexPath];
  };

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

// Returns a configured header for the given path in the collection view.
- (UICollectionViewCell*)configuredHeaderForIndexPath:(NSIndexPath*)indexPath {
  return [_collectionView
      dequeueConfiguredReusableSupplementaryViewWithRegistration:
          _headerRegistration
                                                    forIndexPath:indexPath];
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
