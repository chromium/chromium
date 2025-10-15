// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_main_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/home_customization/ui/background_collection_configuration.h"
#import "ios/chrome/browser/home_customization/ui/background_customization_configuration.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_accessibility_identifiers.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_cell.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_configuration_mutator.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_picker_cell.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_collection_configurator.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_enterprise_policy_cell.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_framing_coordinates.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_mutator.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_search_engine_logo_mediator_provider.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_toggle_cell.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_view_controller_protocol.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_image_background_trait.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_trait.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/custom_ui_trait_accessor.h"
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

  // Registration for the background cell.
  UICollectionViewCellRegistration* _backgroundCellRegistration;

  // Registration for the background picker cell.
  UICollectionViewCellRegistration* _backgroundPickerCellRegistration;

  // Collection of backgrounds to display in the collection view.
  BackgroundCollectionConfiguration* _backgroundCollectionConfiguration;

  // Registration for the enterprise management info cell.
  UICollectionViewCellRegistration* _enterprisePolicyCellRegistration;

  // The id of the selected background cell.
  NSString* _selectedBackgroundId;

  // The number of times a background is selected from the recently used
  // section.
  int _recentBackgroundClickCount;
}

// Synthesized from HomeCustomizationViewControllerProtocol.
@synthesize collectionView = _collectionView;
@synthesize diffableDataSource = _diffableDataSource;
@synthesize page = _page;
@synthesize additionalViewWillTransitionToSizeHandler =
    _additionalViewWillTransitionToSizeHandler;

- (instancetype)init {
  self = [super init];
  if (self) {
    self.backgroundCustomizationUserInteractionEnabled = YES;
  }
  return self;
}

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

  _collectionView.accessibilityIdentifier =
      kHomeCustomizationMainViewAccessibilityIdentifier;

  [_collectionConfigurator configureNavigationBar];
}

- (void)viewWillDisappear:(BOOL)animated {
  base::UmaHistogramCounts10000(
      "IOS.HomeCustomization.Background.RecentlyUsed.ClickCount",
      _recentBackgroundClickCount);
  [super viewWillDisappear:animated];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  if (_additionalViewWillTransitionToSizeHandler) {
    _additionalViewWillTransitionToSizeHandler(size, coordinator);
  }
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

  if (IsNTPBackgroundCustomizationEnabled() &&
      !self.customizationDisabledByPolicy) {
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
               cell.accessibilityLabel = l10n_util::GetNSString(
                   IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_PICKER_ACCESSIBILITY_LABEL);
             }];
  }

  if (IsNTPBackgroundCustomizationEnabled() &&
      self.customizationDisabledByPolicy) {
    _enterprisePolicyCellRegistration = [UICollectionViewCellRegistration
        registrationWithCellClass:[HomeCustomizationEnterprisePolicyCell class]
             configurationHandler:^(HomeCustomizationEnterprisePolicyCell* cell,
                                    NSIndexPath* indexPath,
                                    NSString* itemIdentifier) {
               [cell configureCellWithMutator:weakSelf.mutator];
             }];
  }
}

// Creates a data snapshot representing the content of the collection view.
- (NSDiffableDataSourceSnapshot<CustomizationSection*, NSString*>*)
    dataSnapshot {
  NSDiffableDataSourceSnapshot<CustomizationSection*, NSString*>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];

  if (IsNTPBackgroundCustomizationEnabled() &&
      !self.customizationDisabledByPolicy) {
    // Create background customization section and add items to it.
    [snapshot
        appendSectionsWithIdentifiers:@[ kCustomizationSectionBackground ]];
    [snapshot appendItemsWithIdentifiers:[self identifiersForBackgroundCells]
               intoSectionWithIdentifier:kCustomizationSectionBackground];
  }

  // Create toggles section and add items to it.
  [snapshot
      appendSectionsWithIdentifiers:@[ kCustomizationSectionMainToggles ]];
  [snapshot
      appendItemsWithIdentifiers:[self identifiersForToggleMap:self.toggleMap]
       intoSectionWithIdentifier:kCustomizationSectionMainToggles];

  if (IsNTPBackgroundCustomizationEnabled() &&
      self.customizationDisabledByPolicy) {
    // Create an enterprise section with a message to users.
    [snapshot
        appendSectionsWithIdentifiers:@[ kCustomizationSectionEnterprise ]];
    [snapshot appendItemsWithIdentifiers:@[ kEnterpriseCellIdentifier ]
               intoSectionWithIdentifier:kCustomizationSectionEnterprise];
  }

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

  NSInteger enterpriseIdentifier = [self.diffableDataSource.snapshot
      indexOfSectionIdentifier:kCustomizationSectionEnterprise];

  if (sectionIndex == mainTogglesIdentifier) {
    return [_collectionConfigurator
        verticalListSectionForLayoutEnvironment:layoutEnvironment];
  } else if (sectionIndex == backgroundCustomizationIdentifier) {
    CHECK(IsNTPBackgroundCustomizationEnabled());
    CGSize windowSize = self.view.window.bounds.size;
    return [_collectionConfigurator
        backgroundCellSectionForLayoutEnvironment:layoutEnvironment
                                       windowSize:windowSize];
  } else if (sectionIndex == enterpriseIdentifier) {
    return [_collectionConfigurator
        verticalListSectionForLayoutEnvironment:layoutEnvironment];
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
  } else if (kCustomizationSectionEnterprise == section) {
    return [_collectionView
        dequeueConfiguredReusableCellWithRegistration:
            _enterprisePolicyCellRegistration
                                         forIndexPath:indexPath
                                                 item:itemIdentifier];
  }
  return nil;
}

