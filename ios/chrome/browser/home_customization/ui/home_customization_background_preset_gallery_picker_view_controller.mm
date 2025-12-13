// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_background_preset_gallery_picker_view_controller.h"

#import <Foundation/Foundation.h>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/home_customization/ui/background_collection_configuration.h"
#import "ios/chrome/browser/home_customization/ui/background_customization_configuration.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_accessibility_identifiers.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_cell.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_configuration_mutator.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_picker_presentation_delegate.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_preset_header_view.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_skeleton_cell.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_collection_configurator.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_header_view.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_search_engine_logo_mediator_provider.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_view_controller_protocol.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_image_background_trait.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/custom_ui_trait_accessor.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

// Define constants within the namespace
namespace {
// The left and right padding for the header in the collection view.
const CGFloat kHeaderInsetSides = 7.5;

// The number of skeleton sections to display while content is loading.
const NSInteger kSkeletonSectionCount = 3;

// The number of skeleton items to show in each section during loading.
const NSInteger kSkeletonItemsPerSection = 4;

// The time interval between loading animation updates, in seconds.
const NSTimeInterval kAnimationIntervalSeconds = 0.5;
}  // namespace

@interface HomeCustomizationBackgroundPresetGalleryPickerViewController () <
    HomeCustomizationViewControllerProtocol,
    UICollectionViewDataSourcePrefetching> {
  // The configurator for the collection view.
  HomeCustomizationCollectionConfigurator* _collectionConfigurator;

  // Registration for the background cell.
  UICollectionViewCellRegistration* _backgroundCellRegistration;

  // Registration for the background skeleton cell.
  UICollectionViewCellRegistration* _backgroundSkeletonCellRegistration;

  // Registration for the collection's header.
  UICollectionViewSupplementaryRegistration* _headerRegistration;

  // A flat map of background customization options, keyed by background ID.
  // Used by HomeCustomizationBackgroundCell to apply backgrounds on the NTP.
  NSMutableDictionary<NSString*, id<BackgroundCustomizationConfiguration>>*
      _backgroundCustomizationConfigurationMap;

  // A list of background customization configurations grouped by section,
  // each associated with a collection name.
  NSArray<BackgroundCollectionConfiguration*>*
      _backgroundCollectionConfigurations;

  // Timer used to periodically trigger the loading animation update.
  NSTimer* _loadingTimer;

  // The current index of the cell being dimmed in the loading animation.
  NSInteger _skeletonAnimationIndex;

  // Tracking for maximum visible indices
  NSInteger _maxVisibleSectionIndex;
  NSInteger _maxVisibleItemIndex;

  // The number of times an item from the gallery is selected.
  int _galleryClickCount;

  // Cache for prepared thumbnail images. The key is the configuration ID.
  NSCache<NSString*, UIImage*>* _preparedImageCache;

  // Set of in progress image fetches, keyed by configuration ID.
  NSMutableSet<NSString*>* _inProgressImageFetches;
}

@end

@implementation HomeCustomizationBackgroundPresetGalleryPickerViewController

// Synthesized from HomeCustomizationViewControllerProtocol.
@synthesize collectionView = _collectionView;
@synthesize diffableDataSource = _diffableDataSource;
@synthesize page = _page;
@synthesize additionalViewWillTransitionToSizeHandler =
    _additionalViewWillTransitionToSizeHandler;

