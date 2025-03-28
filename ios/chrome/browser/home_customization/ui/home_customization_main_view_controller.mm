// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_main_view_controller.h"

#import <map>

#import "ios/chrome/browser/home_customization/ui/home_customization_background_view_cell.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_collection_configurator.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_mutator.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_toggle_cell.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_view_controller_protocol.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface HomeCustomizationMainViewController () <
    HomeCustomizationViewControllerProtocol,
    UICollectionViewDelegate>

// Contains the types of HomeCustomizationToggleCells that should be shown, with
// a BOOL indicating if each one is enabled.
@property(nonatomic, assign) std::map<CustomizationToggleType, BOOL> toggleMap;

@end

@implementation HomeCustomizationMainViewController {
  // The configurator for the collection view.
  HomeCustomizationCollectionConfigurator* _collectionConfigurator;

  // Registration for the HomeCustomizationToggleCells.
  UICollectionViewCellRegistration* _toggleCellRegistration;

  // Registration for the background customization cell.
  UICollectionViewCellRegistration* _backgroundCustomizationRegistration;
}

// Synthesized from HomeCustomizationViewControllerProtocol.
@synthesize collectionView = _collectionView;
@synthesize diffableDataSource = _diffableDataSource;
@synthesize page = _page;

- (void)viewDidLoad {
  [super viewDidLoad];

  _collectionConfigurator = [[HomeCustomizationCollectionConfigurator alloc]
      initWithViewController:self];
  _page = CustomizationMenuPage::kMain;

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

  if (IsNTPBackgroundCustomizationEnabled()) {
    _backgroundCustomizationRegistration = [UICollectionViewCellRegistration
        registrationWithCellClass:[HomeCustomizationBackgroundViewCell class]
             configurationHandler:^(HomeCustomizationBackgroundViewCell* cell,
                                    NSIndexPath* indexPath,
                                    NSNumber* itemIdentifier) {
               cell.mutator = weakSelf.mutator;
             }];
  }
}

// Creates a data snapshot representing the content of the collection view.
- (NSDiffableDataSourceSnapshot<CustomizationSection*, NSNumber*>*)
    dataSnapshot {
  NSDiffableDataSourceSnapshot<CustomizationSection*, NSNumber*>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];

  if (IsNTPBackgroundCustomizationEnabled()) {
    // Create background customization section and add items to it.
    [snapshot
        appendSectionsWithIdentifiers:@[ kCustomizationSectionBackground ]];

    NSInteger smallestExistingIdentifier =
        static_cast<NSInteger>(self.toggleMap.begin()->first);
    [snapshot appendItemsWithIdentifiers:@[ @(smallestExistingIdentifier - 1) ]
               intoSectionWithIdentifier:kCustomizationSectionBackground];
  }

  // Create toggles section and add items to it.
  [snapshot
      appendSectionsWithIdentifiers:@[ kCustomizationSectionMainToggles ]];
  [snapshot
      appendItemsWithIdentifiers:[self identifiersForToggleMap:self.toggleMap]
       intoSectionWithIdentifier:kCustomizationSectionMainToggles];

  return snapshot;
}

#pragma mark - HomeCustomizationViewControllerProtocol

- (void)dismissCustomizationMenuPage {
  [self.mutator dismissMenuPage];
}

- (NSCollectionLayoutSection*)
      sectionForIndex:(NSInteger)sectionIndex
    layoutEnvironment:(id<NSCollectionLayoutEnvironment>)layoutEnvironment {
  NSInteger mainTogglesIdentifier = [self.diffableDataSource.snapshot
      indexOfSectionIdentifier:kCustomizationSectionMainToggles];

  NSInteger backgroundCustomizationIdentifier =
      [self.diffableDataSource.snapshot
          indexOfSectionIdentifier:kCustomizationSectionBackground];

  if (sectionIndex == mainTogglesIdentifier ||
      sectionIndex == backgroundCustomizationIdentifier) {
    return [_collectionConfigurator
        verticalListSectionForLayoutEnvironment:layoutEnvironment];
  }
  return nil;
}

- (UICollectionViewCell*)configuredCellForIndexPath:(NSIndexPath*)indexPath
                                     itemIdentifier:(NSNumber*)itemIdentifier {
  CustomizationSection* section = [_diffableDataSource.snapshot
      sectionIdentifierForSectionContainingItemIdentifier:itemIdentifier];

  if (kCustomizationSectionMainToggles == section) {
    return [_collectionView
        dequeueConfiguredReusableCellWithRegistration:_toggleCellRegistration
                                         forIndexPath:indexPath
                                                 item:itemIdentifier];
  } else if (kCustomizationSectionBackground == section) {
    return [_collectionView
        dequeueConfiguredReusableCellWithRegistration:
            _backgroundCustomizationRegistration
                                         forIndexPath:indexPath
                                                 item:itemIdentifier];
  }
  return nil;
}

#pragma mark - HomeCustomizationMainConsumer

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
