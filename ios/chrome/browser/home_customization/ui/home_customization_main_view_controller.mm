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
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_constants.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message_action.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/custom_ui_trait_accessor.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

// Wrapper class for user-uploaded images.
@interface HomeCustomizationUserUploadedThumbnailData : NSObject
@property(nonatomic, strong) UIImage* preparedImage;
@property(nonatomic, strong)
    HomeCustomizationFramingCoordinates* framingCoordinates;
@end

@implementation HomeCustomizationUserUploadedThumbnailData
@end

@interface HomeCustomizationMainViewController () <
    HomeCustomizationViewControllerProtocol,
    UICollectionViewDelegate>

// Contains the types of HomeCustomizationToggleCells that should be shown, with
// a BOOL indicating if each one is enabled.
@property(nonatomic, assign) std::map<CustomizationToggleType, BOOL> toggleMap;

// Cache for prepared user-uploaded thumbnails.
@property(nonatomic, strong)
    NSCache<NSString*, HomeCustomizationUserUploadedThumbnailData*>*
        userThumbnailCache;

// Contains the identifiers for background image loads that have failed.
@property(nonatomic, strong) NSMutableSet<NSString*>* failedPresetImageLoads;

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

  // The number of times a background is selected from the recently used
  // section.
  int _recentBackgroundClickCount;

  // Last handled height. Used so detents are only invalidated when the content
  // height actually changes.
  CGFloat _lastSeenViewContentHeight;

  // The background cell identifier that should be selected initially. Stored
  // temporarily because the initial data load can happen in multiple different
  // places.
  NSString* _initialSelectedBackgroundID;

  // Cache for loaded preset images.
  NSMutableDictionary<NSString*, UIImage*>* _presetImageCache;
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
    _userThumbnailCache = [[NSCache alloc] init];
    _failedPresetImageLoads = [[NSMutableSet alloc] init];
    _presetImageCache = [[NSMutableDictionary alloc] init];
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
  __weak __typeof(self) weakSelf = self;
  [_diffableDataSource applySnapshot:[self dataSnapshot]
                animatingDifferences:NO
                          completion:^() {
                            [weakSelf selectInitialItemAfterDataLoad];
                          }];

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

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];

  if (_lastSeenViewContentHeight != self.viewContentHeight) {
    _lastSeenViewContentHeight = self.viewContentHeight;
    [self.delegate
        viewContentHeightChangedInHomeCustomizationViewController:self];
  }
}