- (void)viewDidLoad {
  [super viewDidLoad];

  _preparedImageCache = [[NSCache alloc] init];
  _inProgressImageFetches = [[NSMutableSet alloc] init];

  __weak __typeof(self) weakSelf = self;

  _collectionConfigurator = [[HomeCustomizationCollectionConfigurator alloc]
      initWithViewController:self];

  self.title = l10n_util::GetNSStringWithFixup(
      IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_PICKER_PRESET_GALLERY_TITLE);

  if (@available(iOS 26, *)) {
    self.view.backgroundColor = UIColor.clearColor;
  } else {
    self.view.backgroundColor =
        [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  }

  UICollectionViewCompositionalLayout* layout =
      [[UICollectionViewCompositionalLayout alloc]
          initWithSectionProvider:^NSCollectionLayoutSection*(
              NSInteger sectionIndex,
              id<NSCollectionLayoutEnvironment> layoutEnvironment) {
            return
                [weakSelf createSectionLayoutWithEnvironment:layoutEnvironment];
          }];

  [self createRegistrations];

  _collectionView = [[UICollectionView alloc] initWithFrame:CGRectZero
                                       collectionViewLayout:layout];
  _collectionView.delegate = self;
  _collectionView.accessibilityIdentifier =
      kHomeCustomizationGalleryPickerViewAccessibilityIdentifier;
  _collectionView.prefetchDataSource = self;
  _collectionView.backgroundColor = UIColor.clearColor;

  _diffableDataSource = [[UICollectionViewDiffableDataSource alloc]
      initWithCollectionView:_collectionView
                cellProvider:^UICollectionViewCell*(
                    UICollectionView* collectionView, NSIndexPath* indexPath,
                    NSString* itemIdentifier) {
                  return [weakSelf configuredCellForIndexPath:indexPath
                                               itemIdentifier:itemIdentifier];
                }];

  _diffableDataSource.supplementaryViewProvider =
      ^UICollectionReusableView*(UICollectionView* collectionView,
                                 NSString* kind, NSIndexPath* indexPath) {
        return [weakSelf configuredHeaderForIndexPath:indexPath];
      };

  NSDiffableDataSourceSnapshot<CustomizationSection*, NSString*>*
      initialSnapshot =
          (_backgroundCollectionConfigurations) ? [self dataSnapshot]
                                                : [self skeletonSnapshot];
  [_diffableDataSource applySnapshot:initialSnapshot animatingDifferences:NO];

  _loadingTimer =
      [NSTimer scheduledTimerWithTimeInterval:(kAnimationIntervalSeconds)
                                       target:self
                                     selector:@selector(updateLoadingAnimation)
                                     userInfo:nil
                                      repeats:YES];

  _collectionView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_collectionView];

  AddSameConstraints(_collectionView, self.view);
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  // Some of the layout sections care about rotation/size changes, so invalidate
  // the layout so those sections can be updated.
  [_collectionView.collectionViewLayout invalidateLayout];
  if (_additionalViewWillTransitionToSizeHandler) {
    _additionalViewWillTransitionToSizeHandler(size, coordinator);
  }
}

- (void)viewWillDisappear:(BOOL)animated {
  // Log final maximums before disappearing, for example.
  base::UmaHistogramSparse(
      "IOS.HomeCustomization.Background.Gallery.MaxVisibleSectionIndex",
      _maxVisibleSectionIndex);
  base::UmaHistogramSparse(
      "IOS.HomeCustomization.Background.Gallery.MaxVisibleItemIndex",
      _maxVisibleItemIndex);
  // Log the total number of selection changes while the gallery was open.
  base::UmaHistogramCounts10000(
      "IOS.HomeCustomization.Background.Gallery.ClickCount",
      _galleryClickCount);
  [self stopLoadingAnimation];
}

- (NSInteger)selectedSectionIndex {
  return _collectionView.indexPathsForSelectedItems.firstObject.section;
}

#pragma mark - HomeCustomizationBackgroundConfigurationConsumer

- (void)setBackgroundCollectionConfigurations:
            (NSArray<BackgroundCollectionConfiguration*>*)
                backgroundCollectionConfigurations
                         selectedBackgroundId:(NSString*)selectedBackgroundId {
  [self stopLoadingAnimation];
  NSMutableDictionary<NSString*, id<BackgroundCustomizationConfiguration>>*
      backgroundCustomizationConfigurationMap =
          [NSMutableDictionary dictionary];

  // Flattens all background configurations from the collections into a single
  // map.
  for (BackgroundCollectionConfiguration* backgroundCollectionConfiguration in
           backgroundCollectionConfigurations) {
    for (NSString* configurationID in backgroundCollectionConfiguration
             .configurations) {
      id<BackgroundCustomizationConfiguration> backgroundConfiguration =
          [backgroundCollectionConfiguration.configurations
              objectForKey:configurationID];
      [backgroundCustomizationConfigurationMap setObject:backgroundConfiguration
                                                  forKey:configurationID];
    }
  }

  _backgroundCustomizationConfigurationMap =
      backgroundCustomizationConfigurationMap;
  _backgroundCollectionConfigurations = backgroundCollectionConfigurations;
  __weak __typeof(self) weakSelf = self;
  [_diffableDataSource
             applySnapshot:[self dataSnapshot]
      animatingDifferences:NO
                completion:^() {
                  [weakSelf selectItemWithIdentifier:selectedBackgroundId];
                }];
}

