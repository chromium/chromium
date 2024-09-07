// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_magic_stack_view_controller.h"

#import <map>

#import "ios/chrome/browser/home_customization/ui/home_customization_collection_configurator.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_header_view.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_mutator.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_toggle_cell.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_view_controller_protocol.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"

@interface HomeCustomizationMagicStackViewController () <
    HomeCustomizationViewControllerProtocol,
    UICollectionViewDelegate>

// Contains the types of HomeCustomizationToggleCells that should be shown, with
// a BOOL indicating if each one is enabled.
@property(nonatomic, assign) std::map<CustomizationToggleType, BOOL> toggleMap;

@end

@implementation HomeCustomizationMagicStackViewController {
  // The configurator for the collection view.
  HomeCustomizationCollectionConfigurator* _collectionConfigurator;

  // Registration for the HomeCustomizationToggleCells.
  UICollectionViewCellRegistration* _toggleCellRegistration;

  // Registration for the collection's header.
  UICollectionViewSupplementaryRegistration* _headerRegistration;
}

// Synthesized from HomeCustomizationViewControllerProtocol.
@synthesize collectionView = _collectionView;
@synthesize diffableDataSource = _diffableDataSource;
@synthesize page = _page;

- (void)viewDidLoad {
  [super viewDidLoad];

  _collectionConfigurator = [[HomeCustomizationCollectionConfigurator alloc]
      initWithViewController:self];
  _page = CustomizationMenuPage::kMagicStack;

  [self registerCells];
  [_collectionConfigurator configureCollectionView];

  // Sets initial data.
  [_diffableDataSource applySnapshot:[self dataSnapshot]
                animatingDifferences:NO];

  // The primary view is set as the collection view for better integration with
  // the UISheetPresentationController which presents it.
  self.view = _collectionView;

  [_collectionConfigurator configureNavigationBar];
}

#pragma mark - Private

// Registers the different cells used by the collection view.
- (void)registerCells {
  __weak __typeof(self) weakSelf = self;
  _toggleCellRegistration = [UICollectionViewCellRegistration
      registrationWithCellClass:[HomeCustomizationToggleCell class]
           configurationHandler:^(HomeCustomizationToggleCell* cell,
                                  NSIndexPath* indexPath,
                                  NSNumber* itemIdentifier) {
             CustomizationToggleType toggleType =
                 (CustomizationToggleType)[itemIdentifier integerValue];
             BOOL enabled = self.toggleMap.at(toggleType);
             [cell configureCellWithType:toggleType enabled:enabled];
             cell.mutator = weakSelf.mutator;
           }];
  _headerRegistration = [UICollectionViewSupplementaryRegistration
      registrationWithSupplementaryClass:[HomeCustomizationHeaderView class]
                             elementKind:UICollectionElementKindSectionHeader
                    configurationHandler:^(HomeCustomizationHeaderView* header,
                                           NSString* elementKind,
                                           NSIndexPath* indexPath) {
                      header.page = CustomizationMenuPage::kMagicStack;
                    }];
}

// Creates a data snapshot representing the content of the collection view.
- (NSDiffableDataSourceSnapshot<CustomizationSection*, NSNumber*>*)
    dataSnapshot {
  NSDiffableDataSourceSnapshot<CustomizationSection*, NSNumber*>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];

  // Create toggles section and add items to it.
  [snapshot appendSectionsWithIdentifiers:@[
    kCustomizationSectionMagicStackToggles
  ]];
  [snapshot
      appendItemsWithIdentifiers:[self identifiersForToggleMap:self.toggleMap]
       intoSectionWithIdentifier:kCustomizationSectionMagicStackToggles];

  return snapshot;
}

#pragma mark - HomeCustomizationViewControllerProtocol

- (void)dismissCustomizationMenuPage {
  [self.mutator dismissMenuPage];
}

- (NSCollectionLayoutSection*)
      sectionForIndex:(NSInteger)sectionIndex
    layoutEnvironment:(id<NSCollectionLayoutEnvironment>)layoutEnvironment {
  if (sectionIndex ==
      [self.diffableDataSource.snapshot
          indexOfSectionIdentifier:kCustomizationSectionMagicStackToggles]) {
    return [_collectionConfigurator
        verticalListSectionForLayoutEnvironment:layoutEnvironment];
  }
  return nil;
}

- (UICollectionViewCell*)configuredCellForIndexPath:(NSIndexPath*)indexPath
                                     itemIdentifier:(NSNumber*)itemIdentifier {
  return [_collectionView
      dequeueConfiguredReusableCellWithRegistration:_toggleCellRegistration
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

#pragma mark - HomeCustomizationMagicStackConsumer

- (void)populateToggles:(std::map<CustomizationToggleType, BOOL>)toggleMap {
  _toggleMap = toggleMap;

  // Recreate the snapshot with the new items to take into account all the
  // changes of items presence (add/remove).
  NSDiffableDataSourceSnapshot<CustomizationSection*, NSNumber*>* snapshot =
      [self dataSnapshot];

  // Reconfigure all present items to ensure that they are updated in case their
  // content changed.
  [snapshot
      reconfigureItemsWithIdentifiers:[self identifiersForToggleMap:toggleMap]];

  [_diffableDataSource applySnapshot:snapshot animatingDifferences:YES];
}

#pragma mark - Helpers

// Returns an array of identifiers for a map of toggle types, which can be
// used by the snapshot.
- (NSMutableArray<NSNumber*>*)identifiersForToggleMap:
    (std::map<CustomizationToggleType, BOOL>)types {
  NSMutableArray<NSNumber*>* toggleDataIdentifiers =
      [[NSMutableArray alloc] init];
  for (auto const& [key, value] : types) {
    [toggleDataIdentifiers addObject:@((int)key)];
  }
  return toggleDataIdentifiers;
}

@end
