// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_main_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/home_customization/model/background_customization_configuration.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_cell.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_picker_cell.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_collection_configurator.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_logo_vendor_provider.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_mutator.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_toggle_cell.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_view_controller_protocol.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/logo_vendor.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/ui_utils/ui_utils_api.h"
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

  // Registration for the background cell.
  UICollectionViewCellRegistration* _backgroundCellRegistration;

  // Registration for the background picker cell.
  UICollectionViewCellRegistration* _backgroundPickerCellRegistration;

  // Contains the options the HomeCustomizationBackgroundCell will use to set a
  // background on the NTP.
  NSMutableDictionary<NSString*, BackgroundCustomizationConfiguration*>*
      _backgroundCustomizationConfigurationMap;

  // The id of the selected background cell.
  NSString* _selectedBackgroundId;
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
                                  NSString* itemIdentifier) {
             CustomizationToggleType toggleType =
                 (CustomizationToggleType)[itemIdentifier integerValue];
             BOOL enabled = weakSelf.toggleMap.at(toggleType);
             [cell configureCellWithType:toggleType enabled:enabled];
             cell.mutator = weakSelf.mutator;
           }];

  if (IsNTPBackgroundCustomizationEnabled()) {
    _backgroundCellRegistration = [UICollectionViewCellRegistration
        registrationWithCellClass:[HomeCustomizationBackgroundCell class]
             configurationHandler:^(HomeCustomizationBackgroundCell* cell,
                                    NSIndexPath* indexPath,
                                    NSString* itemIdentifier) {
               [weakSelf configureBackgroundCell:cell
                                     atIndexPath:indexPath
                              withItemIdentifier:itemIdentifier];
             }];

    _backgroundPickerCellRegistration = [UICollectionViewCellRegistration
        registrationWithCellClass:[HomeCustomizationBackgroundPickerCell class]
             configurationHandler:^(HomeCustomizationBackgroundPickerCell* cell,
                                    NSIndexPath* indexPath,
                                    NSString* itemIdentifier) {
               cell.mutator = weakSelf.mutator;
               cell.delegate = weakSelf.backgroundPickerPresentationDelegate;
             }];
  }
}

// Creates a data snapshot representing the content of the collection view.
- (NSDiffableDataSourceSnapshot<CustomizationSection*, NSString*>*)
    dataSnapshot {
  NSDiffableDataSourceSnapshot<CustomizationSection*, NSString*>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];

  if (IsNTPBackgroundCustomizationEnabled()) {
    // Create background customization section and add items to it.
    [snapshot
        appendSectionsWithIdentifiers:@[ kCustomizationSectionBackground ]];
    [snapshot appendItemsWithIdentifiers:
                  [self identifiersForBackgroundCells:
                            _backgroundCustomizationConfigurationMap]
               intoSectionWithIdentifier:kCustomizationSectionBackground];
    [snapshot appendItemsWithIdentifiers:@[ kBackgroundPickerCellIdentifier ]
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

  if (sectionIndex == mainTogglesIdentifier) {
    return [_collectionConfigurator
        verticalListSectionForLayoutEnvironment:layoutEnvironment];
  } else if (sectionIndex == backgroundCustomizationIdentifier) {
    CHECK(IsNTPBackgroundCustomizationEnabled());
    return [_collectionConfigurator
        backgroundCellSectionForLayoutEnvironment:layoutEnvironment];
  }
  return nil;
}

- (UICollectionViewCell*)configuredCellForIndexPath:(NSIndexPath*)indexPath
                                     itemIdentifier:(NSString*)itemIdentifier {
  CustomizationSection* section = [_diffableDataSource.snapshot
      sectionIdentifierForSectionContainingItemIdentifier:itemIdentifier];

  if (kCustomizationSectionMainToggles == section) {
    return [_collectionView
        dequeueConfiguredReusableCellWithRegistration:_toggleCellRegistration
                                         forIndexPath:indexPath
                                                 item:itemIdentifier];
  } else if (kCustomizationSectionBackground == section) {
    if ([itemIdentifier isEqual:kBackgroundPickerCellIdentifier]) {
      return [_collectionView
          dequeueConfiguredReusableCellWithRegistration:
              _backgroundPickerCellRegistration
                                           forIndexPath:indexPath
                                                   item:itemIdentifier];
    } else if ([itemIdentifier hasPrefix:kBackgroundCellIdentifier]) {
      return [_collectionView
          dequeueConfiguredReusableCellWithRegistration:
              _backgroundCellRegistration
                                           forIndexPath:indexPath
                                                   item:itemIdentifier];
    }
  }
  return nil;
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  CustomizationSection* section =
      [self.diffableDataSource snapshot].sectionIdentifiers[indexPath.section];
  NSString* itemIdentifier =
      [self.diffableDataSource itemIdentifierForIndexPath:indexPath];

  return [section isEqualToString:kCustomizationSectionBackground] &&
         ![itemIdentifier isEqualToString:kBackgroundPickerCellIdentifier];
}

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  NSString* itemIdentifier =
      [self.diffableDataSource itemIdentifierForIndexPath:indexPath];

  BackgroundCustomizationConfiguration* backgroundConfiguration =
      _backgroundCustomizationConfigurationMap[itemIdentifier];

  [self.mutator applyBackgroundForConfiguration:backgroundConfiguration];
}