- (void)selectItemWithIdentifier:(NSString*)itemIdentifier {
  NSIndexPath* indexPath =
      [_diffableDataSource indexPathForItemIdentifier:itemIdentifier];
  [self.collectionView
      selectItemAtIndexPath:indexPath
                   animated:NO
             scrollPosition:UICollectionViewScrollPositionNone];
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

  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(cancelButtonPressed)];
  cancelButton.accessibilityIdentifier =
      kPickerViewCancelButtonAccessibilityIdentifier;

  UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(donebuttonPressed)];
  doneButton.accessibilityIdentifier =
      kPickerViewDoneButtonAccessibilityIdentifier;

  self.navigationItem.leftBarButtonItem = cancelButton;
  self.navigationItem.rightBarButtonItem = doneButton;
}

#pragma mark - UICollectionViewDelegate

// Returns a configured header for the given path in the collection view.
- (UICollectionViewCell*)configuredHeaderForIndexPath:(NSIndexPath*)indexPath {
  return [_collectionView
      dequeueConfiguredReusableSupplementaryViewWithRegistration:
          _headerRegistration
                                                    forIndexPath:indexPath];
}

// Returns a configured cell for the given index path and item identifier.
- (UICollectionViewCell*)configuredCellForIndexPath:(NSIndexPath*)indexPath
                                     itemIdentifier:(NSString*)itemIdentifier {
  if (_backgroundCollectionConfigurations) {
    return [_collectionView
        dequeueConfiguredReusableCellWithRegistration:
            _backgroundCellRegistration
                                         forIndexPath:indexPath
                                                 item:itemIdentifier];
  }
  return [_collectionView
      dequeueConfiguredReusableCellWithRegistration:
          _backgroundSkeletonCellRegistration
                                       forIndexPath:indexPath
                                               item:itemIdentifier];
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  // Before the data has loaded (i.e. when there are skeleton loaders), prevent
  // selection.
  if (!_backgroundCollectionConfigurations) {
    return false;
  }

  // The currently selected item cannot be selected again. This prevents the
  // background when the current background is set again.
  return _collectionView.indexPathsForSelectedItems.firstObject != indexPath;
}

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  NSString* itemIdentifier =
      [_diffableDataSource itemIdentifierForIndexPath:indexPath];

  [self.mutator applyBackgroundForConfiguration:
                    _backgroundCustomizationConfigurationMap[itemIdentifier]];
  _galleryClickCount += 1;
}

- (void)collectionView:(UICollectionView*)collectionView
       willDisplayCell:(HomeCustomizationBackgroundCell*)cell
    forItemAtIndexPath:(NSIndexPath*)indexPath {
  // Update the maximum visible section index.
  _maxVisibleSectionIndex =
      std::max(_maxVisibleSectionIndex, indexPath.section);

  // Update the maximum visible item index.
  _maxVisibleItemIndex = std::max(_maxVisibleItemIndex, indexPath.item);

  NSString* itemIdentifier =
      [_diffableDataSource itemIdentifierForIndexPath:indexPath];

  // If image is already cached, use that.
  UIImage* cachedImage = [_preparedImageCache objectForKey:itemIdentifier];
  if (cachedImage) {
    [cell updateBackgroundImage:cachedImage framingCoordinates:nil];
    return;
  }

  // If image fetch is in progress, just wait for that to complete.
  if ([_inProgressImageFetches containsObject:itemIdentifier]) {
    return;
  }

  // Otherwise, start a new fetch.
  [self fetchThumbnailForItemIdentifier:itemIdentifier];
}

#pragma mark - UICollectionViewDataSourcePrefetching

- (void)collectionView:(UICollectionView*)collectionView
    prefetchItemsAtIndexPaths:(NSArray<NSIndexPath*>*)indexPaths {
  for (NSIndexPath* indexPath : indexPaths) {
    NSString* itemIdentifier =
        [_diffableDataSource itemIdentifierForIndexPath:indexPath];
    // No need to prefetch if this item has already been fetched or is in
    // progress.
    if ([_inProgressImageFetches containsObject:itemIdentifier] ||
        [_preparedImageCache objectForKey:itemIdentifier]) {
      continue;
    }
    [self fetchThumbnailForItemIdentifier:itemIdentifier];
  }
}

- (void)collectionView:(UICollectionView*)collectionView
    cancelPrefetchingForItemsAtIndexPaths:(NSArray<NSIndexPath*>*)indexPaths {
  // Do nothing. Image fetcher requests currently can't be cancelled.
}