- (CGFloat)viewContentHeight {
  return self.navigationController.navigationBar.frame.size.height +
         self.collectionView.contentSize.height;
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
  UICollectionViewCell* cell =
      [collectionView cellForItemAtIndexPath:indexPath];

  __weak __typeof(self) weakSelf = self;

  // The currently selected background should have the menu item visible but
  // disabled.
  UIMenuElementAttributes actionAttributes =
      ([collectionView.indexPathsForSelectedItems containsObject:indexPath])
          ? UIMenuElementAttributesDisabled
          : UIMenuElementAttributesDestructive;

  UIActionHandler deleteHandler = ^(UIAction* action) {
    [weakSelf handleDeleteBackgroundActionAtIndexPath:indexPath];
  };

  UIAction* deleteAction = [UIAction
      actionWithTitle:
          l10n_util::GetNSString(
              IDS_IOS_HOME_CUSTOMIZATION_CONTEXT_MENU_DELETE_RECENT_BACKGROUND_TITLE)
                image:DefaultSymbolWithPointSize(
                          kTrashSymbol,
                          [[UIFont
                              preferredFontForTextStyle:UIFontTextStyleBody]
                              pointSize])
           identifier:nil
              handler:^(UIAction* action) {
                [weakSelf handleDeleteBackgroundActionAtIndexPath:indexPath];
              }];
  deleteAction.attributes = actionAttributes;

  UIAccessibilityCustomAction* accessibilityDeleteAction =
      [[UIAccessibilityCustomAction alloc]
           initWithName:deleteAction.title
          actionHandler:^BOOL(UIAccessibilityCustomAction* _customAction) {
            deleteHandler(deleteAction);
            return YES;
          }];

  NSArray<UIAction*>* actions = @[ deleteAction ];
  NSArray<UIAccessibilityCustomAction*>* accessibilityCustomActions =
      @[ accessibilityDeleteAction ];

  cell.accessibilityCustomActions = accessibilityCustomActions;

  return [UIContextMenuConfiguration
      configurationWithIdentifier:indexPath
                  previewProvider:nil
                   actionProvider:^UIMenu*(
                       NSArray<UIMenuElement*>* suggestedActions) {
                     return [UIMenu menuWithTitle:@"" children:actions];
                   }];
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  CustomizationSection* section =
      [self.diffableDataSource snapshot].sectionIdentifiers[indexPath.section];
  NSString* itemIdentifier =
      [self.diffableDataSource itemIdentifierForIndexPath:indexPath];

  // Only the backgrounds section can be selected.
  if (![section isEqualToString:kCustomizationSectionBackground]) {
    return false;
  }

  // If interaction is disabled, prevent selection.
  if (!self.backgroundCustomizationUserInteractionEnabled) {
    return false;
  }

  // The background picker cell cannot be selected. It handles taps itself.
  if ([itemIdentifier isEqualToString:kBackgroundPickerCellIdentifier]) {
    return false;
  }

  // The currently selected item cannot be selected again. This prevents the
  // background when the current background is set again.
  return _collectionView.indexPathsForSelectedItems.firstObject != indexPath;
}

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  NSString* itemIdentifier =
      [self.diffableDataSource itemIdentifierForIndexPath:indexPath];

  id<BackgroundCustomizationConfiguration> backgroundConfiguration =
      _backgroundCollectionConfiguration.configurations[itemIdentifier];

  [self.customizationMutator
      applyBackgroundForConfiguration:backgroundConfiguration];
  // Main menu does not have Cancel/Done buttons, so save the background
  // immediately.
  [self.customizationMutator saveBackground];

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
    [self fetchPresetImageForCell:backgroundCell
                    configuration:backgroundConfiguration
                   itemIdentifier:itemIdentifier];
  } else if (backgroundConfiguration.backgroundStyle ==
             HomeCustomizationBackgroundStyle::kUserUploaded) {
    NSString* imagePath = backgroundConfiguration.userUploadedImagePath;
    HomeCustomizationUserUploadedThumbnailData* cachedData =
        [self.userThumbnailCache objectForKey:imagePath];
    if (cachedData) {
      [backgroundCell updateBackgroundImage:cachedData.preparedImage
                         framingCoordinates:cachedData.framingCoordinates];
      return;
    }

    HomeCustomizationFramingCoordinates* framingCoordinates =
        backgroundConfiguration.userUploadedFramingCoordinates;
    __weak __typeof(self) weakSelf = self;
    void (^imageHandler)(UIImage*, UserUploadedImageError) = ^(
        UIImage* image, UserUploadedImageError error) {
      [weakSelf handleLoadedUserUploadedImage:image
                           framingCoordinates:framingCoordinates
                               backgroundCell:backgroundCell
                                    imagePath:imagePath];
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

  __weak __typeof(self) weakSelf = self;
  [_diffableDataSource applySnapshot:snapshot
                animatingDifferences:YES
                          completion:^() {
                            [weakSelf selectInitialItemAfterDataLoad];
                          }];
}

#pragma mark - HomeCustomizationBackgroundConfigurationConsumer

- (void)setBackgroundCollectionConfigurations:
            (NSArray<BackgroundCollectionConfiguration*>*)
                backgroundCollectionConfigurations
                         selectedBackgroundId:(NSString*)selectedBackgroundId {
  CHECK(backgroundCollectionConfigurations.count == 1);

  _backgroundCollectionConfiguration =
      backgroundCollectionConfigurations.firstObject;

  // Recreate the snapshot with the new items to take into account all the
  // changes of items presence (add/remove).
  NSDiffableDataSourceSnapshot<CustomizationSection*, NSString*>* snapshot =
      [self dataSnapshot];

  // Reconfigure all present items to ensure that they are updated in case their
  // content changed.
  [snapshot
      reconfigureItemsWithIdentifiers:[self identifiersForBackgroundCells]];

  _initialSelectedBackgroundID = selectedBackgroundId;

  __weak __typeof(self) weakSelf = self;
  [_diffableDataSource applySnapshot:snapshot
                animatingDifferences:YES
                          completion:^() {
                            [weakSelf selectInitialItemAfterDataLoad];
                          }];
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

// Helper method to crop a UIImage based on a rectangle in pixel coordinates.
- (UIImage*)cropImage:(UIImage*)originalImage toPixelRect:(CGRect)pixelRect {
  CGImageRef imageRef = originalImage.CGImage;
  if (!imageRef) {
    return nil;
  }

  // Perform the crop. CGImageCreateWithImageInRect handles clipping.
  CGImageRef croppedImageRef =
      CGImageCreateWithImageInRect(imageRef, pixelRect);
  if (!croppedImageRef) {
    return nil;
  }

  UIImage* croppedImage =
      [UIImage imageWithCGImage:croppedImageRef
                          scale:originalImage.scale
                    orientation:originalImage.imageOrientation];
  CGImageRelease(croppedImageRef);

  return croppedImage;
}

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
  cell.mutator = self.mutator;

  UIImage* cachedImage = _presetImageCache[itemIdentifier];
  if (cachedImage) {
    [cell updateBackgroundImage:cachedImage framingCoordinates:nil];
  }
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
- (void)handleLoadedUserUploadedImage:(UIImage*)image
                   framingCoordinates:
                       (HomeCustomizationFramingCoordinates*)framingCoordinates
                       backgroundCell:
                           (HomeCustomizationBackgroundCell*)backgroundCell
                            imagePath:(NSString*)imagePath {
  UIImage* imageToPrepare = image;
  CGRect visibleRect = framingCoordinates.visibleRect;

  // Crop to a square centered on the visible rect to avoid losing data on
  // rotation.
  CGFloat side = MAX(visibleRect.size.width, visibleRect.size.height);
  CGRect squareCropRect =
      CGRectMake(CGRectGetMidX(visibleRect) - side / 2,
                 CGRectGetMidY(visibleRect) - side / 2, side, side);

  // `CGImageCreateWithImageInRect` has undefined behavior if the crop rect
  // extends beyond the image bounds. To be safe, intersect the desired crop
  // rect with the image's bounds.
  CGImageRef cgImage = image.CGImage;
  if (!cgImage) {
    return;
  }
  CGRect imagePixelBounds =
      CGRectMake(0, 0, CGImageGetWidth(cgImage), CGImageGetHeight(cgImage));
  CGRect cropPixelRect = CGRectIntersection(squareCropRect, imagePixelBounds);

  UIImage* croppedImage = [self cropImage:image toPixelRect:cropPixelRect];

  if (croppedImage) {
    // If cropping succeeds, update the image to use and keep visibleRect in
    // sync with the updated image.
    imageToPrepare = croppedImage;
    visibleRect = CGRectMake((visibleRect.origin.x - cropPixelRect.origin.x),
                             (visibleRect.origin.y - cropPixelRect.origin.y),
                             visibleRect.size.width, visibleRect.size.height);
  }

  CGFloat screenScale = [UIScreen mainScreen].scale;
  // Calculate the maximum pixel dimension needed for the cell's display.
  CGFloat thumbnailDimension =
      MAX(backgroundCell.bounds.size.width, backgroundCell.bounds.size.height) *
      screenScale;
  CGSize thumbnailSize = CGSizeMake(thumbnailDimension, thumbnailDimension);

  __weak __typeof(self) weakSelf = self;
  void (^thumbnailHandler)(UIImage*) = ^(UIImage* preparedImage) {
    dispatch_async(dispatch_get_main_queue(), ^{
      if (!preparedImage) {
        return;
      }

      // Calculate the new framing rect for the user's selection within the
      // prepared square thumbnail so the preview accurately represent the
      // user's selection.
      CGFloat scale = preparedImage.size.width / cropPixelRect.size.width;
      CGRect finalVisibleRect = CGRectMake(
          visibleRect.origin.x * scale, visibleRect.origin.y * scale,
          visibleRect.size.width * scale, visibleRect.size.height * scale);

      HomeCustomizationFramingCoordinates* finalFraming =
          [[HomeCustomizationFramingCoordinates alloc]
              initWithVisibleRect:finalVisibleRect];

      // Cache the prepared thumbnail and its framing.
      HomeCustomizationUserUploadedThumbnailData* cachedData =
          [[HomeCustomizationUserUploadedThumbnailData alloc] init];
      cachedData.preparedImage = preparedImage;
      cachedData.framingCoordinates = finalFraming;
      [weakSelf.userThumbnailCache setObject:cachedData forKey:imagePath];

      // Update the cell with the thumbnail.
      [backgroundCell updateBackgroundImage:preparedImage
                         framingCoordinates:finalFraming];
    });
  };

  [imageToPrepare prepareThumbnailOfSize:thumbnailSize
                       completionHandler:thumbnailHandler];
}

// Selects the initially selected item when new data is loaded.
- (void)selectInitialItemAfterDataLoad {
  if (!_initialSelectedBackgroundID) {
    return;
  }

  NSIndexPath* indexPath = [_diffableDataSource
      indexPathForItemIdentifier:_initialSelectedBackgroundID];
  _initialSelectedBackgroundID = nil;
  [self.collectionView
      selectItemAtIndexPath:indexPath
                   animated:NO
             scrollPosition:UICollectionViewScrollPositionNone];
}

- (void)presentImageLoadFailSnackbarWithRetryBlock:(ProceduralBlock)retryBlock {
  SnackbarMessage* message = [[SnackbarMessage alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_LOAD_FAIL)];
  message.duration = DBL_MAX;

  SnackbarMessageAction* action = [[SnackbarMessageAction alloc] init];
  action.title = l10n_util::GetNSString(
      IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_LOAD_TRY_AGAIN);
  action.handler = ^{
    if (retryBlock) {
      // Dispatching to the main queue with a delay to allow for the snackbar to
      // be dismissed before retrying the image load.
      dispatch_after(
          dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.0 * NSEC_PER_SEC)),
          dispatch_get_main_queue(), ^{
            retryBlock();
          });
    }
  };
  message.action = action;
  dispatch_async(dispatch_get_main_queue(), ^{
    [self.snackbarCommandHandler showSnackbarMessage:message];
  });
}

