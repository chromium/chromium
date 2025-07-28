// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_background_preset_gallery_picker_view_controller.h"

#import <Foundation/Foundation.h>

#import "base/check.h"
#import "ios/chrome/browser/home_customization/model/background_collection_configuration.h"
#import "ios/chrome/browser/home_customization/model/background_customization_configuration.h"
#import "ios/chrome/browser/home_customization/model/background_customization_configuration_item.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_cell.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_picker_action_sheet_presentation_delegate.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_preset_gallery_picker_mutator.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_preset_header_view.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_skeleton_cell.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_collection_configurator.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_header_view.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_search_engine_logo_mediator_provider.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_view_controller_protocol.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
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
    HomeCustomizationViewControllerProtocol> {
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

  // The id of the selected background cell.
  NSString* _selectedBackgroundId;

  // Timer used to periodically trigger the loading animation update.
  NSTimer* _loadingTimer;

  // The current index of the cell being dimmed in the loading animation.
  NSInteger _skeletonAnimationIndex;
}
@end

@implementation HomeCustomizationBackgroundPresetGalleryPickerViewController

// Synthesized from HomeCustomizationViewControllerProtocol.
@synthesize collectionView = _collectionView;
@synthesize diffableDataSource = _diffableDataSource;
@synthesize page = _page;

- (void)viewDidLoad {
  [super viewDidLoad];

  __weak __typeof(self) weakSelf = self;

  _collectionConfigurator = [[HomeCustomizationCollectionConfigurator alloc]
      initWithViewController:self];

  self.title = l10n_util::GetNSStringWithFixup(
      IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_PICKER_PRESET_GALLERY_TITLE);

  self.view.backgroundColor = [UIColor systemBackgroundColor];

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

  [_diffableDataSource applySnapshot:[self skeletonSnapshot]
                animatingDifferences:NO];

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

#pragma mark - HomeCustomizationBackgroundPresetGalleryPickerConsumer

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
  for (BackgroundCollectionConfiguration* BackgroundCollectionConfiguration in
           backgroundCollectionConfigurations) {
    for (id<BackgroundCustomizationConfiguration> backgroundConfiguration in
             BackgroundCollectionConfiguration.configurations) {
      [backgroundCustomizationConfigurationMap
          setObject:backgroundConfiguration
             forKey:backgroundConfiguration.configurationID];
    }
  }

  _selectedBackgroundId = selectedBackgroundId;
  _backgroundCustomizationConfigurationMap =
      backgroundCustomizationConfigurationMap;
  _backgroundCollectionConfigurations = backgroundCollectionConfigurations;
  [_diffableDataSource applySnapshot:[self dataSnapshot]
                animatingDifferences:NO];
}

- (void)viewWillDisappear:(BOOL)animated {
  [self stopLoadingAnimation];
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
  return _backgroundCollectionConfigurations;
}

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  NSString* itemIdentifier =
      [_diffableDataSource itemIdentifierForIndexPath:indexPath];
  [self.presentationDelegate
      applyBackgroundForConfiguration:_backgroundCustomizationConfigurationMap
                                          [itemIdentifier]];
}

- (void)collectionView:(UICollectionView*)collectionView
       willDisplayCell:(HomeCustomizationBackgroundCell*)cell
    forItemAtIndexPath:(NSIndexPath*)indexPath {
  NSString* itemIdentifier =
      [_diffableDataSource itemIdentifierForIndexPath:indexPath];
  id<BackgroundCustomizationConfiguration> backgroundConfiguration =
      _backgroundCustomizationConfigurationMap[itemIdentifier];

  if (backgroundConfiguration &&
      !backgroundConfiguration.thumbnailURL.is_empty()) {
    [self.mutator
        fetchBackgroundCustomizationThumbnailURLImage:backgroundConfiguration
                                                          .thumbnailURL
                                           completion:^(UIImage* image) {
                                             [cell updateBackgroundImage:image];
                                           }];
  }
}

#pragma mark - HomeCustomizationViewControllerProtocol

- (NSCollectionLayoutSection*)
      sectionForIndex:(NSInteger)sectionIndex
    layoutEnvironment:(id<NSCollectionLayoutEnvironment>)layoutEnvironment {
  return [_collectionConfigurator
      backgroundCellSectionForLayoutEnvironment:layoutEnvironment];
}

#pragma mark - Private

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
    for (id<BackgroundCustomizationConfiguration> backgroundConfiguration in
             backgroundCollectionConfiguration.configurations) {
      [backgroundIds addObject:backgroundConfiguration.configurationID];
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
  NSCollectionLayoutSection* section = [_collectionConfigurator
      backgroundCellSectionForLayoutEnvironment:layoutEnvironment];

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
      [self.searchEngineLogoMediatorProvider provideSearchEngineLogoMediator];

  [cell configureWithBackgroundOption:backgroundConfiguration
             searchEngineLogoMediator:searchEngineLogoMediator
                         colorPalette:nil];

  if ([itemIdentifier isEqualToString:_selectedBackgroundId]) {
    [_collectionView selectItemAtIndexPath:indexPath
                                  animated:NO
                            scrollPosition:UICollectionViewScrollPositionNone];
  }
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

@end