#pragma mark - HomeCustomizationViewControllerProtocol

- (NSCollectionLayoutSection*)
      sectionForIndex:(NSInteger)sectionIndex
    layoutEnvironment:(id<NSCollectionLayoutEnvironment>)layoutEnvironment {
  CGSize windowSize = self.view.window.bounds.size;
  return [_collectionConfigurator
      backgroundCellSectionForLayoutEnvironment:layoutEnvironment
                                     windowSize:windowSize];
}

#pragma mark - Private

// Starts an image fetch for the item with the given identifier.
- (void)fetchThumbnailForItemIdentifier:(NSString*)itemIdentifier {
  id<BackgroundCustomizationConfiguration> backgroundConfiguration =
      _backgroundCustomizationConfigurationMap[itemIdentifier];

  __weak __typeof(self) weakSelf = self;

  if (!backgroundConfiguration ||
      backgroundConfiguration.thumbnailURL.is_empty()) {
    return;
  }

  auto completion = ^(UIImage* image, NSError* error) {
    [weakSelf handleThumbnailFetchForItemIdentifier:itemIdentifier
                                              image:image
                                              error:error];
  };
  [_inProgressImageFetches addObject:itemIdentifier];
  [self.mutator
      fetchBackgroundCustomizationThumbnailURLImage:backgroundConfiguration
                                                        .thumbnailURL
                                         completion:completion];
}

// Handles a finished image fetch for the given image identifier.
- (void)handleThumbnailFetchForItemIdentifier:(NSString*)itemIdentifier
                                        image:(UIImage*)image
                                        error:(NSError*)error {
  NSIndexPath* indexPath =
      [_diffableDataSource indexPathForItemIdentifier:itemIdentifier];
  if (error) {
    // Delete the cell if the thumbnail image failed to load.
    [self deleteBackgroundCell:itemIdentifier forItemAtIndexPath:indexPath];
    [_inProgressImageFetches removeObject:itemIdentifier];
    return;
  }

  __weak __typeof(self) weakSelf = self;
  [image prepareForDisplayWithCompletionHandler:^(UIImage*) {
    [weakSelf imagePreparedForDisplay:image itemIdentifier:itemIdentifier];
  }];
}

// Handles an image with the given item identifier being prepared for display.
- (void)imagePreparedForDisplay:(UIImage*)image
                 itemIdentifier:(NSString*)itemIdentifier {
  [_inProgressImageFetches removeObject:itemIdentifier];
  if (!image) {
    return;
  }

  [_preparedImageCache setObject:image forKey:itemIdentifier];

  __weak __typeof(self) weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf setImage:image forCellWithItemIdentifier:itemIdentifier];
  });
}

// Updates the cell for the given identifier to the provided image.
- (void)setImage:(UIImage*)image
    forCellWithItemIdentifier:(NSString*)itemIdentifier {
  // If cell is visible, set its image.
  NSIndexPath* indexPath =
      [_diffableDataSource indexPathForItemIdentifier:itemIdentifier];
  UICollectionViewCell* cell =
      [_collectionView cellForItemAtIndexPath:indexPath];
  HomeCustomizationBackgroundCell* backgroundCell =
      base::apple::ObjCCast<HomeCustomizationBackgroundCell>(cell);
  // Cell may have moved off screen/been reused.
  if (!backgroundCell) {
    return;
  }

  [backgroundCell updateBackgroundImage:image framingCoordinates:nil];
}

// Removes a background cell for the given configurationID.
- (void)deleteBackgroundCell:(NSString*)configurationID
          forItemAtIndexPath:(NSIndexPath*)indexPath {
  [_backgroundCustomizationConfigurationMap removeObjectForKey:configurationID];

  BackgroundCollectionConfiguration* backgroundCollectionConfiguration =
      _backgroundCollectionConfigurations[indexPath.section];
  if (backgroundCollectionConfiguration) {
    [backgroundCollectionConfiguration.configurations
        removeObjectForKey:configurationID];

    NSUInteger indexOfConfigurationOrder =
        [backgroundCollectionConfiguration.configurationOrder
            indexOfObjectPassingTest:^BOOL(NSString* id, NSUInteger index,
                                           BOOL* stop) {
              return configurationID == id;
            }];
    if (indexOfConfigurationOrder != NSNotFound) {
      [backgroundCollectionConfiguration.configurationOrder
          removeObjectAtIndex:indexOfConfigurationOrder];
    }
  }

  NSDiffableDataSourceSnapshot<CustomizationSection*, NSString*>* snapshot =
      [_diffableDataSource snapshot];
  [snapshot deleteItemsWithIdentifiers:@[ configurationID ]];
  [_diffableDataSource applySnapshot:snapshot animatingDifferences:NO];
}