- (void)collectionView:(UICollectionView*)collectionView
       willDisplayCell:(UICollectionViewCell*)cell
    forItemAtIndexPath:(NSIndexPath*)indexPath {
  CustomizationSection* section =
      [self.diffableDataSource snapshot].sectionIdentifiers[indexPath.section];
  NSString* itemIdentifier =
      [_diffableDataSource itemIdentifierForIndexPath:indexPath];
  if (![section isEqualToString:kCustomizationSectionBackground] ||
      ![cell isKindOfClass:[HomeCustomizationBackgroundCell class]]) {
    return;
  }

  BackgroundCustomizationConfiguration* backgroundConfiguration =
      _backgroundCustomizationConfigurationMap[itemIdentifier];

  if (backgroundConfiguration &&
      !backgroundConfiguration.thumbnailURL.is_empty()) {
    [self.mutator
        fetchBackgroundCustomizationThumbnailURLImage:backgroundConfiguration
                                                          .thumbnailURL
                                           completion:^(UIImage* image) {
                                             [(HomeCustomizationBackgroundCell*)
                                                     cell
                                                 updateBackgroundImage:image];
                                           }];
  }
}

#pragma mark - HomeCustomizationMainConsumer

- (void)populateToggles:(std::map<CustomizationToggleType, BOOL>)toggleMap {
  _toggleMap = toggleMap;

  // Recreate the snapshot with the new items to take into account all the
  // changes of items presence (add/remove).
  NSDiffableDataSourceSnapshot<CustomizationSection*, NSString*>* snapshot =
      [self dataSnapshot];

  // Reconfigure all present items to ensure that they are updated in case their
  // content changed.
  [snapshot
      reconfigureItemsWithIdentifiers:[self identifiersForToggleMap:toggleMap]];

  [_diffableDataSource applySnapshot:snapshot animatingDifferences:YES];
}

- (void)
    populateBackgroundCustomizationConfigurations:
        (NSMutableDictionary<NSString*, BackgroundCustomizationConfiguration*>*)
            backgroundCustomizationConfigurationMap
                             selectedBackgroundId:
                                 (NSString*)selectedBackgroundId {
  _backgroundCustomizationConfigurationMap =
      backgroundCustomizationConfigurationMap;
  _selectedBackgroundId = selectedBackgroundId;

  // Recreate the snapshot with the new items to take into account all the
  // changes of items presence (add/remove).
  NSDiffableDataSourceSnapshot<CustomizationSection*, NSString*>* snapshot =
      [self dataSnapshot];

  // Reconfigure all present items to ensure that they are updated in case their
  // content changed.
  [snapshot reconfigureItemsWithIdentifiers:
                [self identifiersForBackgroundCells:
                          _backgroundCustomizationConfigurationMap]];

  [_diffableDataSource applySnapshot:snapshot animatingDifferences:YES];
}

#pragma mark - Helpers

// Returns an array of identifiers for the background options, which can be used
// by the snapshot.
- (NSArray<NSString*>*)identifiersForBackgroundCells:
    (NSMutableDictionary<NSString*, BackgroundCustomizationConfiguration*>*)
        backgroundCustomizationConfigurationMap {
  NSMutableArray<NSString*>* identifiers = [[NSMutableArray alloc] init];
  for (NSString* key in backgroundCustomizationConfigurationMap) {
    [identifiers addObject:key];
  }

  return [identifiers copy];
}

// Returns an array of identifiers for a map of toggle types, which can be
// used by the snapshot.
- (NSMutableArray<NSString*>*)identifiersForToggleMap:
    (std::map<CustomizationToggleType, BOOL>)types {
  NSMutableArray<NSString*>* toggleDataIdentifiers =
      [[NSMutableArray alloc] init];
  for (auto const& [key, value] : types) {
    [toggleDataIdentifiers addObject:[@((int)key) stringValue]];
  }
  return toggleDataIdentifiers;
}

#pragma mark - Private

// Configures a `HomeCustomizationBackgroundCell` with the provided background
// configuration and logo view, and selects it if it matches the currently
// selected background ID.
- (void)configureBackgroundCell:(HomeCustomizationBackgroundCell*)cell
                    atIndexPath:(NSIndexPath*)indexPath
             withItemIdentifier:(NSString*)itemIdentifier {
  BackgroundCustomizationConfiguration* backgroundConfiguration =
      _backgroundCustomizationConfigurationMap[itemIdentifier];
  id<LogoVendor> logoVendor = [self.logoVendorProvider provideLogoVendor];

  [cell configureWithBackgroundOption:backgroundConfiguration
                           logoVendor:logoVendor];

  if ([itemIdentifier isEqualToString:_selectedBackgroundId]) {
    [self.collectionView
        selectItemAtIndexPath:indexPath
                     animated:NO
               scrollPosition:UICollectionViewScrollPositionNone];
  }
  cell.mutator = self.mutator;
}

@end