#pragma mark - Private

// Retries fetching preset images for all backgrounds that previously failed to
// load.
- (void)retryFailedImageLoads {
  NSSet<NSString*>* identifiersToRetry = [self.failedPresetImageLoads copy];
  NSDiffableDataSourceSnapshot* snapshot = [self.diffableDataSource snapshot];
  for (NSString* identifier in identifiersToRetry) {
    [snapshot reloadItemsWithIdentifiers:@[ identifier ]];
  }
  [self.diffableDataSource applySnapshot:snapshot animatingDifferences:NO];
}

// Handles the completion of a preset image fetch request. If the fetch was
// successful, it updates the cell's background. If it failed, it presents a
// snackbar with a retry option.
- (void)onFetchPresetImageCompleteWithImage:(UIImage*)image
                                      error:(NSError*)error
                             itemIdentifier:(NSString*)itemIdentifier {
  if (error) {
    [self.failedPresetImageLoads addObject:itemIdentifier];
    __weak __typeof(self) weakSelf = self;
    [self presentImageLoadFailSnackbarWithRetryBlock:^{
      [weakSelf retryFailedImageLoads];
    }];
    return;
  }

  _presetImageCache[itemIdentifier] = image;

  if ([self.failedPresetImageLoads containsObject:itemIdentifier]) {
    [self.failedPresetImageLoads removeObject:itemIdentifier];
    if (self.failedPresetImageLoads.count == 0) {
      [self.snackbarCommandHandler dismissAllSnackbars];
    }
  }

  // Reconfigure the item to apply the new image and ensure the rest of the cell
  // is loaded.
  NSDiffableDataSourceSnapshot* snapshot = [self.diffableDataSource snapshot];
  if ([[snapshot itemIdentifiers] containsObject:itemIdentifier]) {
    [snapshot reconfigureItemsWithIdentifiers:@[ itemIdentifier ]];
    [self.diffableDataSource applySnapshot:snapshot animatingDifferences:YES];
  }
}

// Fetches the preset image for a given cell and configuration.
- (void)fetchPresetImageForCell:(HomeCustomizationBackgroundCell*)cell
                  configuration:
                      (id<BackgroundCustomizationConfiguration>)configuration
                 itemIdentifier:(NSString*)itemIdentifier {
  if (_presetImageCache[itemIdentifier]) {
    return;
  }
  __weak __typeof(self) weakSelf = self;

  void (^imageHandler)(UIImage*, NSError*) = ^(UIImage* image, NSError* error) {
    [weakSelf onFetchPresetImageCompleteWithImage:image
                                            error:error
                                   itemIdentifier:itemIdentifier];
  };

  [self.customizationMutator
      fetchBackgroundCustomizationThumbnailURLImage:configuration.thumbnailURL
                                         completion:imageHandler];
}

@end