// Creates a skeleton snapshot representing the loading content of the
// collection view.
- (NSDiffableDataSourceSnapshot<CustomizationSection*, NSString*>*)
    skeletonSnapshot {
  NSDiffableDataSourceSnapshot<NSString*, NSString*>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];

  for (NSInteger row = 0; row < kSkeletonSectionCount; row++) {
    NSString* sectionId =
        [NSString stringWithFormat:@"%@_%ld", kBackgroundCellIdentifier, row];
    [snapshot appendSectionsWithIdentifiers:@[ sectionId ]];

    NSMutableArray* skeletonIds = [NSMutableArray array];

    for (NSInteger col = 0; col < kSkeletonItemsPerSection; col++) {
      NSString* id = [NSString
          stringWithFormat:@"%@_%ld_%ld", kBackgroundCellIdentifier, row, col];
      [skeletonIds addObject:id];
    }

    [snapshot appendItemsWithIdentifiers:skeletonIds
               intoSectionWithIdentifier:sectionId];
  }

  return snapshot;
}

// Creates a data snapshot representing the content of the collection view.
- (NSDiffableDataSourceSnapshot<CustomizationSection*, NSString*>*)
    dataSnapshot {
  NSDiffableDataSourceSnapshot<NSString*, NSString*>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];

  for (BackgroundCollectionConfiguration* backgroundCollectionConfiguration in
           _backgroundCollectionConfigurations) {
    [snapshot appendSectionsWithIdentifiers:@[
      backgroundCollectionConfiguration.collectionName
    ]];
    NSMutableArray* backgroundIds = [NSMutableArray array];
    for (NSString* configurationID in backgroundCollectionConfiguration
             .configurationOrder) {
      [backgroundIds addObject:configurationID];
    }

    [snapshot appendItemsWithIdentifiers:backgroundIds
               intoSectionWithIdentifier:backgroundCollectionConfiguration
                                             .collectionName];
  }

  return snapshot;
}

// Creates and configures the section layout using the given layout environment.
- (NSCollectionLayoutSection*)createSectionLayoutWithEnvironment:
    (id<NSCollectionLayoutEnvironment>)layoutEnvironment {
  CGSize windowSize = self.view.window.bounds.size;
  NSCollectionLayoutSection* section = [_collectionConfigurator
      backgroundCellSectionForLayoutEnvironment:layoutEnvironment
                                     windowSize:windowSize];

  // Header.
  NSCollectionLayoutSize* headerSize = [NSCollectionLayoutSize
      sizeWithWidthDimension:[NSCollectionLayoutDimension
                                 fractionalWidthDimension:1.0]
             heightDimension:[NSCollectionLayoutDimension
                                 estimatedDimension:0.0]];
  NSCollectionLayoutBoundarySupplementaryItem* headerItem =
      [NSCollectionLayoutBoundarySupplementaryItem
          boundarySupplementaryItemWithLayoutSize:headerSize
                                      elementKind:
                                          UICollectionElementKindSectionHeader
                                        alignment:NSRectAlignmentTop];
  headerItem.contentInsets =
      NSDirectionalEdgeInsetsMake(0, kHeaderInsetSides, 0, kHeaderInsetSides);

  section.boundarySupplementaryItems = @[ headerItem ];

  return section;
}

// Configuration handler for the header view.
- (void)configureHeaderView:(HomeCustomizationBackgroundPresetHeaderView*)header
                elementKind:(NSString*)elementKind
                  indexPath:(NSIndexPath*)indexPath {
  if (!_backgroundCollectionConfigurations) {
    return;
  }

  NSString* collectionName =
      [_diffableDataSource snapshot].sectionIdentifiers[indexPath.section];

  [header setText:collectionName];
}

