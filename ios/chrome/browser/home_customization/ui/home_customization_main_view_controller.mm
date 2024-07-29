// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_main_view_controller.h"

#import <map>

#import "ios/chrome/browser/home_customization/ui/home_customization_collection_configurator.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_toggle_cell.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface HomeCustomizationMainViewController () <UICollectionViewDelegate>

// Contains the types of HomeCustomizationToggleCells that should be shown, with
// a BOOL indicating if each one is enabled.
@property(nonatomic, assign) std::map<CustomizationToggleType, BOOL> toggleMap;

@end

@implementation HomeCustomizationMainViewController {
  // The collection view containing this menu page's content.
  UICollectionView* _collectionView;

  // The configurator for the collection view.
  HomeCustomizationCollectionConfigurator* _collectionConfigurator;

  // The diffable data source for the collection view.
  UICollectionViewDiffableDataSource<CustomizationSection*, NSNumber*>*
      _diffableDataSource;

  // Registration for the HomeCustomizationToggleCells.
  UICollectionViewCellRegistration* _toggleCellRegistration;
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
  _toggleCellRegistration = [UICollectionViewCellRegistration
      registrationWithCellClass:[HomeCustomizationToggleCell class]
           configurationHandler:^(HomeCustomizationToggleCell* cell,
                                  NSIndexPath* indexPath,
                                  NSNumber* itemIdentifier) {
             CHECK(weakSelf.mutator);
             CustomizationToggleType toggleType =
                 (CustomizationToggleType)[itemIdentifier integerValue];
             BOOL enabled = self.toggleMap.at(toggleType);
             [cell configureCellWithType:toggleType enabled:enabled];
             cell.mutator = weakSelf.mutator;
           }];
}

// Creates and returns the collection view for the main menu page.
- (void)createCollectionView {
  _collectionConfigurator = [[HomeCustomizationCollectionConfigurator alloc]
      initWithPage:CustomizationMenuPage::kMain];

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

  return diffableDataSource;
}

// Returns a configured cell for the given path in the collection view.
- (UICollectionViewCell*)configuredCellForIndexPath:(NSIndexPath*)indexPath
                                     itemIdentifier:(NSNumber*)itemIdentifier {
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

// Creates a data snapshot representing the content of the collection view.
- (NSDiffableDataSourceSnapshot<CustomizationSection*, NSNumber*>*)
    dataSnapshot {
  NSDiffableDataSourceSnapshot<CustomizationSection*, NSNumber*>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];

  // Create toggles section and add items to it.
  [snapshot appendSectionsWithIdentifiers:@[ kCustomizationSectionToggles ]];
  [snapshot
      appendItemsWithIdentifiers:[self identifiersForToggleMap:self.toggleMap]
       intoSectionWithIdentifier:kCustomizationSectionToggles];

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