- (UIContextMenuConfiguration*)collectionView:(UICollectionView*)collectionView
    contextMenuConfigurationForItemAtIndexPath:(NSIndexPath*)indexPath
                                         point:(CGPoint)point {
  CustomizationSection* section =
      [self.diffableDataSource snapshot].sectionIdentifiers[indexPath.section];
  NSString* itemIdentifier =
      [self.diffableDataSource itemIdentifierForIndexPath:indexPath];

  if (![section isEqualToString:kCustomizationSectionBackground] ||
      ![itemIdentifier hasPrefix:kBackgroundCellIdentifier]) {
    return nil;
  }

  id<BackgroundCustomizationConfiguration> configuration =
      _backgroundCollectionConfiguration.configurations[itemIdentifier];
  // Don't allow deletion for the default entry.
  if (configuration.backgroundStyle ==
      HomeCustomizationBackgroundStyle::kDefault) {
    return [UIContextMenuConfiguration configurationWithIdentifier:nil
                                                   previewProvider:nil
                                                    actionProvider:nil];
  }

  __weak __typeof(self) weakSelf = self;

  // The currently selected background should have the menu item visible but
  // disabled.
  UIMenuElementAttributes actionAttributes =
      ([collectionView.indexPathsForSelectedItems containsObject:indexPath])
          ? UIMenuElementAttributesDisabled
          : UIMenuElementAttributesDestructive;

  return [UIContextMenuConfiguration
      configurationWithIdentifier:indexPath
                  previewProvider:nil
                   actionProvider:^UIMenu*(
                       NSArray<UIMenuElement*>* suggestedActions) {
                     UIAction* deleteAction = [UIAction
                         actionWithTitle:
                             l10n_util::GetNSString(
                                 IDS_IOS_HOME_CUSTOMIZATION_CONTEXT_MENU_DELETE_RECENT_BACKGROUND_TITLE)
                                   image:DefaultSymbolWithPointSize(
                                             kTrashSymbol,
                                             [[UIFont preferredFontForTextStyle:
                                                          UIFontTextStyleBody]
                                                 pointSize])
                              identifier:nil
                                 handler:^(UIAction* action) {
                                   [weakSelf
                                       handleDeleteBackgroundActionAtIndexPath:
                                           indexPath];
                                 }];
                     deleteAction.attributes = actionAttributes;

                     return [UIMenu menuWithTitle:@""
                                         children:@[ deleteAction ]];
                   }];
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  CustomizationSection* section =
      [self.diffableDataSource snapshot].sectionIdentifiers[indexPath.section];
  NSString* itemIdentifier =
      [self.diffableDataSource itemIdentifierForIndexPath:indexPath];

  return [section isEqualToString:kCustomizationSectionBackground] &&
         self.backgroundCustomizationUserInteractionEnabled &&
         ![itemIdentifier isEqualToString:kBackgroundPickerCellIdentifier];
}

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  NSString* itemIdentifier =
      [self.diffableDataSource itemIdentifierForIndexPath:indexPath];

  // Prevent background updates when a user clicks on an already selected cell.
  if (_selectedBackgroundId == itemIdentifier) {
    return;
  }

  _selectedBackgroundId = itemIdentifier;

  id<BackgroundCustomizationConfiguration> backgroundConfiguration =
      _backgroundCollectionConfiguration.configurations[itemIdentifier];

  [self.customizationMutator
      applyBackgroundForConfiguration:backgroundConfiguration];

  _recentBackgroundClickCount += 1;

  if (backgroundConfiguration.backgroundStyle ==
      HomeCustomizationBackgroundStyle::kDefault) {
    base::RecordAction(base::UserMetricsAction(
        "IOS.HomeCustomization.Background.ResetDefault.Tapped"));
    return;
  }

  base::RecordAction(base::UserMetricsAction(
      "IOS.HomeCustomization.Background.RecentlyUsed.Tapped"));
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

  id<BackgroundCustomizationConfiguration> backgroundConfiguration =
      _backgroundCollectionConfiguration.configurations[itemIdentifier];

  if (!backgroundConfiguration) {
    return;
  }

  HomeCustomizationBackgroundCell* backgroundCell =
      base::apple::ObjCCast<HomeCustomizationBackgroundCell>(cell);

  if (!cell) {
    return;
  }

  if (backgroundConfiguration.backgroundStyle ==
      HomeCustomizationBackgroundStyle::kPreset) {
    void (^imageHandler)(UIImage*, NSError*) =
        ^(UIImage* image, NSError* error) {
          if (!error) {
            // TODO(crbug.com/444505682): Handle error loading thumbnail image.
          }
          [backgroundCell updateBackgroundImage:image framingCoordinates:nil];
        };
    [self.customizationMutator
        fetchBackgroundCustomizationThumbnailURLImage:backgroundConfiguration
                                                          .thumbnailURL
                                           completion:imageHandler];
  } else if (backgroundConfiguration.backgroundStyle ==
             HomeCustomizationBackgroundStyle::kUserUploaded) {
    HomeCustomizationFramingCoordinates* framingCoordinates =
        backgroundConfiguration.userUploadedFramingCoordinates;
    __weak __typeof(self) weakSelf = self;
    void (^imageHandler)(UIImage*, UserUploadedImageError) = ^(
        UIImage* image, UserUploadedImageError error) {
      [weakSelf handleLoadedUserUploadedImage:image
                           framingCoordinates:framingCoordinates
                               backgroundCell:backgroundCell];
      if (!image) {
        base::UmaHistogramEnumeration(
            "IOS.HomeCustomization.Background.RecentlyUsed."
            "ImageUserUploadedFetchError",
            error);
      }
    };
    [self.customizationMutator
        fetchBackgroundCustomizationUserUploadedImage:backgroundConfiguration
                                                          .userUploadedImagePath
                                           completion:imageHandler];
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

#pragma mark - HomeCustomizationBackgroundConfigurationConsumer

- (void)setBackgroundCollectionConfigurations:
            (NSArray<BackgroundCollectionConfiguration*>*)
                backgroundCollectionConfigurations
                         selectedBackgroundId:(NSString*)selectedBackgroundId {
  CHECK(backgroundCollectionConfigurations.count == 1);

  _backgroundCollectionConfiguration =
      backgroundCollectionConfigurations.firstObject;
  _selectedBackgroundId = selectedBackgroundId;

  // Recreate the snapshot with the new items to take into account all the
  // changes of items presence (add/remove).
  NSDiffableDataSourceSnapshot<CustomizationSection*, NSString*>* snapshot =
      [self dataSnapshot];

  // Reconfigure all present items to ensure that they are updated in case their
  // content changed.
  [snapshot
      reconfigureItemsWithIdentifiers:[self identifiersForBackgroundCells]];

  [_diffableDataSource applySnapshot:snapshot animatingDifferences:YES];
}

- (void)currentBackgroundConfigurationChanged:
    (id<BackgroundCustomizationConfiguration>)currentConfiguration {
  NSString* currentItemID = currentConfiguration.configurationID;
  NSIndexPath* currentItemIndexPath =
      [_diffableDataSource indexPathForItemIdentifier:currentItemID];

  [self.collectionView
      selectItemAtIndexPath:currentItemIndexPath
                   animated:NO
             scrollPosition:UICollectionViewScrollPositionNone];

  _selectedBackgroundId = currentItemID;
}

#pragma mark - Helpers

// Returns an array of identifiers for the background options, which can be used
// by the snapshot.
- (NSArray<NSString*>*)identifiersForBackgroundCells {
  NSMutableArray<NSString*>* identifiers = [[NSMutableArray alloc] init];

  NSUInteger indexAfterDefault = 0;

  for (NSString* key in _backgroundCollectionConfiguration.configurationOrder) {
    id<BackgroundCustomizationConfiguration> configuration =
        _backgroundCollectionConfiguration.configurations[key];
    if (!configuration) {
      continue;
    }

    [identifiers addObject:key];

    if (configuration.backgroundStyle ==
        HomeCustomizationBackgroundStyle::kDefault) {
      indexAfterDefault = identifiers.count;
    }
  }

  [identifiers insertObject:kBackgroundPickerCellIdentifier
                    atIndex:indexAfterDefault];

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
  id<BackgroundCustomizationConfiguration> backgroundConfiguration =
      _backgroundCollectionConfiguration.configurations[itemIdentifier];

  CustomUITraitAccessor* traitAccessor =
      [[CustomUITraitAccessor alloc] initWithMutableTraits:cell.traitOverrides];
  [traitAccessor
      setObjectForNewTabPageTrait:backgroundConfiguration.colorPalette];

  BOOL hasBackgroundImage =
      !backgroundConfiguration.thumbnailURL.is_empty() ||
      backgroundConfiguration.userUploadedImagePath.length > 0;
  [traitAccessor setBoolForNewTabPageImageBackgroundTrait:hasBackgroundImage];

  SearchEngineLogoMediator* searchEngineLogoMediator =
      [self.searchEngineLogoMediatorProvider
          provideSearchEngineLogoMediatorForKey:itemIdentifier];

  [cell configureWithBackgroundOption:backgroundConfiguration
             searchEngineLogoMediator:searchEngineLogoMediator];

  if ([itemIdentifier isEqualToString:_selectedBackgroundId]) {
    [self.collectionView
        selectItemAtIndexPath:indexPath
                     animated:NO
               scrollPosition:UICollectionViewScrollPositionNone];
  }
  cell.mutator = self.mutator;
}

// Handles the "Delete Background" context menu action for the given index path.
// This method removes the background from the model (via the mutator) and
// updates the collection view by removing the associated item from the
// diffable data source.
- (void)handleDeleteBackgroundActionAtIndexPath:(NSIndexPath*)indexPath {
  NSString* identifier =
      [self.diffableDataSource itemIdentifierForIndexPath:indexPath];

  if (!identifier) {
    return;
  }

  NSDiffableDataSourceSnapshot* snapshot = [self.diffableDataSource snapshot];
  [self.customizationMutator
      deleteBackgroundFromRecentlyUsed:_backgroundCollectionConfiguration
                                           .configurations[identifier]];
  [snapshot deleteItemsWithIdentifiers:@[ identifier ]];
  [_backgroundCollectionConfiguration.configurations
      removeObjectForKey:identifier];
  [_backgroundCollectionConfiguration.configurationOrder
      removeObject:identifier];
  [self.diffableDataSource applySnapshot:snapshot animatingDifferences:YES];
}

// Handles a loaded user-uploaded image, including optimizations for displaying
// large images in the small menu thumbnails.
//
// TODO(crbug.com/441181385): Improve optimization logic. Some possible options:
// - Cache prepared image so scrolling the carousel doesn't take time to re-load
// image.
// - pre-crop larger images, possibly to a square around the framing
// coordinates, as highly zoomed in images look even worse when made into a low
// resolution thumbnail.
// - pick thumbnail sized based on a combination of view size and frame size.
- (void)handleLoadedUserUploadedImage:(UIImage*)image
                   framingCoordinates:
                       (HomeCustomizationFramingCoordinates*)framingCoordinates
                       backgroundCell:
                           (HomeCustomizationBackgroundCell*)backgroundCell {
  // This thumnail size gives a good balance between quality and responsiveness.
  CGFloat thumbnailDimension =
      3 * MAX(self.view.bounds.size.width, self.view.bounds.size.height);
  CGSize thumbnailSize = CGSize(thumbnailDimension, thumbnailDimension);
  CGFloat originalImageHeight = image.size.height;

  void (^thumbnailHandler)(UIImage*) = ^(UIImage* preparedImage) {
    dispatch_async(dispatch_get_main_queue(), ^{
      // Scale the framing coordinates down to the size of the prepared image.
      CGFloat scale = preparedImage.size.height / originalImageHeight;
      CGRect visibleRect = framingCoordinates.visibleRect;
      CGRect scaledVisibleRect = CGRectMake(
          visibleRect.origin.x * scale, visibleRect.origin.y * scale,
          visibleRect.size.width * scale, visibleRect.size.height * scale);
      HomeCustomizationFramingCoordinates* newFramingCoordinates =
          [[HomeCustomizationFramingCoordinates alloc]
              initWithVisibleRect:scaledVisibleRect];

      [backgroundCell updateBackgroundImage:preparedImage
                         framingCoordinates:newFramingCoordinates];
    });
  };

  [image prepareThumbnailOfSize:thumbnailSize
              completionHandler:thumbnailHandler];
}

@end