// Creates the registrations for the collection view.
- (void)createRegistrations {
  __weak __typeof(self) weakSelf = self;

  _backgroundCellRegistration = [UICollectionViewCellRegistration
      registrationWithCellClass:[HomeCustomizationBackgroundCell class]
           configurationHandler:^(HomeCustomizationBackgroundCell* cell,
                                  NSIndexPath* indexPath,
                                  NSString* itemIdentifier) {
             [weakSelf configureBackgroundCell:cell
                                   atIndexPath:indexPath
                            withItemIdentifier:itemIdentifier];
           }];

  _backgroundSkeletonCellRegistration = [UICollectionViewCellRegistration
      registrationWithCellClass:[HomeCustomizationBackgroundSkeletonCell class]
           configurationHandler:^(HomeCustomizationBackgroundSkeletonCell* cell,
                                  NSIndexPath* indexPath,
                                  NSString* itemIdentifier){
           }];

  _headerRegistration = [UICollectionViewSupplementaryRegistration
      registrationWithSupplementaryClass:
          [HomeCustomizationBackgroundPresetHeaderView class]
                             elementKind:UICollectionElementKindSectionHeader
                    configurationHandler:^(
                        HomeCustomizationBackgroundPresetHeaderView* header,
                        NSString* elementKind, NSIndexPath* indexPath) {
                      [weakSelf configureHeaderView:header
                                        elementKind:elementKind
                                          indexPath:indexPath];
                    }];
}

// Configures a `HomeCustomizationBackgroundCell` with the provided background
// configuration and logo view, and selects it if it matches the currently
// selected background ID.
- (void)configureBackgroundCell:(HomeCustomizationBackgroundCell*)cell
                    atIndexPath:(NSIndexPath*)indexPath
             withItemIdentifier:(NSString*)itemIdentifier {
  if (!_backgroundCustomizationConfigurationMap) {
    return;
  }
  id<BackgroundCustomizationConfiguration> backgroundConfiguration =
      _backgroundCustomizationConfigurationMap[itemIdentifier];
  SearchEngineLogoMediator* searchEngineLogoMediator =
      [self.searchEngineLogoMediatorProvider
          provideSearchEngineLogoMediatorForKey:itemIdentifier];

  CustomUITraitAccessor* traitAccessor =
      [[CustomUITraitAccessor alloc] initWithMutableTraits:cell.traitOverrides];
  [traitAccessor setBoolForNewTabPageImageBackgroundTrait:YES];

  [cell configureWithBackgroundOption:backgroundConfiguration
             searchEngineLogoMediator:searchEngineLogoMediator];
}

// Dismisses the current customization menu page.
- (void)dismissCustomizationMenuPage {
  [self dismissViewControllerAnimated:YES completion:nil];
}

// This method simulates a loading shimmer effect by dimming one cell at a time.
// Only one cell per section is dimmed at any given moment.
- (void)updateLoadingAnimation {
  NSInteger previousIndex =
      (_skeletonAnimationIndex - 1 + kSkeletonItemsPerSection) %
      kSkeletonItemsPerSection;

  for (NSInteger section = 0; section < kSkeletonSectionCount; section++) {
    NSIndexPath* previousIndexPath = [NSIndexPath indexPathForItem:previousIndex
                                                         inSection:section];
    NSIndexPath* currentIndexPath =
        [NSIndexPath indexPathForItem:_skeletonAnimationIndex
                            inSection:section];

    UICollectionViewCell* previousCell =
        [self.collectionView cellForItemAtIndexPath:previousIndexPath];
    UICollectionViewCell* currentCell =
        [self.collectionView cellForItemAtIndexPath:currentIndexPath];

    previousCell.alpha = 1;
    currentCell.alpha = 0.5;
  }

  _skeletonAnimationIndex =
      (_skeletonAnimationIndex + 1) % kSkeletonItemsPerSection;
}

// Stops the loading animation by invalidating the timer and resetting the alpha
// of currently dimmed cells back to fully opaque.
- (void)stopLoadingAnimation {
  if (!_loadingTimer) {
    return;
  }

  [_loadingTimer invalidate];
  _loadingTimer = nil;

  for (NSInteger section = 0; section < kSkeletonSectionCount; section++) {
    NSIndexPath* indexPath =
        [NSIndexPath indexPathForItem:_skeletonAnimationIndex
                            inSection:section];
    UICollectionViewCell* cell =
        [self.collectionView cellForItemAtIndexPath:indexPath];
    cell.alpha = 1.0;
  }
}

// Cancels any unsaved changes and dismisses the menu.
- (void)cancelButtonPressed {
  [self.mutator discardBackground];
  [self.presentationDelegate cancelBackgroundPicker];
}

// Dismiss the menu. The current background will be saved on menu dismiss.
- (void)donebuttonPressed {
  [self.presentationDelegate dismissBackgroundPicker];
}

@end
